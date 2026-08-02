CC ?= gcc
CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I../CommonLibrary/Inc -IHostStubs
CORE := ../CommonLibrary/Src/tiny_aes.c ../CommonLibrary/Src/Crypto.c ../CommonLibrary/Src/DeviceKeys.c ../CommonLibrary/Src/SecureProtocol.c ../CommonLibrary/Src/SecureTransport.c
all: test_secure_protocol syntax_nrf syntax_devices

test_secure_protocol: test_secure_protocol.c $(CORE)
	$(CC) $(CFLAGS) $^ -o $@

syntax_nrf: ../CommonLibrary/Src/nRF24L01.c hal_stub.c
	$(CC) $(CFLAGS) -DNRF24_HAL_HEADER='"stm32f4xx_hal.h"' -c ../CommonLibrary/Src/nRF24L01.c -o nRF24L01.o
	$(CC) $(CFLAGS) -c hal_stub.c -o hal_stub.o

syntax_devices:
	$(CC) $(CFLAGS) -I../Device1/Core/Inc -c ../Device1/Core/Src/device1_app.c -o device1_app.o
	$(CC) $(CFLAGS) -I../Device1/Core/Inc -c ../Device1/Core/Src/secure_storage_port.c -o device1_storage.o
	$(CC) $(CFLAGS) -I../Device2/Core/Inc -c ../Device2/Core/Src/device2_app.c -o device2_app.o
	$(CC) $(CFLAGS) -I../Device2/Core/Inc -c ../Device2/Core/Src/secure_storage_port.c -o device2_storage.o

run: all
	./test_secure_protocol

clean:
	rm -f test_secure_protocol *.o
