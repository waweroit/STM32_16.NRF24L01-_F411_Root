# Device2

Local device ID: `0x02`.

This directory contains CubeMX integration sources rather than a generated `.ioc` project. Follow `INTEGRATION.md`. The application fails closed until a persistent boot-counter backend is supplied.

## Fragmentation

The example now uses `SecureTransport` above `SecureProtocol`. Queue messages can contain up to 256 application bytes. Messages longer than 11 bytes are split into authenticated nRF24-sized frames and reassembled before `ApplicationTask` receives them.

Device2 exposes the most recently reassembled heartbeat through `g_lastHeartbeat` and `g_lastHeartbeatLength` for debugger inspection.
