#!/usr/bin/env python3
"""Exercise real loopback WS/WSS and telnet sessions in supervised driver sandboxes."""

from __future__ import annotations

import argparse
import base64
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import socket
import ssl
import struct
import subprocess
import sys
import threading
import time
import unittest
import zlib

spec = importlib.util.spec_from_file_location(
    "websocket_runner", Path(__file__).with_name("run-targeted.py")
)
runner = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = runner
spec.loader.exec_module(runner)

BURST = b"B" + "ab中🙂".encode() * 8193 + b"Z\n"
CASES = {
    "u11-interactive-efuns": ("u11-interactive", "plain", "telnet"),
    **{f"contract-{transport}-{protocol}": ("contract", transport, protocol)
       for transport in ("ws", "wss") for protocol in ("ascii", "telnet")},
    **{f"contract-{transport}": ("contract", transport, "telnet")
       for transport in ("plain", "tls")},
    **{f"callback-destruct-{transport}": ("callback-destruct", transport, "telnet")
       for transport in ("ws", "wss", "plain", "tls")},
    **{f"{policy}-{transport}-{protocol}": (policy, transport, protocol)
       for policy in ("untrusted", "trusted") for transport in ("ws", "wss")
       for protocol in ("ascii", "telnet")},
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def masked_frame(opcode: int, payload: bytes, final: bool = True) -> bytes:
    mask = os.urandom(4)
    size = len(payload)
    prefix = bytes([(0x80 if final else 0) | opcode])
    if size < 126:
        prefix += bytes([0x80 | size])
    elif size <= 65535:
        prefix += b"\xfe" + struct.pack("!H", size)
    else:
        prefix += b"\xff" + struct.pack("!Q", size)
    return prefix + mask + bytes(value ^ mask[i % 4] for i, value in enumerate(payload))


class Peer:
    """Minimal RFC 6455 peer; retain every received byte, including HTTP read-ahead."""

    def __init__(self, port: int, transport: str, protocol: str, wire: Path,
                 headers: list[str] | None = None, pipeline_close: bool = False):
        self.transport = transport
        self.protocol = protocol
        self.websocket = transport in ("ws", "wss")
        self.enforce_frame_utf8 = protocol == "ascii"
        self.raw = bytearray()
        self.text = bytearray()
        self.telnet_state = 0
        self.telnet_command = 0
        self.subnegotiation = bytearray()
        self.subnegotiations: list[bytes] = []
        self.telnet_commands: list[int] = []
        self.negotiations: list[tuple[int, int]] = []
        self.inflate = None
        self.mccp_requested = False
        self.close_code = None
        self.closed = False
        self.frames = 0
        self.wire_bytes = 0
        self.wire = wire.open("xb")
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.settimeout(10)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 8192)
        try:
            self.socket.connect(("127.0.0.1", port))
            if transport in ("wss", "tls"):
                context = ssl.create_default_context(cafile="etc/cert.pem")
                self.socket = context.wrap_socket(self.socket, server_hostname="localhost")
            if self.websocket:
                key = base64.b64encode(os.urandom(16)).decode("ascii")
                request = (f"GET / HTTP/1.1\r\nHost: localhost:{port}\r\n"
                           "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                           f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n"
                           f"Sec-WebSocket-Protocol: {protocol}\r\n"
                           + "".join(f"{header}\r\n" for header in (headers or [])) + "\r\n")
                extra = masked_frame(8, struct.pack("!H", 1000)) if pipeline_close else b""
                self.send_raw(request.encode("ascii") + extra)
                while b"\r\n\r\n" not in self.raw:
                    self.receive_raw()
                    require(len(self.raw) <= 65536, "oversized HTTP response")
                response, remainder = bytes(self.raw).split(b"\r\n\r\n", 1)
                self.raw = bytearray(remainder)
                lines = response.split(b"\r\n")
                require(lines[0].split()[1] == b"101", f"upgrade rejected: {lines[0]!r}")
                fields = dict(line.split(b":", 1) for line in lines[1:])
                fields = {key.lower(): value.strip() for key, value in fields.items()}
                expected = base64.b64encode(hashlib.sha1(
                    (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode("ascii")
                ).digest())
                require(fields.get(b"sec-websocket-accept") == expected, "wrong accept key")
                require(fields.get(b"sec-websocket-protocol") == protocol.encode(),
                        "wrong negotiated subprotocol")
        except BaseException:
            self.close()
            raise

    def record(self, direction: int, data: bytes) -> None:
        self.wire_bytes += len(data) + 5
        require(self.wire_bytes <= 8 * 1024 * 1024, "peer wire log limit exceeded")
        self.wire.write(struct.pack("!BI", direction, len(data)) + data)

    def send_raw(self, data: bytes) -> None:
        self.record(0, data)
        self.socket.sendall(data)

    def receive_raw(self) -> None:
        data = self.socket.recv(65536)
        self.record(1, data)
        if not data:
            self.closed = True
            raise EOFError("peer closed the transport")
        self.raw.extend(data)

    def take_raw(self, count: int) -> bytes:
        while len(self.raw) < count:
            self.receive_raw()
        data = bytes(self.raw[:count])
        del self.raw[:count]
        return data

    def send(self, data: bytes, opcode: int | None = None, final: bool = True) -> None:
        if self.websocket:
            if opcode is None:
                opcode = 1 if self.protocol == "ascii" else 2
            self.send_raw(masked_frame(opcode, data, final))
        else:
            self.send_raw(data)

    def send_command(self, command: str) -> None:
        self.send(command.encode() + b"\n")

    def decode_telnet(self, data: bytes) -> None:
        if self.inflate is not None:
            data = self.inflate.decompress(data)
        for i, value in enumerate(data):
            if self.telnet_state == 0:
                if value == 255:
                    self.telnet_state = 1
                else:
                    self.text.append(value)
            elif self.telnet_state == 1:
                if value == 255:
                    self.text.append(value)
                    self.telnet_state = 0
                elif value in (251, 252, 253, 254):
                    self.telnet_command = value
                    self.telnet_state = 2
                elif value == 250:
                    self.subnegotiation.clear()
                    self.telnet_state = 3
                else:
                    self.telnet_commands.append(value)
                    self.telnet_state = 0
            elif self.telnet_state == 2:
                self.negotiations.append((self.telnet_command, value))
                if self.telnet_command in (251, 253):
                    accepting_mccp = (self.telnet_command == 251 and value == 86
                                      and self.mccp_requested)
                    if not accepting_mccp:
                        reply = 254 if self.telnet_command == 251 else 252
                        self.send(bytes([255, reply, value]))
                self.telnet_state = 0
            elif self.telnet_state == 3:
                if value == 255:
                    self.telnet_state = 4
                else:
                    self.subnegotiation.append(value)
            elif self.telnet_state == 4:
                if value == 240:
                    self.subnegotiations.append(bytes(self.subnegotiation))
                    self.telnet_state = 0
                    if self.subnegotiation == b"\x56":
                        require(not self.websocket, "MCCP enabled on a websocket")
                        require(self.inflate is None, "MCCP started twice")
                        self.inflate = zlib.decompressobj()
                        self.decode_telnet(data[i + 1:])
                        return
                elif value == 255:
                    self.subnegotiation.append(255)
                    self.telnet_state = 3
                else:
                    raise AssertionError("invalid telnet subnegotiation")

    def receive(self) -> None:
        if self.websocket:
            first, second = self.take_raw(2)
            require(first & 0x80 != 0 and first & 0x70 == 0, "unexpected frame flags")
            require(second & 0x80 == 0, "masked server frame")
            size = second & 0x7f
            if size == 126:
                size = struct.unpack("!H", self.take_raw(2))[0]
            elif size == 127:
                size = struct.unpack("!Q", self.take_raw(8))[0]
            require(size <= 1024 * 1024, "oversized server frame")
            data = self.take_raw(size)
            opcode = first & 0x0f
            if opcode == 8:
                self.close_code = struct.unpack("!H", data[:2])[0] if data else None
                self.closed = True
                raise EOFError("websocket close frame")
            if opcode == 9:
                self.send(data, 10)
                return
            require(opcode == 2, f"unexpected output opcode {opcode}")
            self.frames += 1
        else:
            if not self.raw:
                self.receive_raw()
            data = bytes(self.raw)
            self.raw.clear()
        if self.protocol == "telnet":
            self.decode_telnet(data)
        else:
            if self.enforce_frame_utf8:
                data.decode("utf-8")
            self.text.extend(data)

    def expect(self, data: bytes) -> None:
        if not self.websocket:
            data = data.replace(b"\n", b"\r\n")
        while len(self.text) < len(data):
            self.receive()
        received = bytes(self.text[:len(data)])
        del self.text[:len(data)]
        require(received == data, f"payload mismatch: {received[:100]!r} != {data[:100]!r}")

    def line(self) -> bytes:
        while b"\n" not in self.text:
            self.receive()
        data, remainder = bytes(self.text).split(b"\n", 1)
        self.text = bytearray(remainder)
        return data.removesuffix(b"\r")

    def expect_close(self, normal: bool = False) -> None:
        try:
            while not self.closed:
                self.receive()
        except (EOFError, ConnectionResetError):
            self.closed = True
        require(not self.text, f"unexpected text before close: {bytes(self.text[:100])!r}")
        if normal and self.websocket:
            require(self.close_code == 1000, f"wrong close status: {self.close_code}")

    def close(self) -> None:
        self.socket.close()
        self.wire.close()


class DriverProbe:
    def __init__(self, driver: str):
        self.process = subprocess.Popen([driver, "etc/config.test", "-fwebsocket"],
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.ports: dict[str, int] = {}
        self.errors: list[str] = []
        self.ready = threading.Event()
        self.serial = 0
        self.peers: list[Peer] = []
        self.reader = threading.Thread(target=self.read_output)
        self.reader.start()

    def read_output(self) -> None:
        pending = bytearray()
        try:
            while chunk := self.process.stdout.read1(65536):
                remaining = memoryview(chunk)
                while remaining:
                    remaining = remaining[os.write(1, remaining):]
                pending.extend(chunk)
                while b"\n" in pending:
                    line, tail = pending.split(b"\n", 1)
                    pending = bytearray(tail)
                    match = re.search(
                        rb"Accepting (telnet|websocket)(\(TLS\))? "
                        rb"connections on 127\.0\.0\.1:(\d+)\.", line
                    )
                    if match:
                        kind, tls, port = match.groups()
                        key = {b"telnet": "plain", b"websocket": "ws"}[kind]
                        if tls:
                            key = "tls" if key == "plain" else "wss"
                        require(key not in self.ports, f"duplicate listener {key}")
                        self.ports[key] = int(port)
                        if len(self.ports) == 4:
                            self.ready.set()
                require(len(pending) <= 65536, "oversized driver log line")
        except BaseException as error:
            self.errors.append(str(error))
        finally:
            self.ready.set()

    def start(self) -> None:
        require(self.ready.wait(20), "driver listeners did not become ready")
        require(not self.errors, f"driver output reader failed: {self.errors}")
        require(set(self.ports) == {"plain", "tls", "ws", "wss"}, "missing listeners")
        require(self.process.poll() is None, "driver exited during startup")

    def peer(self, transport: str, protocol: str, **options) -> Peer:
        self.serial += 1
        peer = Peer(self.ports[transport], transport, protocol,
                    Path("log") / f"wire-{self.serial:03}.bin", **options)
        self.peers.append(peer)
        return peer

    def settled_stats(self, control: Peer) -> tuple[int, int]:
        deadline = time.monotonic() + 10
        while True:
            control.send_command("stats")
            words = control.line().split()
            require(len(words) == 3 and words[0] == b"STATS", f"bad stats: {words!r}")
            count, dead = int(words[1]), int(words[2])
            if count == 1:
                return count, dead
            require(time.monotonic() < deadline, f"user entries left after close: {count}")
            time.sleep(0.02)

    def finish(self, control: Peer) -> None:
        self.settled_stats(control)
        control.send_command("shutdown")
        control.expect(b"BYE\n")
        require(self.process.wait(timeout=15) == 0, "driver did not exit cleanly")
        self.reader.join(timeout=3)
        require(not self.reader.is_alive() and not self.errors, "incomplete driver log")

    def close(self) -> None:
        for peer in self.peers:
            peer.close()
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=3)
        self.reader.join(timeout=3)
        require(not self.reader.is_alive(), "driver log reader did not finish")
        self.process.stdout.close()


def probe_contract(probe: DriverProbe, transport: str, protocol: str) -> list[str]:
    checks = []
    peer = probe.peer(transport, protocol)
    peer.expect(b"READY 127.0.0.1\n")
    peer.send_command("hello 中🙂")
    peer.expect("ECHO hello 中🙂\n".encode())
    checks.append("echo")
    if transport in ("ws", "wss"):
        peer.send(b"fragment-", 1, False)
        peer.send("中🙂\n".encode(), 0)
        peer.expect("ECHO fragment-中🙂\n".encode())
        checks.append("fragment")
    if protocol == "telnet":
        peer.mccp_requested = True
        peer.send(bytes([255, 253, 86]))
        peer.send_command("after-mccp")
        peer.expect(b"ECHO after-mccp\n")
        if peer.websocket:
            require((252, 86) in peer.negotiations, "WS did not refuse MCCP")
            require(peer.inflate is None, "WS unexpectedly compressed telnet")
        else:
            require(peer.inflate is not None, "ordinary telnet did not enable MCCP")
        checks.append("mccp")
    peer.send_command("burst")
    time.sleep(0.03)
    peer.expect(BURST)
    checks.append("burst")
    peer.send_command("exec")
    peer.expect(b"BEFORE\nEXEC\n")
    peer.send_command("after-exec")
    peer.expect(b"ECHO after-exec\n")
    checks.append("exec")
    peer.close()
    control = probe.peer("ws", "ascii")
    control.expect(b"READY 127.0.0.1\n")
    _count, dead = probe.settled_stats(control)
    require(dead >= 1, "net_dead did not run on peer close")
    checks.append("net-dead-self-destruct")
    closing = probe.peer(transport, protocol)
    closing.expect(b"READY 127.0.0.1\n")
    if closing.websocket:
        # The close path sends all remaining bytes, even across UTF-8 frame boundaries.
        closing.enforce_frame_utf8 = False
        closing.send_command("close")
        time.sleep(0.03)
        closing.expect(BURST)
    else:
        # Ordinary remove_interactive() flushes once; it has no deferred-drain contract.
        closing.send_command("disconnect")
        closing.expect(b"CLOSED\n")
    closing.expect_close(normal=True)
    closing.close()
    checks.append("close-after-flush" if closing.websocket else "disconnect")
    if transport in ("ws", "wss"):
        for _ in range(8):
            early = probe.peer(transport, protocol, pipeline_close=True)
            early.close()
        probe.settled_stats(control)
        checks.append("prelogon-close")
        if protocol == "ascii":
            invalid = probe.peer(transport, protocol)
            invalid.expect(b"READY 127.0.0.1\n")
            invalid.send(b"not-text\n", 2)
            invalid.expect_close()
            invalid.close()
            checks.append("reject-binary-input")
    probe.finish(control)
    return checks


def probe_interactive_efuns(probe: DriverProbe) -> list[str]:
    peer = probe.peer("plain", "telnet")
    peer.expect(b"READY 127.0.0.1\n")
    checks = []

    peer.send_command("u11-output")
    peer.expect(b"U11_PRINTF:ok:7\n")
    peer.expect(b"U11_MESSAGE\n")
    peer.expect(b"U11_NOTIFY:U11_NOTIFY\n")
    checks.extend(["printf", "message", "notify-fail"])

    peer.send_command("u11-get-char")
    peer.expect(b"U11_GET_CHAR_READY\n")
    peer.send(b"Q")
    peer.expect(b"U11_GET_CHAR:Q\n")
    peer.send_command("after-get-char")
    peer.expect(b"ECHO after-get-char\n")
    checks.append("get-char")

    peer.send(bytes([255, 253, 90]))
    deadline = time.monotonic() + 5
    while (251, 90) not in peer.negotiations:
        require(time.monotonic() < deadline, "server did not accept MSP")
        peer.receive()
    peer.send_command("u11-telnet")
    peer.expect(b"U11_TELNET_DONE\n")
    require(241 in peer.telnet_commands, "telnet_nop was not emitted")
    require(249 in peer.telnet_commands, "telnet_ga was not emitted")
    expected_msp = b"\x5a!!SOUND(cow.wav L=2 V=100)"
    require(expected_msp in peer.subnegotiations, "telnet_msp_oob payload was not emitted")
    checks.extend(["telnet-nop", "telnet-ga", "telnet-msp-oob"])

    probe.finish(peer)
    return checks


def probe_callback_destruct(probe: DriverProbe, transport: str, protocol: str) -> list[str]:
    peer = probe.peer(transport, protocol)
    peer.expect(b"READY 127.0.0.1\n")
    peer.send_command("callback-destruct")
    peer.expect(b"ARMED\n")
    peer.send(bytes([255, 253, 201]))
    peer.expect(b"CALLBACK\n")
    peer.expect_close(normal=True)
    peer.close()
    control = probe.peer("ws", "ascii")
    control.expect(b"READY 127.0.0.1\n")
    _count, dead = probe.settled_stats(control)
    require(dead == 0, "destruct unexpectedly invoked net_dead")
    probe.finish(control)
    return ["gmcp-destruct", "user-count"]


def probe_proxy(probe: DriverProbe, policy: str, transport: str, protocol: str) -> list[str]:
    samples = [
        ("absent", [], "127.0.0.1"),
        ("ipv4", ["X-Real-IP: 192.0.2.42"], "192.0.2.42"),
        ("ipv6", ["X-Real-IP: 2001:db8::42"], "2001:db8::42"),
        ("mapped", ["X-Real-IP: ::ffff:192.0.2.42"], "192.0.2.42"),
        ("invalid", ["X-Real-IP: invalid.example"], None),
        ("list", ["X-Real-IP: 192.0.2.42, 192.0.2.43"], None),
        ("duplicate", ["X-Real-IP: 192.0.2.42", "X-Real-IP: 192.0.2.43"], None),
        ("long", ["X-Real-IP: " + "1" * 160], None),
    ]
    checks = []
    for name, headers, expected in samples:
        if policy == "untrusted":
            expected = "127.0.0.1"
        peer = probe.peer(transport, protocol, headers=headers)
        if expected is None:
            peer.expect_close()
        else:
            peer.expect(f"READY {expected}\n".encode())
        peer.close()
        checks.append(name)
    control = probe.peer("ws", "ascii")
    control.expect(b"READY 127.0.0.1\n")
    probe.finish(control)
    return checks


def run_probe(driver: str, case: str) -> None:
    require(Path(".fluffos-test-sandbox").is_file(), "probe requires a runner sandbox")
    policy, transport, protocol = CASES[case]
    probe = DriverProbe(driver)
    try:
        probe.start()
        if policy == "contract":
            checks = probe_contract(probe, transport, protocol)
        elif policy == "u11-interactive":
            checks = probe_interactive_efuns(probe)
        elif policy == "callback-destruct":
            checks = probe_callback_destruct(probe, transport, protocol)
        else:
            checks = probe_proxy(probe, policy, transport, protocol)
        report = {"case": case, "checks": checks, "driver_returncode": probe.process.returncode}
    finally:
        probe.close()
    print("WEBSOCKET_RESULT=" + json.dumps(report, sort_keys=True), flush=True)


class WebsocketTest(unittest.TestCase):
    driver: Path
    session: runner.EvidenceSession

    def __init__(self, case: str):
        super().__init__()
        self.case = case

    def id(self) -> str:
        return self.case

    def shortDescription(self) -> str:
        return self.case

    def runTest(self) -> None:
        _root, mudlib = runner.render_sandbox(self.session)
        config_path = mudlib / "etc/config.test"
        config, count = re.subn(r"^master file : .*$", "master file : /single/u09_websocket_master",
                                config_path.read_text(), flags=re.MULTILINE)
        self.assertEqual(count, 1)
        policy, transport, protocol = CASES[self.case]
        if policy == "trusted":
            config += "\nwebsocket trusted proxy cidrs : 127.0.0.1/32\n"
        config_path.write_text(config)
        (mudlib / "single/u09_websocket_master.lpc").write_text(
            'inherit "/single/master";\n'
            'private int net_dead_count;\n'
            'void flag(string argument) {}\n'
            'object connect() { return new("/clone/websocket_session_user"); }\n'
            'void record_net_dead() { net_dead_count++; }\n'
            'int query_net_dead() { return net_dead_count; }\n'
            'void finish_tests() { shutdown(has_error ? 1 : 0); }\n'
        )
        result = runner.run_process(
            self.session,
            [sys.executable, "-B", str(Path(__file__).resolve()), "--driver", str(self.driver),
             "--probe", self.case],
            cwd=mudlib, environment=runner.command_environment(mudlib), timeout=150,
            label=self.case, binary=self.driver,
        )
        runner.reject_sanitizer_output(result.output)
        self.assertFalse(result.timed_out, result.log_path)
        self.assertEqual(result.failure_reason, "", result.log_path)
        self.assertEqual(result.returncode, 0, result.log_path)
        reports = re.findall(r"^WEBSOCKET_RESULT=(.+)$", result.output, re.MULTILINE)
        self.assertEqual(len(reports), 1, result.log_path)
        report = json.loads(reports[0])
        self.assertEqual(report["case"], self.case)
        self.assertEqual(report["driver_returncode"], 0)
        if policy == "u11-interactive":
            expected = ["printf", "message", "notify-fail", "get-char",
                        "telnet-nop", "telnet-ga", "telnet-msp-oob"]
        elif policy == "callback-destruct":
            expected = ["gmcp-destruct", "user-count"]
        elif policy != "contract":
            expected = ["absent", "ipv4", "ipv6", "mapped", "invalid", "list", "duplicate", "long"]
        else:
            expected = ["echo"]
            if transport in ("ws", "wss"):
                expected.append("fragment")
            if protocol == "telnet":
                expected.append("mccp")
            expected += ["burst", "exec", "net-dead-self-destruct",
                         "close-after-flush" if transport in ("ws", "wss") else "disconnect"]
            if transport in ("ws", "wss"):
                expected.append("prelogon-close")
                if protocol == "ascii":
                    expected.append("reject-binary-input")
        self.assertEqual(report["checks"], expected, result.log_path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--driver", required=True)
    parser.add_argument("--evidence-dir")
    parser.add_argument("--case", action="append", choices=CASES)
    parser.add_argument("--probe", choices=CASES, help=argparse.SUPPRESS)
    options = parser.parse_args()
    if options.probe:
        run_probe(options.driver, options.probe)
        return 0
    WebsocketTest.driver = runner.require_executable(options.driver)
    WebsocketTest.session = runner.EvidenceSession(options.evidence_dir)
    cases = options.case or list(CASES)
    if len(set(cases)) != len(cases):
        parser.error("duplicate --case")
    suite = unittest.TestSuite(WebsocketTest(case) for case in cases)
    discovered = suite.countTestCases()
    print(f"evidence_directory: {WebsocketTest.session.root}", flush=True)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    passed = (result.wasSuccessful() and not result.skipped
              and result.testsRun == discovered and discovered > 0)
    (WebsocketTest.session.root / "test-summary.json").write_text(json.dumps({
        "passed": passed, "cases": cases, "discovered": discovered, "executed": result.testsRun,
        "failures": len(result.failures), "errors": len(result.errors),
        "skipped": len(result.skipped),
    }, indent=2) + "\n")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
