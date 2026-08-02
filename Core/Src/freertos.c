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
#include "spi.h"
#include "nRF24L01.h"
#include "SecureProtocol.h"
#include "SecureTransport.h"
#include "DeviceKeys.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define DBG_TX_BUFFER_SIZE  256

#define LOCAL_DEVICE_ID            0x01u
#define DEVICE_2_ID                0x02u
#define NRF_TX_QUEUE_DEPTH         3u
#define NRF_RX_QUEUE_DEPTH         3u
#define NRF_SEND_TIMEOUT_MS        30u
#define DEVICE1_DEMO_BOOT_COUNTER  1u

typedef struct
{
    uint8_t destinationId;
    SecureMessageType_t messageType;
    uint16_t payloadLength;
    uint8_t payload[SECURE_TRANSPORT_MAX_MESSAGE_SIZE];
} NrfTxMessage_t;

typedef struct
{
    uint8_t sourceId;
    SecureMessageType_t messageType;
    uint16_t payloadLength;
    uint8_t payload[SECURE_TRANSPORT_MAX_MESSAGE_SIZE];
} NrfRxMessage_t;
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

osMessageQueueId_t nrfTxQueueHandle;
osMessageQueueId_t applicationRxQueueHandle;

volatile uint32_t nrfTxMessageSuccessCount = 0u;
volatile uint32_t nrfTxMessageErrorCount = 0u;
volatile uint32_t nrfTxFragmentSuccessCount = 0u;
volatile uint32_t nrfTxFragmentErrorCount = 0u;
volatile uint32_t nrfRxFragmentCount = 0u;
volatile uint32_t nrfRxMessageCount = 0u;
volatile uint32_t secureAuthErrorCount = 0u;
volatile uint32_t secureReplayErrorCount = 0u;
volatile uint32_t device1SessionId = 0u;

static NRF24_Handle_t nrfRadio;
static SecureProtocolContext_t secureProtocol;
static SecureTransportContext_t secureTransport;
static uint8_t communicationInitialized = 0u;

static const uint8_t device1RadioAddress[5] = {0xE7u, 0xE7u, 0xE7u, 0xE7u, 0x01u};
static const uint8_t device2RadioAddress[5] = {0xE7u, 0xE7u, 0xE7u, 0xE7u, 0x02u};

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
static uint8_t Device1Communication_Init(void);
static void Communication_PollRx(void);
static void Communication_TransmitMessage(const NrfTxMessage_t *message);
static void Communication_RecordProtocolError(SecureProtocolStatus_t status);
static uint8_t Communication_RestoreRx(void);

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
  nrfTxQueueHandle = osMessageQueueNew(NRF_TX_QUEUE_DEPTH,
                                       sizeof(NrfTxMessage_t),
                                       NULL);
  applicationRxQueueHandle = osMessageQueueNew(NRF_RX_QUEUE_DEPTH,
                                                sizeof(NrfRxMessage_t),
                                                NULL);

  if (nrfTxQueueHandle != NULL && applicationRxQueueHandle != NULL)
  {
    communicationInitialized = Device1Communication_Init();
  }
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
  NrfTxMessage_t txMessage;
  (void)argument;

  if (communicationInitialized == 0u)
  {
    Debug("Communication init failed\r\n");
    for (;;)
    {
      osDelay(1000u);
    }
  }

  for (;;)
  {
    /* RX is serviced first so the radio spends the normal state listening. */
    Communication_PollRx();

    if (osMessageQueueGet(nrfTxQueueHandle, &txMessage, NULL, 0u) == osOK)
    {
      Communication_TransmitMessage(&txMessage);
    }

    osDelay(1u);
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
  static const uint8_t helloMessage[] =
  {
    'H','e','l','l','o',' ',
    'W','o','r','l','d',' ','!'
  };
  NrfTxMessage_t message;
  (void)argument;

  osDelay(5000u);

  for (;;)
  {
    memset(&message, 0, sizeof(message));
    message.destinationId = DEVICE_2_ID;
    message.messageType = MESSAGE_TYPE_HEARTBEAT;
    message.payloadLength = (uint16_t)sizeof(helloMessage);
    memcpy(message.payload, helloMessage, sizeof(helloMessage));

    if (nrfTxQueueHandle == NULL ||
        osMessageQueuePut(nrfTxQueueHandle, &message, 0u, 0u) != osOK)
    {
      Debug("NRF TX queue full/error\r\n");
    }

    osDelay(5000u);
  }
  /* USER CODE END StartReadSensorsTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
static uint8_t Device1Communication_Init(void)
{
    uint8_t localKey[CRYPTO_AES_KEY_SIZE];
    uint8_t peerKey[CRYPTO_AES_KEY_SIZE];
    uint32_t uniqueId[3];
    uint32_t sessionId = 0u;
    uint16_t entropySample;
    SecureProtocolStatus_t protocolStatus;
    SecureTransportStatus_t transportStatus;
    NRF24_Status_t radioStatus;

    if (!DeviceKeys_GetMasterKey(LOCAL_DEVICE_ID, localKey) ||
        !DeviceKeys_GetMasterKey(DEVICE_2_ID, peerKey))
    {
        return 0u;
    }

    uniqueId[0] = HAL_GetUIDw0();
    uniqueId[1] = HAL_GetUIDw1();
    uniqueId[2] = HAL_GetUIDw2();
    entropySample = (uint16_t)(uniqueId[0] ^ uniqueId[1] ^ uniqueId[2] ^ HAL_GetTick());

    /*
     * Bench-session generation. For deployment, DEVICE1_DEMO_BOOT_COUNTER must
     * be replaced by a monotonic boot generation stored in persistent memory.
     */
    protocolStatus = SecureProtocol_GenerateSessionId(localKey,
                                                       uniqueId,
                                                       DEVICE1_DEMO_BOOT_COUNTER,
                                                       HAL_GetTick(),
                                                       entropySample,
                                                       &sessionId);
    if (protocolStatus != SECURE_PROTOCOL_OK)
    {
        return 0u;
    }

    device1SessionId = sessionId;

    protocolStatus = SecureProtocol_Init(&secureProtocol,
                                         LOCAL_DEVICE_ID,
                                         sessionId,
                                         localKey);
    if (protocolStatus != SECURE_PROTOCOL_OK)
    {
        return 0u;
    }

    protocolStatus = SecureProtocol_AddPeer(&secureProtocol,
                                            DEVICE_2_ID,
                                            peerKey);
    if (protocolStatus != SECURE_PROTOCOL_OK)
    {
        return 0u;
    }

    transportStatus = SecureTransport_Init(&secureTransport, &secureProtocol);
    if (transportStatus != SECURE_TRANSPORT_OK)
    {
        return 0u;
    }

    radioStatus = NRF24_Init(&nrfRadio,
                             &hspi1,
                             NRF24_CE_GPIO_Port,
                             NRF24_CE_Pin,
                             SPI1_NRF24_CS_GPIO_Port,
                             SPI1_NRF24_CS_Pin);
    if (radioStatus != NRF24_OK)
    {
        return 0u;
    }

    radioStatus = NRF24_SetRxAddress(&nrfRadio, 0u, device1RadioAddress);
    if (radioStatus == NRF24_OK)
    {
        radioStatus = NRF24_StartListening(&nrfRadio);
    }

    return (radioStatus == NRF24_OK) ? 1u : 0u;
}

static void Communication_RecordProtocolError(SecureProtocolStatus_t status)
{
    if (status == SECURE_PROTOCOL_AUTHENTICATION_FAILED)
    {
        ++secureAuthErrorCount;
    }
    else if (status == SECURE_PROTOCOL_REPLAY_DETECTED)
    {
        ++secureReplayErrorCount;
    }

    Debug("SecureProtocol RX error: ");
    Debug(SecureProtocol_StatusToString(status));
    Debug("\r\n");
}

static uint8_t Communication_RestoreRx(void)
{
    NRF24_Status_t radioStatus;

    radioStatus = NRF24_SetRxAddress(&nrfRadio, 0u, device1RadioAddress);
    if (radioStatus == NRF24_OK)
    {
        radioStatus = NRF24_StartListening(&nrfRadio);
    }

    if (radioStatus != NRF24_OK)
    {
        Debug("NRF RX restore failed: ");
        Debug(NRF24_StatusToString(radioStatus));
        Debug("\r\n");
        return 0u;
    }

    return 1u;
}

static void Communication_TransmitMessage(const NrfTxMessage_t *message)
{
    SecureTransportTxState_t txState;
    SecureTransportStatus_t transportStatus;
    NRF24_Status_t radioStatus;
    uint8_t messageComplete = 0u;
    uint8_t messageSucceeded = 1u;

    if (message == NULL ||
        message->destinationId != DEVICE_2_ID ||
        message->payloadLength > SECURE_TRANSPORT_MAX_MESSAGE_SIZE)
    {
        ++nrfTxMessageErrorCount;
        return;
    }

    transportStatus = SecureTransport_BeginMessage(&secureTransport,
                                                    message->destinationId,
                                                    message->messageType,
                                                    message->payload,
                                                    message->payloadLength,
                                                    &txState);
    if (transportStatus != SECURE_TRANSPORT_OK)
    {
        ++nrfTxMessageErrorCount;
        Debug("SecureTransport TX begin failed\r\n");
        return;
    }

    radioStatus = NRF24_StopListening(&nrfRadio);
    if (radioStatus == NRF24_OK)
    {
        radioStatus = NRF24_SetTxAddress(&nrfRadio, device2RadioAddress);
    }
    if (radioStatus == NRF24_OK)
    {
        /* Pipe 0 must match TX_ADDR while nRF24 Auto ACK is enabled. */
        radioStatus = NRF24_SetRxAddress(&nrfRadio, 0u, device2RadioAddress);
    }

    if (radioStatus != NRF24_OK)
    {
        ++nrfTxMessageErrorCount;
        (void)Communication_RestoreRx();
        Debug("NRF TX setup failed\r\n");
        return;
    }

    while (messageComplete == 0u)
    {
        uint8_t frame[SECURE_MAX_FRAME_SIZE];
        uint8_t frameLength = 0u;
        bool secureMessageComplete = false;

        transportStatus = SecureTransport_CreateNextFrame(&secureTransport,
                                                           &txState,
                                                           frame,
                                                           (uint8_t)sizeof(frame),
                                                           &frameLength,
                                                           &secureMessageComplete);
        if (transportStatus != SECURE_TRANSPORT_OK)
        {
            messageSucceeded = 0u;
            if (transportStatus == SECURE_TRANSPORT_PROTOCOL_ERROR)
            {
                SecureProtocolStatus_t protocolError =
                    SecureTransport_GetLastProtocolStatus(&secureTransport);
                Debug("SecureProtocol TX error: ");
                Debug(SecureProtocol_StatusToString(protocolError));
                Debug("\r\n");
            }
            else
            {
                Debug("SecureTransport TX frame failed\r\n");
            }
            break;
        }

        if (frameLength == 0u || frameLength > NRF24_MAX_PAYLOAD_SIZE)
        {
            ++nrfTxFragmentErrorCount;
            messageSucceeded = 0u;
            Debug("Invalid NRF frame length\r\n");
            break;
        }

        radioStatus = NRF24_Send(&nrfRadio,
                                 frame,
                                 frameLength,
                                 NRF_SEND_TIMEOUT_MS);
        if (radioStatus != NRF24_OK)
        {
            ++nrfTxFragmentErrorCount;
            messageSucceeded = 0u;
            Debug("NRF TX fragment failed: ");
            Debug(NRF24_StatusToString(radioStatus));
            Debug("\r\n");
            break;
        }

        ++nrfTxFragmentSuccessCount;
        messageComplete = secureMessageComplete ? 1u : 0u;
    }

    if (Communication_RestoreRx() == 0u)
    {
        messageSucceeded = 0u;
    }

    if (messageSucceeded != 0u && messageComplete != 0u)
    {
        ++nrfTxMessageSuccessCount;
    }
    else
    {
        ++nrfTxMessageErrorCount;
    }
}

static void Communication_PollRx(void)
{
    uint8_t frame[SECURE_MAX_FRAME_SIZE];
    uint8_t frameLength = 0u;
    uint16_t payloadLength = 0u;
    uint8_t sourceId = 0u;
    SecureMessageType_t messageType = MESSAGE_TYPE_NONE;
    SecureTransportStatus_t transportStatus;
    NRF24_Status_t radioStatus;
    NrfRxMessage_t message;

    radioStatus = NRF24_Receive(&nrfRadio,
                                frame,
                                (uint8_t)sizeof(frame),
                                &frameLength);
    if (radioStatus != NRF24_OK)
    {
        Debug("NRF RX failed: ");
        Debug(NRF24_StatusToString(radioStatus));
        Debug("\r\n");
        return;
    }

    if (frameLength == 0u)
    {
        return;
    }

    ++nrfRxFragmentCount;
    memset(&message, 0, sizeof(message));

    /* SecureTransport invokes SecureProtocol first and only reassembles
       authenticated, destination/session-checked and anti-replay-safe data. */
    transportStatus = SecureTransport_ProcessFrame(&secureTransport,
                                                    frame,
                                                    frameLength,
                                                    &messageType,
                                                    message.payload,
                                                    sizeof(message.payload),
                                                    &payloadLength,
                                                    &sourceId);
    if (transportStatus == SECURE_TRANSPORT_IN_PROGRESS)
    {
        return;
    }

    if (transportStatus != SECURE_TRANSPORT_OK)
    {
        if (transportStatus == SECURE_TRANSPORT_PROTOCOL_ERROR)
        {
            Communication_RecordProtocolError(
                SecureTransport_GetLastProtocolStatus(&secureTransport));
        }
        else
        {
            Debug("SecureTransport RX error: ");
            Debug(SecureTransport_StatusToString(transportStatus));
            Debug("\r\n");
        }
        return;
    }

    message.sourceId = sourceId;
    message.messageType = messageType;
    message.payloadLength = payloadLength;
    ++nrfRxMessageCount;

    if (osMessageQueuePut(applicationRxQueueHandle, &message, 0u, 0u) != osOK)
    {
        Debug("Application RX queue full/error\r\n");
    }
}

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

