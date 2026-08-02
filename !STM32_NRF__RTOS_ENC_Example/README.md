# STM32 nRF24L01+ FreeRTOS AES-CTR example

## Purpose

This repository is a CubeMX/CubeIDE integration example for two STM32F411CEU6 devices communicating through nRF24L01+ modules. It provides:

- STM32 HAL SPI radio driver,
- CMSIS-RTOS v2 radio and application tasks,
- AES-128-CTR confidentiality,
- AES-CMAC authentication with an 8-byte tag,
- directional KDF-derived encryption/authentication keys,
- per-peer TX/RX counters,
- replay protection and restart/session handling,
- transparent application-message fragmentation/reassembly up to 256 bytes,
- host tests that do not require a radio.

A single SecureProtocol/nRF24 frame still carries at most **11 plaintext bytes** because the authenticated header is 13 bytes and the tag is 8 bytes. `SecureTransport` removes this application-level limitation by splitting longer logical messages across multiple independently authenticated SecureProtocol frames. The default logical application-message limit is **256 bytes**.

## Directory layout

```text
CommonLibrary/Inc      public headers
CommonLibrary/Src      radio, crypto, protocol and transport implementation
Device1/Core           Device ID 0x01 example
Device2/Core           Device ID 0x02 example
Tests                  host tests and HAL syntax stubs
ProtocolDescription.md byte-level protocol specification
THIRD_PARTY_NOTICES.md AES provenance/license
```

## Third-party AES

The project uses the tiny-AES-c API and AES-128 encryption/CTR path by kokke and contributors, reference release `v1.0.0`, licensed under The Unlicense/public domain. The integrated files are `tiny_aes.h` and `tiny_aes.c`. AES-CMAC, KDF, argument checks and error codes are project code layered above the AES primitive.

## CommonLibrary integration in STM32CubeIDE

1. Create two independent STM32CubeMX projects under `Device1` and `Device2` or copy their generated `Core`, `Drivers` and `Middlewares` directories into those folders.
2. Add `../CommonLibrary/Inc` to **Project > Properties > C/C++ Build > Settings > MCU GCC Compiler > Include paths**.
3. Link or copy all six `CommonLibrary/Src/*.c` files into each project, including `SecureTransport.c`. In CubeIDE, linked resources avoid duplicate source copies.
4. Add `DeviceX/Core/Inc` to include paths and compile `deviceX_app.c` plus `secure_storage_port.c`.
5. Keep `NRF24_HAL_HEADER` at its default `stm32f4xx_hal.h` for STM32F4. Other STM32 families may define it to the appropriate HAL umbrella header.
6. Configure CMSIS-RTOS v2, SPI1 and GPIO as described in each `INTEGRATION.md`.

## Radio defaults

- channel 76,
- 1 Mbps,
- 0 dBm maximum PA power,
- 5-byte addresses,
- hardware CRC enabled, 2 bytes,
- Auto ACK on pipe 0,
- 15 retries, 750 µs retry delay,
- dynamic payload enabled,
- maximum physical payload 32 bytes.

The examples use addresses `E7:E7:E7:E7:01` and `E7:E7:E7:E7:02`. During TX, pipe 0 is temporarily set to the destination address as required for Auto ACK and then restored to the local RX address.

## FreeRTOS architecture

`ApplicationTask` never calls SPI or the radio driver. It exchanges fixed-size queue items with `RadioTask`. `RadioTask` owns `SecureProtocol` and `SecureTransport` state and controls RX/TX transitions. A CMSIS mutex protects radio SPI transactions and can be shared with other SPI1 clients.

`RadioTxMessage_t` and `ApplicationRxMessage_t` can carry up to `SECURE_TRANSPORT_MAX_MESSAGE_SIZE` (256) application bytes. Messages of 0..11 bytes remain single SecureProtocol frames. Longer messages are fragmented automatically; the receiving `RadioTask` queues data to the application only after the complete logical message has been reassembled.

Device1 sends the explicitly encoded 5-byte temperature message every 1000 ms and additionally sends the 13-byte `Hello World !` heartbeat every 5000 ms to exercise fragmentation. Device2 stores the latest temperature values and the last reassembled heartbeat, and toggles Device1's LED command after each group of five authenticated temperature messages.

## Keys

`DeviceKeys.c` contains random-looking demonstration keys. They are not production secrets. Generate independent 16-byte keys with a cryptographically secure host tool, for example:

```text
python -c "import secrets; print(secrets.token_hex(16))"
```

Provision only the local master key and authorized peer sender keys into each firmware image. Protect release binaries and debug access. Rotate all demonstration keys before deployment.

To add Device3:

1. assign a unique nonzero ID such as `0x03`,
2. generate an independent 16-byte master key,
3. add it to the authorized key table of peers that may receive Device3 frames,
4. call `SecureProtocol_AddPeer` for each direction,
5. allocate a unique 5-byte radio address or use a shared network address with protocol destination filtering,
6. maintain separate `SecurePeerContext_t` state for each peer.

No on-air format change is required while `SECURE_MAX_PEERS` is sufficient.

## Restart/storage requirements

The included storage port is fail-closed by default. `DeviceXApp_Initialize` returns `false` until a persistent `LoadBootCounter/SaveBootCounter` implementation is supplied. Defining `SECURE_DEMO_ALLOW_VOLATILE_STORAGE=1` permits bench testing only and is insecure after reset.

Recommended production implementation: AT24C32 or wear-levelled Flash with redundant records, sequence number and CRC. The boot counter is written once per boot, not once per packet. Peer session state is written on session change and the RX counter is checkpointed every 256 frames in the example.

## Host tests

On a machine with GCC:

```text
cd Tests
make clean
make run
```

The suite covers NIST AES-CTR, RFC 4493 CMAC, non-block lengths, nonce variation, wrong keys, header/cipher/tag tampering, replay, packet loss, destinations, unknown devices, malformed lengths, max physical payload, serialization, counter update ordering, session changes, direct transport messages, 13-byte fragmentation, 256-byte fragmentation/reassembly, missing fragments and authenticated-fragment tampering.

## Fragmentation/reassembly

`SecureTransport` is layered above `SecureProtocol`. It does not alter the 32-byte nRF24 frame format. For fragmented messages, the 11-byte encrypted SecureProtocol payload contains a 4-byte transport header and up to 7 bytes of application data:

```text
messageId[1] | originalMessageType[1] | fragmentIndex[1] | fragmentCount[1] | data[1..7]
```

`MESSAGE_TYPE_FRAGMENT (0x7F)` is reserved for these internal frames. Each fragment has its own SecureProtocol counter, CTR nonce and CMAC. The receiver maintains one incomplete logical message per source device and releases data to the application only when every fragment has arrived.

The default logical message limit is 256 bytes (37 fragments maximum). It can be changed deliberately in `SecureTransport.h`, subject to RAM, queue-size and latency analysis.

## Limitations

- One incomplete fragmented message is retained per source device. A newer `messageId` replaces an older incomplete message from that source.
- Version 1 SecureProtocol still requires monotonically increasing counters, so arbitrary radio-frame reordering is not supported.
- Fragment loss prevents completion of that logical message. Hardware Auto ACK/retries reduce this risk; application-level whole-message ACK/retry can be added later if required.
- The 8-byte tag provides 64-bit forgery resistance per attempt; rate-limit hostile traffic in deployed products.
- The example polls RX every 2 ms. IRQ/EXTI can replace polling.
- The storage backend must be completed for secure power-cycle behavior.
- Device source is integration code, not generated `.ioc` projects.
- Physical nRF24L01+ modules require clean 3.3 V supply decoupling; add at least 100 nF plus a local bulk capacitor close to each module.
