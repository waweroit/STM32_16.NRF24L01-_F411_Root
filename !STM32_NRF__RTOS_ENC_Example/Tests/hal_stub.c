#include "stm32f4xx_hal.h"
static uint32_t tick;
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef*a,uint8_t*b,uint8_t*c,uint16_t d,uint32_t e){(void)a;(void)b;(void)c;(void)d;(void)e;return HAL_OK;}
void HAL_GPIO_WritePin(GPIO_TypeDef*a,uint16_t b,GPIO_PinState c){(void)a;(void)b;(void)c;}
void HAL_Delay(uint32_t d){tick+=d;}
uint32_t HAL_GetTick(void){return tick++;}
uint32_t HAL_GetUIDw0(void){return 0x12345678u;}
uint32_t HAL_GetUIDw1(void){return 0x9abcdef0u;}
uint32_t HAL_GetUIDw2(void){return 0x0badc0deu;}
