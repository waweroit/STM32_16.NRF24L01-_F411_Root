# Secure protocol description

## 1. On-air frame

The nRF24L01+ dynamic payload contains exactly `21 + payloadLength` bytes. No C structure is cast to bytes.

| Offset | Size | Field | Encoding |
|---:|---:|---|---|
| 0 | 1 | `version` | `0x01` |
| 1 | 1 | `sourceId` | unsigned byte |
| 2 | 1 | `destinationId` | unsigned byte |
| 3 | 1 | `messageType` | authenticated enum value |
| 4 | 4 | `sessionId` | big-endian |
| 8 | 4 | `messageCounter` | big-endian |
| 12 | 1 | `payloadLength` | `0..11` |
| 13 | 0..11 | encrypted payload | AES-128-CTR ciphertext |
| 13+N | 8 | authentication tag | first 8 bytes of AES-CMAC |

The fixed authenticated header is 13 bytes. The tag is 8 bytes. Therefore `13 + 11 + 8 = 32`, which is the nRF24L01+ maximum. A 12-byte application payload would produce 33 bytes and is rejected.

The authentication input is the exact serialized byte sequence from offset 0 through the final ciphertext byte. The tag itself is not included. The nRF hardware CRC remains enabled for accidental RF corruption; it is not treated as authentication.

## 1.1 Logical-message fragmentation (`SecureTransport`)

The 32-byte SecureProtocol frame remains unchanged. `SecureTransport` sits above it and permits logical application messages up to **256 bytes** by creating multiple SecureProtocol frames.

Messages with length `0..11` use the original message type and are not fragmented. Messages longer than 11 bytes are emitted as `MESSAGE_TYPE_FRAGMENT (0x7F)`. The plaintext inside each fragment frame is:

| Offset | Size | Field | Encoding |
|---:|---:|---|---|
| 0 | 1 | `messageId` | nonzero transport sequence, wraps after 255 |
| 1 | 1 | `originalMessageType` | application type being transported |
| 2 | 1 | `fragmentIndex` | zero-based |
| 3 | 1 | `fragmentCount` | total number of fragments |
| 4 | 1..7 | fragment data | application bytes |

The 4-byte fragment header is encrypted and authenticated because it is part of the SecureProtocol plaintext. A full fragment therefore uses all 11 plaintext bytes and produces a 32-byte nRF packet.

With the default `SECURE_TRANSPORT_MAX_MESSAGE_SIZE = 256`, at most 37 fragments are needed because each fragment carries up to 7 application bytes. Example: `Hello World !` is 13 bytes and is transported as 2 fragments containing 7 and 6 application bytes.

Each fragment independently consumes a SecureProtocol TX counter and therefore gets a unique CTR nonce and its own 8-byte CMAC tag. A fragmented logical message is delivered to the application only after all fragments have been accepted and reassembled. If a fragment is missing, the partial message is not delivered. A subsequent new `messageId` from the same source replaces the older incomplete reassembly.

## 2. Endianness and serialization

All multibyte protocol integers are serialized explicitly in network order (big-endian). Application messages are also encoded field by field. Structure padding, compiler ABI and MCU endianness therefore do not affect the radio format.

Temperature payload (`MESSAGE_TYPE_TEMPERATURE`, 5 bytes):

| Offset | Size | Meaning |
|---:|---:|---|
| 0 | 2 | signed `temperatureX10`, big-endian |
| 2 | 2 | `supplyVoltageMv`, big-endian |
| 4 | 1 | `statusFlags` |

`240` means `24.0 °C`.

## 3. Directional keys

Each sender owns a unique 16-byte master key. A receiver stores the master key of every authorized sender. Keys are never sent by radio.

For a frame `sourceId -> destinationId`, two independent keys are derived:

```text
Kenc  = AES-CMAC(senderMasterKey, "ENC\0"  || sourceId || destinationId || protocol context)
Kauth = AES-CMAC(senderMasterKey, "AUTH" || sourceId || destinationId || protocol context)
```

The KDF input is a fixed 16-byte domain-separated block implemented by `DeviceKeys_DeriveConnectionKey`. `Kenc` is used only for CTR and `Kauth` only for CMAC.

## 4. CTR nonce/counter block

```text
Byte 0      protocol version
Byte 1      source ID
Byte 2      destination ID
Byte 3      direction/domain byte (0 for data frame)
Byte 4..7   session ID, big-endian
Byte 8..11  message counter, big-endian
Byte 12..15 AES block counter, initially 0
```

`tiny-AES-c` increments the 128-bit counter between AES blocks. Since the application payload is at most 11 bytes, one keystream block is currently sufficient, but the API also supports longer buffers outside this radio protocol.

The same `(Kenc, nonceCounter)` pair must never be reused. The TX counter is consumed when a frame is created, even if the subsequent RF transmission fails. Hardware Auto ACK retransmits the same already-built frame, which is safe. An application-level retry creates a new frame and consumes a new counter.

## 5. Session IDs and restart behavior

`SecureProtocol_GenerateSessionId` uses:

- the STM32 96-bit unique device ID,
- a persistent boot counter,
- a timer value,
- an ADC entropy sample,
- AES-CMAC under the local master key.

The 32-bit value has this form:

```text
bits 31..8: persistent boot generation (24 bits)
bits 7..0 : CMAC-derived session salt
```

The monotonic boot generation is the uniqueness anchor. Timer and ADC values diversify the low byte but are not trusted as the sole entropy source. `HAL_GetTick()` alone is not random and is insufficient.

The boot counter must be incremented and saved before radio communication starts. Values above `0x00FFFFFF` are rejected and require reprovisioning/key rotation.

## 6. TX processing

1. Find the peer context.
2. Reject an exhausted `UINT32_MAX` TX counter.
3. Derive `Kenc` and `Kauth` from the local sender master key.
4. Build the nonce from IDs, session and current counter.
5. Encrypt the application bytes with AES-CTR.
6. Serialize the authenticated header and ciphertext.
7. Calculate the 8-byte truncated CMAC.
8. Serialize the final frame.
9. Consume/increment the TX counter.
10. Pass the frame to the nRF driver.

## 7. RX processing and anti-replay

1. Validate physical frame length and payload length.
2. Validate protocol version and destination.
3. Find the known sender and its master key.
4. Compare the session generation with the persisted/current peer state.
5. For the same session, reject `counter <= lastAcceptedRxCounter`.
6. Derive the sender-direction keys.
7. Verify CMAC in constant time.
8. Only after successful authentication, decrypt the payload.
9. Only after complete success, update accepted session/counter state.

Lost packets are allowed because the rule is `receivedCounter > lastAcceptedRxCounter`, not exact increment-by-one.

A session with a lower generation is rejected. A higher authenticated generation is accepted. A different salt with the same generation is rejected. Persist the peer session whenever it changes. The examples checkpoint the RX counter every 256 accepted frames; after a receiver power loss, up to 255 already-seen frames could otherwise be replayed. Tighten the interval or use an EEPROM-backed replay window for higher assurance.

Out-of-order delivery is not supported in version 1. A future version can replace the single last counter with a high-water mark and bitmap replay window.

## 8. Session/storage policy

The `SecureStorageInterface_t` abstraction supports a boot counter and peer replay state. Production choices include:

- AT24C32/external EEPROM with CRC and redundant records,
- wear-levelled Flash EEPROM emulation,
- reserved counter ranges,
- periodic checkpoints,
- FRAM.

Do not write internal Flash after every transmitted packet. The preferred model is persistent boot generation plus RAM TX counters, because a new boot generation prevents CTR nonce reuse after restart.
