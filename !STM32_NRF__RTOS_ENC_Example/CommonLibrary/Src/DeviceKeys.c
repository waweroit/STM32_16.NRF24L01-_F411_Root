#include "DeviceKeys.h"
#include <string.h>

#define KDF_CONTEXT_VERSION 1u

/*
 * DEMONSTRATION KEYS ONLY.
 * Generate new independent 128-bit random keys before production use.
 */
static const uint8_t device1MasterKey[16] = {
    0x9D, 0x31, 0xC7, 0x4A, 0xE2, 0x68, 0x05, 0xB9,
    0x73, 0xF0, 0x1C, 0x86, 0xDA, 0x42, 0xAF, 0x57
};

static const uint8_t device2MasterKey[16] = {
    0x26, 0xE8, 0x91, 0x5F, 0xB4, 0x0D, 0xC3, 0x7A,
    0x18, 0xD6, 0x62, 0xAE, 0xF9, 0x34, 0x0B, 0xC5
};

bool DeviceKeys_GetMasterKey(uint8_t deviceId, uint8_t key[16])
{
    if (key == NULL) {
        return false;
    }
    if (deviceId == DEVICE_ID_1) {
        memcpy(key, device1MasterKey, sizeof(device1MasterKey));
        return true;
    }
    if (deviceId == DEVICE_ID_2) {
        memcpy(key, device2MasterKey, sizeof(device2MasterKey));
        return true;
    }
    return false;
}

CryptoStatus_t DeviceKeys_DeriveConnectionKey(const uint8_t masterKey[16],
                                               const char label[4],
                                               uint8_t sourceId,
                                               uint8_t destinationId,
                                               uint8_t derivedKey[16])
{
    uint8_t kdfInput[16] = {0};

    if (masterKey == NULL || label == NULL || derivedKey == NULL) {
        return CRYPTO_INVALID_ARGUMENT;
    }

    kdfInput[0] = (uint8_t)label[0];
    kdfInput[1] = (uint8_t)label[1];
    kdfInput[2] = (uint8_t)label[2];
    kdfInput[3] = (uint8_t)label[3];
    kdfInput[4] = sourceId;
    kdfInput[5] = destinationId;
    kdfInput[6] = KDF_CONTEXT_VERSION;       /* protocol/KDF context version */
    kdfInput[7] = 0x01u;                  /* first/only CMAC output block */
    kdfInput[15] = 0x80u;                 /* 128-bit output length marker */

    return Crypto_CalculateCmac(masterKey, kdfInput, sizeof(kdfInput),
                                derivedKey);
}
