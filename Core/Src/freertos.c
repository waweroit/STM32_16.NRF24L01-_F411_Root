/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"
#include "string.h"
#include "semphr.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define DBG_TX_BUFFER_SIZE  256
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

extern uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
extern uint8_t recvDone;
extern uint32_t recvLen;

volatile uint8_t ToggleLed = 1;

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for CommunicationTa */
osThreadId_t CommunicationTaHandle;
const osThreadAttr_t CommunicationTa_attributes = {
  .name = "CommunicationTa",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for ReadSensorsTask */
osThreadId_t ReadSensorsTaskHandle;
const osThreadAttr_t ReadSensorsTask_attributes = {
  .name = "ReadSensorsTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* Deklaracja zewnętrznej struktury USB z biblioteki ST (wygenerowanej przez CubeMX) */
extern USBD_HandleTypeDef hUsbDeviceFS;
void Debug(const char *msg);
void SendToUSB(uint8_t* Buf, uint16_t Len);

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartCommunicationTask(void *argument);
void StartReadSensorsTask(void *argument);

extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of CommunicationTa */
  CommunicationTaHandle = osThreadNew(StartCommunicationTask, NULL, &CommunicationTa_attributes);

  /* creation of ReadSensorsTask */
  ReadSensorsTaskHandle = osThreadNew(StartReadSensorsTask, NULL, &ReadSensorsTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN StartDefaultTask */
  Debug("Welcome to my world :D ...\r\n");
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartCommunicationTask */
/**
* @brief Function implementing the CommunicationTa thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartCommunicationTask */
void StartCommunicationTask(void *argument)
{
  /* USER CODE BEGIN StartCommunicationTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartCommunicationTask */
}

/* USER CODE BEGIN Header_StartReadSensorsTask */
/**
* @brief Function implementing the ReadSensorsTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartReadSensorsTask */
void StartReadSensorsTask(void *argument)
{
  /* USER CODE BEGIN StartReadSensorsTask */
  /* Infinite loop */
  for(;;)
  {
	if (ToggleLed == 1)
	{
		HAL_GPIO_TogglePin(BLUE_LED_GPIO_Port, BLUE_LED_Pin);
		vTaskDelay(pdMS_TO_TICKS(500));
	}
  }
  /* USER CODE END StartReadSensorsTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void Debug(const char *msg)
{
    if (msg == NULL) {
        return;
    }
        // Wywołanie zewnętrznej, bezpiecznej funkcji USB CDC
    SendToUSB((uint8_t*)msg, (uint16_t)strlen(msg));
}

void SendToUSB(uint8_t* Buf, uint16_t Len)
{
    // 1. Zabezpieczenie przed błędnymi parametrami
    if (Buf == NULL || Len == 0) {
        return;
    }

    // 2. Statyczne zmienne lokalne - widoczne TYLKO w tej funkcji, bezpieczne dla pamięci
    static uint8_t txBuffer[DBG_TX_BUFFER_SIZE];
    static SemaphoreHandle_t usbDebugMutex = NULL;

    // 3. Leniwa inicjalizacja Mutexu przy pierwszym wywołaniu funkcji
    if (usbDebugMutex == NULL) {
        usbDebugMutex = xSemaphoreCreateMutex();
        if (usbDebugMutex == NULL) {
            return; // Jeśli brakuje pamięci na Mutex, wychodzimy
        }
    }

    // 4. Pobranie Mutexu - blokuje inne Taski przed jednoczesnym dostępem do txBuffer i USB
    if (xSemaphoreTake(usbDebugMutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        return; // Mutex zajęty przez zbyt długi czas, pomijamy ten log
    }

    // 5. Pobranie wskaźnika do stanu kontrolera USB
    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*) hUsbDeviceFS.pClassData;
    if (hcdc == NULL) {
        xSemaphoreGive(usbDebugMutex);
        return; // Sterownik USB nie jest jeszcze gotowy
    }

    // 6. Jeśli kontroler USB wysyła coś w tle, czekamy w sposób nieblokujący CPU
    while (hcdc->TxState != 0)
    {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    // 7. Dopasowanie długości danych do rozmiaru naszego bufora zabezpieczającego
    uint16_t bytesToCopy = (Len < DBG_TX_BUFFER_SIZE) ? Len : (DBG_TX_BUFFER_SIZE - 1);

    // 8. Kopiujemy dane do bezpiecznego txBuffer (gwarancja stałości danych podczas nadawania DMA)
    memcpy(txBuffer, Buf, bytesToCopy);

    // 9. Uruchomienie transmisji przez hardware USB
    if (CDC_Transmit_FS(txBuffer, bytesToCopy) == USBD_OK)
    {
        // 10. Czekamy, aż dane opuszczą bufor, zanim zwolnimy Mutex i pozwolimy nadpisać txBuffer
        while (hcdc->TxState != 0)
        {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    // 11. Zwolnienie Mutexu - inny Task może teraz bezpiecznie logować
    xSemaphoreGive(usbDebugMutex);
}

/* USER CODE END Application */

