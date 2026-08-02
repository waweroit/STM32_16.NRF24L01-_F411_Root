#include "secure_storage_port.h"
#include <stddef.h>

/*
 * Set to 1 only for a bench demonstration. Volatile state is lost after reset
 * and therefore must never be used for deployed AES-CTR communication.
 * Replace these functions with AT24C32, Flash EEPROM emulation or another
 * wear-managed persistent backend.
 */
#ifndef SECURE_DEMO_ALLOW_VOLATILE_STORAGE
#define SECURE_DEMO_ALLOW_VOLATILE_STORAGE 0
#endif

#if SECURE_DEMO_ALLOW_VOLATILE_STORAGE
static uint32_t demoBootCounter;
static uint32_t demoPeerSession;
static uint32_t demoPeerCounter;
static uint8_t demoPeerId;
static bool demoPeerValid;
#endif

static bool load_boot(uint32_t *counter) {
#if SECURE_DEMO_ALLOW_VOLATILE_STORAGE
    if (counter == NULL) return false;
    *counter = demoBootCounter;
    return true;
#else
    (void)counter;
    return false;
#endif
}
static bool save_boot(uint32_t counter) {
#if SECURE_DEMO_ALLOW_VOLATILE_STORAGE
    demoBootCounter = counter;
    return true;
#else
    (void)counter;
    return false;
#endif
}
static bool load_peer(uint8_t peerId, uint32_t *sessionId, uint32_t *lastCounter) {
#if SECURE_DEMO_ALLOW_VOLATILE_STORAGE
    if (!demoPeerValid || peerId != demoPeerId || sessionId == NULL || lastCounter == NULL) return false;
    *sessionId = demoPeerSession;
    *lastCounter = demoPeerCounter;
    return true;
#else
    (void)peerId; (void)sessionId; (void)lastCounter;
    return false;
#endif
}
static bool save_peer(uint8_t peerId, uint32_t sessionId, uint32_t lastCounter) {
#if SECURE_DEMO_ALLOW_VOLATILE_STORAGE
    demoPeerId = peerId; demoPeerSession = sessionId; demoPeerCounter = lastCounter; demoPeerValid = true;
    return true;
#else
    (void)peerId; (void)sessionId; (void)lastCounter;
    return false;
#endif
}
const SecureStorageInterface_t g_secureStorage = { load_boot, save_boot, load_peer, save_peer };
