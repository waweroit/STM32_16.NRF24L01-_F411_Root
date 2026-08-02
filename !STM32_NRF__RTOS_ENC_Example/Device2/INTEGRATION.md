# Device2 STM32CubeMX integration

## MCU and clock

- MCU: STM32F411CEU6
- SYSCLK: 100 MHz
- Enable instruction/data caches according to the generated F4 project defaults.

## SPI1

| Signal | Pin |
|---|---|
| SCK | PA5 |
| MISO | PA6 |
| MOSI | PA7 |

Settings: Master, full duplex, 8-bit, MSB first, CPOL Low, CPHA 1 Edge, software NSS. The nRF24L01+ SPI limit is 10 MHz; with a 100 MHz system clock and typical APB2/SPI1 clocking, choose a prescaler that stays at or below 10 MHz (for example `/16` if SPI1 receives 100 MHz).

## GPIO

| Signal | Example pin | Mode |
|---|---|---|
| CE | PB0 | output push-pull, initial Low |
| CSN | PB1 | output push-pull, initial High |
| IRQ | PB2 | input or falling-edge EXTI (optional) |
| LED | board-specific | output push-pull |

Change these pins in the `Device2HardwareConfig_t` instance to match the real PCB. The driver has no hard-coded GPIO ports.

## FreeRTOS

Enable FreeRTOS with CMSIS-RTOS v2. The example creates its own objects after `osKernelInitialize()` and before `osKernelStart()`:

```c
#include "device2_app.h"
#include "secure_storage_port.h"

Device2HardwareConfig_t cfg = {
    .hspi = &hspi1,
    .cePort = GPIOB, .cePin = GPIO_PIN_0,
    .csnPort = GPIOB, .csnPin = GPIO_PIN_1,
    .ledPort = LED_GPIO_Port, .ledPin = LED_Pin,
    .storage = &g_secureStorage,
    .adcEntropySample = adc_noise_sample
};

if (!Device2App_Initialize(&cfg)) {
    Error_Handler();
}
```

Do not also create CubeMX tasks with the same purpose. Alternatively, remove the internal `osThreadNew` calls and assign `Device2_RadioTask` / `Device2_ApplicationTask` as CubeMX task entry functions.

Recommended initial stack sizes are 768 bytes for `RadioTask` and 512 bytes for `ApplicationTask`; confirm with FreeRTOS stack watermark measurement.

## Storage

Replace `secure_storage_port.c` with a persistent implementation. The callbacks must be callable before the scheduler starts. Save the incremented boot counter atomically before creating the radio tasks. For AT24C32, use redundant records with magic, version, sequence and CRC, and verify the write by reading it back.

The optional peer session callbacks should preserve `(peerId, sessionId, lastCounter)`. They are written infrequently; do not erase internal Flash for every packet.

## Debug logging

Override the weak `Device2_Log` function. It can enqueue a diagnostic record for a UART task. Avoid blocking UART transmission from `RadioTask`. The library itself does not depend on `printf`.

## nRF wiring/power

Use 3.3 V only. Place a 100 nF ceramic capacitor and approximately 10–47 µF bulk capacitor directly at the module. Long jumper wires and weak 3.3 V regulators commonly cause `MAX_RT` and corrupted status reads.

## SecureTransport / fragmentation update

Also compile `CommonLibrary/Src/SecureTransport.c` and include `SecureTransport.h`.
Application queue items now use `uint16_t payloadLength` and `payload[SECURE_TRANSPORT_MAX_MESSAGE_SIZE]` (default 256 bytes). The radio task owns `SecureTransportContext_t`; messages longer than 11 bytes are fragmented automatically and are queued to the application only after complete reassembly.

Because the queue items and local task variables are larger, the example task stack sizes were increased to 1536 bytes for RadioTask and 1024 bytes for ApplicationTask. Measure the FreeRTOS high-water marks on the target and tune them for the real application.
