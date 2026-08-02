# Device1

Local device ID: `0x01`.

This directory contains CubeMX integration sources rather than a generated `.ioc` project. Follow `INTEGRATION.md`. The application fails closed until a persistent boot-counter backend is supplied.

## Fragmentation

The example now uses `SecureTransport` above `SecureProtocol`. Queue messages can contain up to 256 application bytes. Messages longer than 11 bytes are split into authenticated nRF24-sized frames and reassembled before `ApplicationTask` receives them.

Device1 sends `Hello World !` (13 bytes) every 5 seconds as a fragmented heartbeat in addition to the original temperature message.
