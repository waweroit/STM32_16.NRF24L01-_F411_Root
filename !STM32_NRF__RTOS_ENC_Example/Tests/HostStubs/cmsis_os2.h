#ifndef CMSIS_OS2_H
#define CMSIS_OS2_H
#include <stdint.h>
#include <stddef.h>
typedef void* osThreadId_t;typedef void* osMessageQueueId_t;typedef void* osMutexId_t;typedef int32_t osStatus_t;typedef int32_t osPriority_t;
#define osOK 0
#define osWaitForever 0xFFFFFFFFu
#define osPriorityNormal 24
#define osPriorityAboveNormal 32
typedef struct{const char*name;uint32_t attr_bits;void*cb_mem;uint32_t cb_size;}osMutexAttr_t;
typedef struct{const char*name;uint32_t attr_bits;void*cb_mem;uint32_t cb_size;void*mq_mem;uint32_t mq_size;}osMessageQueueAttr_t;
typedef struct{const char*name;uint32_t attr_bits;void*cb_mem;uint32_t cb_size;void*stack_mem;uint32_t stack_size;osPriority_t priority;uint32_t tz_module;uint32_t reserved;}osThreadAttr_t;
osMutexId_t osMutexNew(const osMutexAttr_t*);osStatus_t osMutexAcquire(osMutexId_t,uint32_t);osStatus_t osMutexRelease(osMutexId_t);
osMessageQueueId_t osMessageQueueNew(uint32_t,uint32_t,const osMessageQueueAttr_t*);osStatus_t osMessageQueueGet(osMessageQueueId_t,void*,uint8_t*,uint32_t);osStatus_t osMessageQueuePut(osMessageQueueId_t,const void*,uint8_t,uint32_t);
osThreadId_t osThreadNew(void(*)(void*),void*,const osThreadAttr_t*);osStatus_t osDelay(uint32_t);uint32_t osKernelGetTickCount(void);
#endif
