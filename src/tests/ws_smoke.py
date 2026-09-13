#!/usr/bin/env python3
"""Minimal WebSocket (ws/wss) smoke client for the FluffOS_XK driver.

Stdlib only: performs the RFC6455 handshake against an lws-mounted protocol
("ascii" / "telnet"), receives at least one data frame, sends a text frame,
closes cleanly, and (for the telnet protocol) checks that MCCP2 is refused on
websocket transports while plain telnet still negotiates it.

Usage (driver running with testsuite/etc/config.test from testsuite/):

    python3 ws_smoke.py ws-ascii
    python3 ws_smoke.py ws-telnet
    python3 ws_smoke.py wss-ascii
    python3 ws_smoke.py wss-telnet
    python3 ws_smoke.py ws-mccp-telnet
    python3 ws_smoke.py wss-mccp-telnet
    python3 ws_smoke.py telnet-mccp
    python3 ws_smoke.py ws-burst
    python3 ws_smoke.py wss-burst
    python3 ws_smoke.py ws-burst-flush
    python3 ws_smoke.py wss-burst-flush
"""
import base64
import hashlib
import os
import socket
import ssl
import struct
import sys

IAC, WILL, WONT, DO, DONT = 0xFF, 0xFB, 0xFC, 0xFD, 0xFE
COMPRESS2 = 86  # TELNET_TELOPT_COMPRESS2 (MCCP2, RFC 1073-era option 86)


def handshake(sock, host, path, subprotocol):
    key = base64.b64encode(os.urandom(16)).decode()
    req = (
        f"GET {path} HTTP/1.1\r\n"
        f"Host: {host}\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        f"Sec-WebSocket-Protocol: {subprotocol}\r\n"
        "\r\n"
    )
    sock.sendall(req.encode())
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = sock.recv(4096)
        if not chunk:
            raise RuntimeError(f"connection closed during handshake: {data!r}")
        data += chunk
    head, _, rest = data.partition(b"\r\n\r\n")
    status = head.split(b"\r\n")[0].decode(errors="replace")
    if "101" not in status:
        raise RuntimeError(f"handshake rejected: {status}")
    expected = base64.b64encode(
        hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()
    ).decode()
    if expected.lower().encode() not in head.lower():
        raise RuntimeError("Sec-WebSocket-Accept mismatch")
    return rest


def send_frame(sock, payload, opcode=0x1):
    data = payload if isinstance(payload, bytes) else payload.encode()
    mask = os.urandom(4)
    header = bytearray([0x80 | opcode])
    length = len(data)
    if length < 126:
        header.append(0x80 | length)
    elif length < 65536:
        header.append(0x80 | 126)
        header += struct.pack(">H", length)
    else:
        header.append(0x80 | 127)
        header += struct.pack(">Q", length)
    header += mask
    masked = bytes(b ^ mask[i % 4] for i, b in enumerate(data))
    sock.sendall(bytes(header) + masked)


def recv_frame(sock, buffered=b""):
    def read(n):
        nonlocal buffered
        while len(buffered) < n:
            chunk = sock.recv(65536)
            if not chunk:
                raise EOFError("connection closed by server")
            buffered += chunk
        out, buffered = buffered[:n], buffered[n:]
        return out

    head = read(2)
    opcode = head[0] & 0x0F
    length = head[1] & 0x7F
    if length == 126:
        length = struct.unpack(">H", read(2))[0]
    elif length == 127:
        length = struct.unpack(">Q", read(8))[0]
    payload = read(length) if length else b""
    return opcode, payload, buffered


def connect(host, port, use_tls):
    raw = socket.create_connection((host, port), timeout=10)
    if use_tls:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE
        raw = ctx.wrap_socket(raw, server_hostname=host)
    return raw


def run(url, host, port, path, subprotocol, use_tls, send_text):
    raw = connect(host, port, use_tls)
    buffered = handshake(raw, host, path, subprotocol)

    frames, opcodes = [], []
    raw.settimeout(6)
    try:
        for _ in range(3):
            opcode, payload, buffered = recv_frame(raw, buffered)
            opcodes.append(opcode)
            frames.append(payload)
            if opcode == 0x8:
                break
    except (socket.timeout, EOFError):
        pass
    if not any(op in (0x1, 0x2) and payload for op, payload in zip(opcodes, frames)):
        raise RuntimeError(f"no data frame received (opcodes={opcodes})")

    send_frame(raw, send_text)
    after_send = 0
    try:
        for _ in range(3):
            opcode, payload, buffered = recv_frame(raw, buffered)
            opcodes.append(opcode)
            frames.append(payload)
            after_send += len(payload)
            if opcode == 0x8:
                break
    except (socket.timeout, EOFError):
        pass

    try:
        raw.sendall(b"\x88\x80" + os.urandom(4))  # masked empty close frame
    except OSError:
        pass
    raw.close()
    total = sum(len(f) for f in frames)
    print(
        f"OK {url} opcodes={opcodes} frames={len(frames)} bytes={total} "
        f"after_send={after_send}"
    )


def drain_frames(sock, buffered, count=2):
    frames = []
    try:
        for _ in range(count):
            opcode, payload, buffered = recv_frame(sock, buffered)
            frames.append(payload)
            if opcode == 0x8:
                break
    except (socket.timeout, EOFError):
        pass
    return frames, buffered


def run_mccp_refusal(host, port, path, subprotocol, use_tls):
    """A websocket telnet client offering MCCP2 must be refused."""
    raw = connect(host, port, use_tls)
    buffered = handshake(raw, host, path, subprotocol)
    raw.settimeout(8)
    _, buffered = drain_frames(raw, buffered)

    # Telnet negotiation bytes are not valid UTF-8, so they must go in a
    # binary frame (lws closes a text frame carrying invalid UTF-8 with 1007).
    send_frame(raw, bytes([IAC, WILL, COMPRESS2, IAC, DO, COMPRESS2]), opcode=0x2)
    replies, buffered = drain_frames(raw, buffered, count=3)

    negotiation = b"".join(replies)
    refused = bytes([IAC, WONT, COMPRESS2]) in negotiation or bytes([IAC, DONT, COMPRESS2]) in negotiation
    compressed = negotiation[:2] in (b"\x78\x01", b"\x78\x9c", b"\x78\xda")
    raw.close()
    print(
        f"MCCP {subprotocol}{'/tls' if use_tls else ''} refused={refused} "
        f"compressed_tail={compressed} negotiation={negotiation[:16]!r}"
    )
    if not refused:
        raise SystemExit("MCCP negotiation was not refused on a websocket transport")
    if compressed:
        raise SystemExit("server started a deflate stream despite refusing MCCP")


def run_mccp_plain(host, port):
    """Control: plain telnet is offered COMPRESS2 and must still accept it."""
    sock = socket.create_connection((host, port), timeout=10)
    sock.settimeout(8)
    try:
        sock.recv(4096)
    except socket.timeout:
        pass
    sock.sendall(bytes([IAC, WILL, COMPRESS2]))
    reply = b""
    try:
        reply = sock.recv(4096)
    except socket.timeout:
        pass
    sock.close()
    accepted = bytes([IAC, DO, COMPRESS2]) in reply
    print(f"MCCP plain-telnet accepted={accepted} reply={reply[:16]!r}")
    if not accepted:
        raise SystemExit("plain telnet should still accept MCCP2")


def run_burst(url, host, port, path, subprotocol, use_tls, expect_bytes=6000, flush=False):
    """Multi-window output burst: proves the write path drains exactly what
    was written (a short/over-long drain truncates or repeats data), which is
    what TLS is sensitive to because lws_write()'s return value is not the
    byte count consumed.

    flush=True drives the output through 5000 separate write() calls instead
    of one returned string (printf caps a single value's rendering), so the
    websocket send buffer really does refill across many windows.
    """
    raw = connect(host, port, use_tls)
    buffered = handshake(raw, host, path, subprotocol)
    raw.settimeout(20)
    _, buffered = drain_frames(raw, buffered, count=3)

    if flush:
        lines = 5000
        expect_bytes = lines * 32
        expression = (
            f'eval {{ for (int i = 0; i < {lines}; i++) '
            'write("yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy\\n"); return 1; }\n'
        )
    else:
        expression = f'eval return repeat_string("y", {expect_bytes})\n'
    send_frame(raw, expression)

    frames = []
    try:
        while True:
            opcode, payload, buffered = recv_frame(raw, buffered)
            frames.append(payload)
            if opcode == 0x8:
                break
    except (socket.timeout, EOFError):
        pass
    raw.close()

    total = sum(len(f) for f in frames)
    payload_text = b"".join(frames).decode("utf-8", "replace")
    y_count = payload_text.count("y")
    print(
        f"BURST {url} frames={len(frames)} bytes={total} y_count={y_count} "
        f"expected={expect_bytes} prompt={'yes' if payload_text.rstrip().endswith('>') else 'no'}"
    )
    if y_count < expect_bytes:
        raise SystemExit(f"burst response truncated: {y_count} < {expect_bytes}")


def main():
    kind = sys.argv[1]
    host = "127.0.0.1"
    config = {
        "ws-ascii": ("ws://127.0.0.1:4001/ascii", 4001, "/ascii", "ascii", False, "look\n"),
        "ws-telnet": ("ws://127.0.0.1:4001/telnet", 4001, "/telnet", "telnet", False, "\n"),
        "wss-ascii": ("wss://127.0.0.1:4002/ascii", 4002, "/ascii", "ascii", True, "look\n"),
        "wss-telnet": ("wss://127.0.0.1:4002/telnet", 4002, "/telnet", "telnet", True, "\n"),
    }
    if kind == "ws-burst":
        run_burst("ws://127.0.0.1:4001/ascii", host, 4001, "/ascii", "ascii", False)
    elif kind == "wss-burst":
        run_burst("wss://127.0.0.1:4002/ascii", host, 4002, "/ascii", "ascii", True)
    elif kind == "ws-burst-flush":
        run_burst("ws://127.0.0.1:4001/ascii", host, 4001, "/ascii", "ascii", False, flush=True)
    elif kind == "wss-burst-flush":
        run_burst("wss://127.0.0.1:4002/ascii", host, 4002, "/ascii", "ascii", True, flush=True)
    elif kind == "ws-mccp-telnet":
        run_mccp_refusal(host, 4001, "/telnet", "telnet", False)
    elif kind == "wss-mccp-telnet":
        run_mccp_refusal(host, 4002, "/telnet", "telnet", True)
    elif kind == "telnet-mccp":
        run_mccp_plain(host, 4000)
    else:
        url, port, path, subprotocol, use_tls, send_text = config[kind]
        run(url, host, port, path, subprotocol, use_tls, send_text)


if __name__ == "__main__":
    main()
