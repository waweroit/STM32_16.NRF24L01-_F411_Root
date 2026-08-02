#ifndef DEVICE_KEYS_H
#define DEVICE_KEYS_H

#include <stdbool.h>
#include <stdint.h>
#include "Crypto.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEVICE_ID_1 0x01u
#define DEVICE_ID_2 0x02u

/**
 * @brief Copy a demonstration master key for an authorized device.
 * @warning Replace every demonstration key before production deployment.
 */
bool DeviceKeys_GetMasterKey(uint8_t deviceId,
                             uint8_t key[CRYPTO_AES_KEY_SIZE]);

/**
 * @brief Derive a full 128-bit directional key with AES-CMAC.
 * @param label Four-byte domain label, normally "ENC" or "AUTH" including NUL.
 */
CryptoStatus_t DeviceKeys_DeriveConnectionKey(
    const uint8_t masterKey[CRYPTO_AES_KEY_SIZE],
    const char label[4],
    uint8_t sourceId,
    uint8_t destinationId,
    uint8_t derivedKey[CRYPTO_AES_KEY_SIZE]);

#ifdef __cplusplus
}
#endif
#endif
