#ifndef STM32F4XX_HAL_H
#define STM32F4XX_HAL_H
#include <stdint.h>
typedef struct { int unused; } SPI_HandleTypeDef;
typedef struct { int unused; } GPIO_TypeDef;
typedef enum { GPIO_PIN_RESET=0,GPIO_PIN_SET=1 } GPIO_PinState;
typedef enum { HAL_OK=0,HAL_ERROR=1,HAL_BUSY=2,HAL_TIMEOUT=3 } HAL_StatusTypeDef;
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef*,uint8_t*,uint8_t*,uint16_t,uint32_t);
void HAL_GPIO_WritePin(GPIO_TypeDef*,uint16_t,GPIO_PinState);
void HAL_Delay(uint32_t);
uint32_t HAL_GetTick(void);
#endif
