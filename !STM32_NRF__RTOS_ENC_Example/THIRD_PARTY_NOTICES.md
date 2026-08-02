# Third-party notices

`CommonLibrary/Inc/tiny_aes.h` and `CommonLibrary/Src/tiny_aes.c` provide the API and AES-128 encryption/CTR path of **tiny-AES-c** by kokke and contributors.

- Project: tiny-AES-c
- Upstream: `kokke/tiny-AES-c`
- Reference release: `v1.0.0` (`e72b6ef`)
- License: The Unlicense / public domain
- Integration changes: header renamed to `tiny_aes.h`, CBC and AES decryption paths omitted because this project requires ECB block encryption for CMAC and CTR for payload encryption.

The AES mathematics and round operations are unchanged. Project-specific AES-CMAC, KDF, validation and error handling are implemented in `Crypto.c` and `DeviceKeys.c`.
