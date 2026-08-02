# Host tests

`make run` builds the crypto/protocol suite and performs a standalone syntax compile of the nRF24L01+ driver using a minimal HAL stub. Device application sources can be syntax-checked with the command documented in `run_checks.sh`.

The host HAL is not a radio emulator and is never part of an STM32 build.

## Fragmentation tests

The host suite also verifies `SecureTransport`: direct <=11-byte messages, `Hello World !` split into two fragments, full 256-byte reassembly, missing-fragment behavior, size rejection and CMAC rejection of a tampered fragment.
