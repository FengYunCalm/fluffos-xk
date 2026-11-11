---
layout: doc
title: S16 websocket live smoke evidence (ws/wss)
---
# S16 websocket live smoke evidence

Live (non-gtest) evidence for the websocket transport on the current tree:
handshake, bidirectional data, disconnect, MCCP2 refusal on websocket
transports, and a clean AddressSanitizer log.

## Environment

- Driver: `build-asan/bin/driver` (Debug + AddressSanitizer, LTO off),
  started from `testsuite/` with `etc/config.test`:
  `ASAN_OPTIONS=detect_leaks=0 ../build-asan/bin/driver etc/config.test`
- Ports from that config: 4001 websocket (plain), 4002 websocket + TLS,
  4000 telnet (plain control), 4003 telnet + TLS.
- Client: `src/tests/ws_smoke.py` (stdlib only: `socket`/`ssl`/
  `hashlib`/`base64`; no third-party websocket library). It performs the
  RFC6455 handshake, verifies `Sec-WebSocket-Accept`, receives at least one
  data frame, sends one frame, and closes.
- Both runs below were issued against a single long-running driver process;
  the ASan log was checked after the traffic.

## Commands

```bash
cd testsuite
ASAN_OPTIONS=detect_leaks=0 ../build-asan/bin/driver etc/config.test &
for k in ws-ascii ws-telnet wss-ascii wss-telnet \
         ws-mccp-telnet wss-mccp-telnet telnet-mccp \
         ws-burst wss-burst ws-burst-flush wss-burst-flush; do
  python3 ../src/tests/ws_smoke.py "$k"
done
```

## Observed output

```
OK ws://127.0.0.1:4001/ascii opcodes=[2, 2] frames=2 bytes=487 after_send=63
OK ws://127.0.0.1:4001/telnet opcodes=[2, 2] frames=2 bytes=459 after_send=2
OK wss://127.0.0.1:4002/ascii opcodes=[2, 2] frames=2 bytes=487 after_send=63
OK wss://127.0.0.1:4002/telnet opcodes=[2, 2] frames=2 bytes=459 after_send=2
MCCP telnet refused=True compressed_tail=False negotiation=b'\xff\xfeV'
MCCP telnet/tls refused=True compressed_tail=False negotiation=b'\xff\xfeV'
MCCP plain-telnet accepted=True reply=b'\xff\xfdV'
BURST ws://127.0.0.1:4001/ascii frames=3 bytes=6014 y_count=6000 prompt=yes
BURST wss://127.0.0.1:4002/ascii frames=3 bytes=6014 y_count=6000 prompt=yes
BURST ws://127.0.0.1:4001/ascii frames=82 bytes=165013 y_count=160000 expected=160000 prompt=yes
BURST wss://127.0.0.1:4002/ascii frames=82 bytes=165013 y_count=160000 expected=160000 prompt=yes
```

Interpretation:

- The four `OK` lines are handshake + receive + send + close on both
  protocols, with and without TLS. `bytes=` is the mudlib banner received
  before sending; `after_send=` is what came back after the client's frame,
  so data flowed in both directions before the client closed.
- `\xff\xfeV` is `IAC DONT <telenet option 86 = COMPRESS2>`: on websocket
  transports the driver answers the client's MCCP2 offer with DONT, and the
  following bytes stay plain (no deflate stream), i.e. the websocket layer's
  own compression is not nested with MCCP.
- `\xff\xfdV` on the plain telnet port is `IAC DO COMPRESS2`: MCCP is still
  negotiated where it is appropriate, so the websocket refusal is specific to
  websocket transports rather than a global regression.
- The `BURST` lanes push output repeatedly (6000 chars from one returned
  string, then 5000 separate `write()` calls = 160 000 chars) and assert the
  byte count, the character count and the trailing prompt, covering the
  multi-window write path on both plain ws and wss: a truncated, dropped or
  duplicated drain would show up as a missing character count.

## Sanitizer result

After all eleven connections plus the disconnect of each session, the ASan
driver log contained **0** matches for `AddressSanitizer` / `LeakSanitizer`,
and the process shut down cleanly (`closed external ports`,
`clear_call_outs: 0 leftover callouts cleared.`). Leak detection is off for
this smoke run (`detect_leaks=0`); the leak baseline is tracked separately.

## What the burst lane does not prove

The websocket write path drains `numbytes` (the payload) instead of
`lws_write()`'s return value, because that return can exceed the payload on
TLS. A local negative control (temporarily restoring the old
`evbuffer_drain(pss->buffer, m)` form and re-running the burst lanes) still
delivered both burst sizes intact, so these runs demonstrate "no loss in this
environment", not a reproduction of the over-drain. The change stays
justified by lws's documented return contract and upstream's analysis; the
burst lanes are the guard that would catch it if it did trigger (a slow or
choked reader, larger payloads, or a different lws build).

## Notes and limits

- The script is a manual/live tool, not part of `ctest`: it needs a driver
  listening on fixed ports. It is deliberately dependency-free so it can run
  on any machine with Python 3.
- Telnet negotiation bytes cannot be sent in a websocket *text* frame (lws
  closes such a frame with status 1007 "bad utf8"), so the MCCP probes send
  them as a binary frame; `ws_telnet` accepts binary frames by design while
  `ws_ascii` rejects them.
- Backpressure behaviour with a *slow* reader is not covered: the client reads
  as fast as it can. The websocket send paths are also pinned by the
  source-contract tests in `src/tests/test_lpc.cc`
  (`TestUserLogonSchedulingChecksEventBaseOnceResult`).
