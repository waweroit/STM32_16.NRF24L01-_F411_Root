#!/usr/bin/env sh
set -eu
CFLAGS="-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I../CommonLibrary/Inc -IHostStubs"
cc $CFLAGS test_secure_protocol.c ../CommonLibrary/Src/tiny_aes.c ../CommonLibrary/Src/Crypto.c ../CommonLibrary/Src/DeviceKeys.c ../CommonLibrary/Src/SecureProtocol.c ../CommonLibrary/Src/SecureTransport.c -o test_secure_protocol
cc $CFLAGS -DNRF24_HAL_HEADER='"stm32f4xx_hal.h"' -c ../CommonLibrary/Src/nRF24L01.c -o nRF24L01.o
cc $CFLAGS -I../Device1/Core/Inc -c ../Device1/Core/Src/device1_app.c -o device1_app.o
cc $CFLAGS -I../Device2/Core/Inc -c ../Device2/Core/Src/device2_app.c -o device2_app.o
./test_secure_protocol
