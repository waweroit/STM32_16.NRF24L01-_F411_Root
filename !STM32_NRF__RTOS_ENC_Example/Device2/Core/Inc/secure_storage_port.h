#ifndef SECURE_STORAGE_PORT_H
#define SECURE_STORAGE_PORT_H
#include "SecureProtocol.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Storage interface passed to DeviceXApp_Initialize. */
extern const SecureStorageInterface_t g_secureStorage;
#ifdef __cplusplus
}
#endif
#endif
