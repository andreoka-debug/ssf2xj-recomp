# Native match synchronization

This is the first Windows-to-Windows synchronization layer. It does not connect
to the web lobby, Supabase accounts, or public matchmaking yet. The existing
FBNeo web client and backend remain unchanged.

Each instance supplies one player's 12-bit held-button mask. A frame runs only
after both peers have supplied that frame's input and agreed on the preceding
state checksum. The host is P1, the joining instance P2; **both PCs use their
local P1 keyboard/first-gamepad mapping**. PPP and KKK bindings work normally.

## Try it locally

Build with `build.ps1`, then run these from two PowerShell windows in the source
folder:

```powershell
./netplay.ps1 -Mode Host
./netplay.ps1 -Mode Join
```

The default endpoint is `127.0.0.1:27888` and the connection deadline is 30 seconds.
Press Esc to leave. The window title shows the assigned player, connection state
and confirmed frame. Logs go to `userdata/logs/`.

On a trusted LAN, the host can specify `-Address` with its local IPv4 address;
the joining PC must specify that same address. Both need the identical game
executable, their own ROMs, and matching feature settings. The helper starts
with a fresh EEPROM on both sides and leaves offline saves intact.

The transport is an **unauthenticated TCP test transport**. Do not expose it as
a public matchmaking endpoint. Account authentication, room membership, WebRTC
signaling, NAT traversal and integration into the launcher are separate work.
There is no Windows-to-FBNeo crossplay, rollback, input-delay pipeline, automatic
reconnection or relay in this implementation. Stop-and-wait stepping establishes
correctness; internet latency will reduce its frame rate. Low-latency scheduling
must be implemented and measured before treating this as a finished online mode.

## Protocol and compatibility

- Nonblocking Winsock sockets with `TCP_NODELAY`; a bounded amount of work per
  poll keeps the event loop responsive while waiting.
- TCP messages have a 4-byte big-endian length, limited to 4,096 bytes. Send and
  receive queues are bounded. Partial reads/writes are retained.
- Hello: type 1, protocol version, player role and compatibility identity. The
  identity includes the executable SHA-256, validated ROM region checksums,
  CPU/sound modes, feature values, frame limit and initial state checksum.
- Input: type 2, 64-bit frame number, 32-bit input (only bits 0–11 are allowed),
  and 32-bit preceding state checksum. The local sample is frozen once sent.
- Completion: type 3 with the same layout, confirming the final state when a
  frame limit is used. A successful limited run requires this confirmation.
- The checksum covers the port's existing CPU, RAM, video, object/register,
  sound-CPU and framebuffer checksum, plus EEPROM. It is a divergence diagnostic,
  not a cryptographic proof of correct play or an anti-cheat system.
- Out-of-order frames, invalid messages, mismatches, EOF and lack of progress
  fail the session. Failed peers never fall back to independent offline play.
- Training and run-ahead are disabled for the session without changing saved
  preferences. Pause, speed, reset, service input, save/load and training commands
  cannot mutate just one peer. A movie can provide deterministic local inputs for
  tests; only its assigned player's column is used.
- There are no ROM transfers, untrusted save-state deserialization, database
  changes or external service calls in this layer.

The native socket boundary follows Microsoft's documented [nonblocking socket
behavior](https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-ioctlsocket)
and [partial receive / disconnect behavior](https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-recv).

## Reproduce the tests

```powershell
python tests/test_netplay.py --frames 6000
```

Requires MSYS2 UCRT64 and a built game. All listeners bind to loopback. Reports,
logs, temporary EEPROM copies and test binaries stay under `_build/netplay-tests/`.
The suite checks fragmented/delayed packets, protocol/role/runtime mismatches,
bad input bits, duplicate/future frames, divergent checksums, disconnects and
timeouts. It also runs two independent native game processes with separate P1/P2
input movies, confirms the last frame, and compares every frame with an offline
reference. The unused movie column contains deliberately incorrect commands to
detect accidental replay of remote inputs.

Direct game arguments for diagnostics are `--net-host IP:PORT`,
`--net-join IP:PORT`, `--net-timeout MS` (100–60,000), and `--net-log FILE`.
Use the same `--frames N` on both peers for a bounded test. Errors return a
nonzero exit code; synchronization failures return 5.
