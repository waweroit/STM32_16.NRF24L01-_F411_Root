# Final implementation report - fragmentation update

Updated: 2026-08-02

## 1. Added/modified files

### Added

- `CommonLibrary/Inc/SecureTransport.h`
- `CommonLibrary/Src/SecureTransport.c`

### Modified

- `CommonLibrary/Inc/SecureProtocol.h` - reserves `MESSAGE_TYPE_FRAGMENT = 0x7F`.
- `Device1/Core/Inc/device1_app.h`
- `Device1/Core/Src/device1_app.c`
- `Device2/Core/Inc/device2_app.h`
- `Device2/Core/Src/device2_app.c`
- `README.md`
- `ProtocolDescription.md`
- `Device1/README.md`
- `Device1/INTEGRATION.md`
- `Device2/README.md`
- `Device2/INTEGRATION.md`
- `Tests/Makefile`
- `Tests/run_checks.sh`
- `Tests/test_secure_protocol.c`
- `Tests/README.md`
- `Tests/TEST_RESULTS.txt`

The original AES, CMAC, KDF and 32-byte SecureProtocol wire format remain unchanged.

## 2. Fragmentation architecture

`SecureTransport` is layered above `SecureProtocol`:

```text
Application message (0..256 B)
        |
        v
SecureTransport
        |
        +-- 0..11 B: one normal SecureProtocol frame
        |
        +-- >11 B: N MESSAGE_TYPE_FRAGMENT frames
                         |
                         v
                   SecureProtocol
                         |
                         v
                     nRF24L01+
```

Every physical nRF24 packet remains at or below 32 bytes.

## 3. Fragment format

The existing SecureProtocol permits 11 plaintext bytes per RF frame. A fragmented frame uses 4 bytes for encrypted transport metadata:

```text
byte 0    : messageId
byte 1    : originalMessageType
byte 2    : fragmentIndex
byte 3    : fragmentCount
byte 4..10: up to 7 application bytes
```

`MESSAGE_TYPE_FRAGMENT = 0x7F` is reserved for transport frames.

A full fragment therefore carries 7 application bytes and produces a 32-byte RF payload after the existing SecureProtocol header and CMAC are added.

## 4. Application message size

Default logical application-message limit:

```text
SECURE_TRANSPORT_MAX_MESSAGE_SIZE = 256 bytes
```

Maximum fragment count:

```text
ceil(256 / 7) = 37 fragments
```

The limit is compile-time configurable in `SecureTransport.h`, but increasing it requires RAM/queue/latency review.

## 5. Security behavior

Each fragment is independently processed by the existing `SecureProtocol`:

- unique per-peer TX counter,
- unique AES-CTR nonce,
- AES-128-CTR encryption,
- 8-byte AES-CMAC tag,
- anti-replay validation before reassembly.

Fragment metadata is inside the encrypted/authenticated payload. A forged or modified fragment cannot be accepted without passing CMAC.

## 6. Reassembly behavior

- One incomplete logical message is retained per source device.
- A new `messageId` from the same source replaces an older incomplete message.
- The application queue receives data only after all fragments are present.
- Missing fragments prevent delivery of that logical message.
- Hardware nRF24 Auto ACK/retries remain the first reliability layer.
- Whole-message application ACK/retry is not implemented yet.
- Arbitrary fragment reordering is not supported because SecureProtocol v1 accepts only monotonically increasing RX counters.

## 7. Device examples

`RadioTxMessage_t` and `ApplicationRxMessage_t` now contain:

```text
uint16_t payloadLength
uint8_t payload[256]
```

Device1 keeps the original 5-byte temperature message every second and additionally sends the 13-byte `Hello World !` heartbeat every 5 seconds. The heartbeat is automatically split into two fragments: 7 + 6 bytes.

Device2 reassembles the heartbeat before passing it to the application and exposes it in:

```text
g_lastHeartbeat
g_lastHeartbeatLength
```

for debugger inspection.

Task stack sizes were increased in the example to account for larger queue/local objects:

```text
RadioTask       1536 bytes
ApplicationTask 1024 bytes
```

Measure FreeRTOS stack high-water marks on the target before production use.

## 8. Test results

Host checks use GCC C11 with:

```text
-O2 -Wall -Wextra -Wpedantic -Werror
```

Compiled successfully:

- `tiny_aes.c`
- `Crypto.c`
- `DeviceKeys.c`
- `SecureProtocol.c`
- `SecureTransport.c`
- `nRF24L01.c` with HAL host stub
- Device1 application/storage sources
- Device2 application/storage sources

Runtime result:

```text
Tests: 346, failed: 0
```

New transport tests cover:

- direct <=11-byte message,
- `Hello World !` 13-byte fragmentation/reassembly,
- maximum 256-byte logical message,
- every generated RF frame <=32 bytes,
- missing-fragment behavior and recovery on the next message,
- rejection of >256-byte messages,
- CMAC rejection of a tampered fragment,
- acceptance of the original valid frame after failed authentication.

## 9. Remaining limitations

- Final STM32CubeIDE ARM build still requires the real CubeMX-generated projects/HAL configuration.
- Whole-message ACK/retry is not implemented; nRF24 Auto ACK operates per physical fragment.
- No timeout field is required for correctness in the current one-message-per-source reassembly policy; a newer message replaces an incomplete older one.
- SecureProtocol v1 does not support out-of-order frame acceptance.
- Queue payloads of 256 bytes increase RAM usage. Reduce `SECURE_TRANSPORT_MAX_MESSAGE_SIZE` if the application does not need it.
