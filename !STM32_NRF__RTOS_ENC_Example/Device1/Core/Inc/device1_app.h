#ifndef DEVICE1_APP_H
#define DEVICE1_APP_H
#include <stdbool.h>
#include <stdint.h>
#include "cmsis_os2.h"
#include "main.h"
#include "SecureProtocol.h"
#include "SecureTransport.h"
#include "nRF24L01.h"
#ifdef __cplusplus
extern "C" {
#endif
#define LOCAL_DEVICE_ID 0x01u

/** Logical temperature message. Fields are explicitly encoded; this struct is never transmitted directly. */
typedef struct __attribute__((packed)) {
    int16_t temperatureX10;
    uint16_t supplyVoltageMv;
    uint8_t statusFlags;
} TemperatureMessage_t;

typedef struct {
    uint8_t destinationId;
    SecureMessageType_t messageType;
    uint16_t payloadLength;
    uint8_t payload[SECURE_TRANSPORT_MAX_MESSAGE_SIZE];
} RadioTxMessage_t;
typedef struct {
    uint8_t sourceId;
    SecureMessageType_t messageType;
    uint16_t payloadLength;
    uint8_t payload[SECURE_TRANSPORT_MAX_MESSAGE_SIZE];
} ApplicationRxMessage_t;
typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cePort; uint16_t cePin;
    GPIO_TypeDef *csnPort; uint16_t csnPin;
    GPIO_TypeDef *ledPort; uint16_t ledPin;
    const SecureStorageInterface_t *storage;
    uint16_t adcEntropySample;
} Device1HardwareConfig_t;

extern osThreadId_t radioTaskHandle;
extern osThreadId_t applicationTaskHandle;
extern osMessageQueueId_t radioTxQueueHandle;
extern osMessageQueueId_t applicationRxQueueHandle;
extern osMutexId_t spiMutexHandle;

/** Initialize protocol state and create CMSIS-RTOS v2 objects. Kernel must already be initialized. */
bool Device1App_Initialize(const Device1HardwareConfig_t *config);
void Device1_RadioTask(void *argument);
void Device1_ApplicationTask(void *argument);
/** Optional weak diagnostic hook. Override in application code. */
void Device1_Log(const char *component, const char *statusText);
#ifdef __cplusplus
}
#endif
#endif
