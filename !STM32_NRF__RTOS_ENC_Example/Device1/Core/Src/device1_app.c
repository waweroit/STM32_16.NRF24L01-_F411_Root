#include "device1_app.h"
#include "DeviceKeys.h"
#include <string.h>

#define REMOTE_DEVICE_ID DEVICE_ID_2
#define RX_CHECKPOINT_INTERVAL 256u
#define RADIO_SEND_TIMEOUT_MS 30u

static const uint8_t ownAddress[5] = {0xE7, 0xE7, 0xE7, 0xE7, 0x01};
static const uint8_t peerAddress[5] = {0xE7, 0xE7, 0xE7, 0xE7, 0x02};

static Device1HardwareConfig_t hardware;
static NRF24_Handle_t radio;
static SecureProtocolContext_t protocol;
static SecureTransportContext_t transport;
static uint32_t lastStoredRxSession;

osThreadId_t radioTaskHandle;
osThreadId_t applicationTaskHandle;
osMessageQueueId_t radioTxQueueHandle;
osMessageQueueId_t applicationRxQueueHandle;
osMutexId_t spiMutexHandle;

__attribute__((weak)) void Device1_Log(const char *component,
                                       const char *statusText)
{
    (void)component;
    (void)statusText;
}

static void put_i16_be(uint8_t *buffer, int16_t value)
{
    uint16_t unsignedValue = (uint16_t)value;
    buffer[0] = (uint8_t)(unsignedValue >> 8);
    buffer[1] = (uint8_t)unsignedValue;
}

static void put_u16_be(uint8_t *buffer, uint16_t value)
{
    buffer[0] = (uint8_t)(value >> 8);
    buffer[1] = (uint8_t)value;
}

static SecurePeerContext_t *get_remote_peer(void)
{
    size_t i;
    for (i = 0u; i < SECURE_MAX_PEERS; ++i) {
        if (protocol.peers[i].inUse &&
            protocol.peers[i].remoteDeviceId == REMOTE_DEVICE_ID) {
            return &protocol.peers[i];
        }
    }
    return NULL;
}

static bool create_local_session(const uint8_t masterKey[16], uint32_t *sessionId)
{
    uint32_t bootCounter;
    uint32_t uniqueId[3];

    if (hardware.storage == NULL ||
        hardware.storage->LoadBootCounter == NULL ||
        hardware.storage->SaveBootCounter == NULL) {
        return false;
    }
    if (!hardware.storage->LoadBootCounter(&bootCounter) ||
        bootCounter == UINT32_MAX) {
        return false;
    }

    ++bootCounter;
    if (!hardware.storage->SaveBootCounter(bootCounter)) {
        return false;
    }

    uniqueId[0] = HAL_GetUIDw0();
    uniqueId[1] = HAL_GetUIDw1();
    uniqueId[2] = HAL_GetUIDw2();
    return SecureProtocol_GenerateSessionId(masterKey,
                                            uniqueId,
                                            bootCounter,
                                            HAL_GetTick(),
                                            hardware.adcEntropySample,
                                            sessionId) == SECURE_PROTOCOL_OK;
}

static void persist_rx_checkpoint(void)
{
    SecurePeerContext_t *peer = get_remote_peer();

    if (peer == NULL || hardware.storage == NULL ||
        hardware.storage->SavePeerSession == NULL ||
        !peer->rxCounterInitialized) {
        return;
    }

    if (peer->acceptedRxSessionId != lastStoredRxSession ||
        (peer->lastAcceptedRxCounter % RX_CHECKPOINT_INTERVAL) == 0u) {
        if (hardware.storage->SavePeerSession(peer->remoteDeviceId,
                                              peer->acceptedRxSessionId,
                                              peer->lastAcceptedRxCounter)) {
            lastStoredRxSession = peer->acceptedRxSessionId;
        } else {
            Device1_Log("storage", "SavePeerSession failed");
        }
    }
}

bool Device1App_Initialize(const Device1HardwareConfig_t *config)
{
    uint8_t localKey[16];
    uint8_t peerKey[16];
    uint32_t sessionId;
    uint32_t storedSession;
    uint32_t storedCounter;
    SecureProtocolStatus_t protocolStatus;
    static const osMutexAttr_t mutexAttributes = {.name = "nrfSpiMutex"};
    static const osMessageQueueAttr_t txQueueAttributes = {.name = "radioTxQueue"};
    static const osMessageQueueAttr_t rxQueueAttributes = {.name = "applicationRxQueue"};
    static const osThreadAttr_t radioTaskAttributes = {
        .name = "RadioTask",
        .stack_size = 1536u,
        .priority = osPriorityAboveNormal
    };
    static const osThreadAttr_t applicationTaskAttributes = {
        .name = "ApplicationTask",
        .stack_size = 1024u,
        .priority = osPriorityNormal
    };

    if (config == NULL || config->hspi == NULL || config->cePort == NULL ||
        config->csnPort == NULL || config->ledPort == NULL) {
        return false;
    }
    hardware = *config;

    if (!DeviceKeys_GetMasterKey(LOCAL_DEVICE_ID, localKey) ||
        !DeviceKeys_GetMasterKey(REMOTE_DEVICE_ID, peerKey) ||
        !create_local_session(localKey, &sessionId)) {
        return false;
    }

    protocolStatus = SecureProtocol_Init(&protocol, LOCAL_DEVICE_ID,
                                         sessionId, localKey);
    if (protocolStatus != SECURE_PROTOCOL_OK) {
        return false;
    }
    protocolStatus = SecureProtocol_AddPeer(&protocol, REMOTE_DEVICE_ID,
                                            peerKey);
    if (protocolStatus != SECURE_PROTOCOL_OK) {
        return false;
    }

    if (SecureTransport_Init(&transport, &protocol) != SECURE_TRANSPORT_OK) {
        return false;
    }

    if (hardware.storage != NULL &&
        hardware.storage->LoadPeerSession != NULL &&
        hardware.storage->LoadPeerSession(REMOTE_DEVICE_ID,
                                          &storedSession,
                                          &storedCounter)) {
        protocolStatus = SecureProtocol_SeedPeerRxState(&protocol,
                                                        REMOTE_DEVICE_ID,
                                                        storedSession,
                                                        storedCounter,
                                                        true);
        if (protocolStatus != SECURE_PROTOCOL_OK) {
            return false;
        }
        lastStoredRxSession = storedSession;
    }

    spiMutexHandle = osMutexNew(&mutexAttributes);
    radioTxQueueHandle = osMessageQueueNew(8u, sizeof(RadioTxMessage_t),
                                           &txQueueAttributes);
    applicationRxQueueHandle = osMessageQueueNew(8u,
                                                  sizeof(ApplicationRxMessage_t),
                                                  &rxQueueAttributes);
    if (spiMutexHandle == NULL || radioTxQueueHandle == NULL ||
        applicationRxQueueHandle == NULL) {
        return false;
    }

    radioTaskHandle = osThreadNew(Device1_RadioTask, NULL,
                                  &radioTaskAttributes);
    applicationTaskHandle = osThreadNew(Device1_ApplicationTask, NULL,
                                        &applicationTaskAttributes);
    return radioTaskHandle != NULL && applicationTaskHandle != NULL;
}

static bool initialize_radio(void)
{
    NRF24_Status_t status;

    status = NRF24_Init(&radio, hardware.hspi,
                        hardware.cePort, hardware.cePin,
                        hardware.csnPort, hardware.csnPin);
    if (status != NRF24_OK) {
        Device1_Log("nRF24", NRF24_StatusToString(status));
        return false;
    }
    status = NRF24_SetRxAddress(&radio, 0u, ownAddress);
    if (status == NRF24_OK) {
        status = NRF24_StartListening(&radio);
    }
    if (status != NRF24_OK) {
        Device1_Log("nRF24", NRF24_StatusToString(status));
        return false;
    }
    return true;
}

static NRF24_Status_t send_serialized_frame(const uint8_t *frame, uint8_t frameLength)
{
    NRF24_Status_t radioStatus = NRF24_ERROR;

    if (osMutexAcquire(spiMutexHandle, osWaitForever) != osOK) {
        Device1_Log("SPI mutex", "acquire failed");
        return NRF24_ERROR;
    }

    radioStatus = NRF24_SetTxAddress(&radio, peerAddress);
    if (radioStatus == NRF24_OK) {
        /* Pipe 0 must match TX_ADDR while Auto ACK is active. */
        radioStatus = NRF24_SetRxAddress(&radio, 0u, peerAddress);
    }
    if (radioStatus == NRF24_OK) {
        radioStatus = NRF24_Send(&radio, frame, frameLength,
                                 RADIO_SEND_TIMEOUT_MS);
    }

    {
        NRF24_Status_t restoreStatus = NRF24_SetRxAddress(&radio, 0u, ownAddress);
        if (restoreStatus == NRF24_OK) {
            restoreStatus = NRF24_StartListening(&radio);
        }
        if (restoreStatus != NRF24_OK) {
            Device1_Log("RX restore", NRF24_StatusToString(restoreStatus));
        }
    }

    if (osMutexRelease(spiMutexHandle) != osOK) {
        Device1_Log("SPI mutex", "release failed");
    }
    return radioStatus;
}

static void transmit_queued_message(const RadioTxMessage_t *message)
{
    SecureTransportTxState_t txState;
    SecureTransportStatus_t transportStatus;
    bool messageComplete = false;

    transportStatus = SecureTransport_BeginMessage(&transport,
                                                   message->destinationId,
                                                   message->messageType,
                                                   message->payload,
                                                   message->payloadLength,
                                                   &txState);
    if (transportStatus != SECURE_TRANSPORT_OK) {
        Device1_Log("transport TX",
                    SecureTransport_StatusToString(transportStatus));
        return;
    }

    while (!messageComplete) {
        uint8_t frame[SECURE_MAX_FRAME_SIZE];
        uint8_t frameLength = 0u;
        NRF24_Status_t radioStatus;

        transportStatus = SecureTransport_CreateNextFrame(&transport,
                                                          &txState,
                                                          frame,
                                                          sizeof(frame),
                                                          &frameLength,
                                                          &messageComplete);
        if (transportStatus != SECURE_TRANSPORT_OK) {
            Device1_Log("transport TX",
                        SecureTransport_StatusToString(transportStatus));
            if (transportStatus == SECURE_TRANSPORT_PROTOCOL_ERROR) {
                Device1_Log("protocol TX",
                            SecureProtocol_StatusToString(
                                SecureTransport_GetLastProtocolStatus(&transport)));
            }
            return;
        }

        radioStatus = send_serialized_frame(frame, frameLength);
        if (radioStatus != NRF24_OK) {
            Device1_Log("TX", NRF24_StatusToString(radioStatus));
            return;
        }
    }
}

static void poll_received_frame(void)
{
    uint8_t frame[SECURE_MAX_FRAME_SIZE];
    uint8_t frameLength = 0u;
    uint16_t plainLength = 0u;
    uint8_t sourceId = 0u;
    SecureMessageType_t messageType = MESSAGE_TYPE_NONE;
    ApplicationRxMessage_t message;
    NRF24_Status_t radioStatus;
    SecureTransportStatus_t transportStatus;

    if (osMutexAcquire(spiMutexHandle, osWaitForever) != osOK) {
        Device1_Log("SPI mutex", "acquire failed");
        return;
    }
    radioStatus = NRF24_Receive(&radio, frame, sizeof(frame), &frameLength);
    if (osMutexRelease(spiMutexHandle) != osOK) {
        Device1_Log("SPI mutex", "release failed");
    }

    if (radioStatus != NRF24_OK) {
        Device1_Log("RX", NRF24_StatusToString(radioStatus));
        return;
    }
    if (frameLength == 0u) {
        return;
    }

    transportStatus = SecureTransport_ProcessFrame(&transport,
                                                   frame,
                                                   frameLength,
                                                   &messageType,
                                                   message.payload,
                                                   sizeof(message.payload),
                                                   &plainLength,
                                                   &sourceId);
    if (transportStatus == SECURE_TRANSPORT_IN_PROGRESS) {
        /* A valid authenticated fragment was accepted. */
        persist_rx_checkpoint();
        return;
    }
    if (transportStatus != SECURE_TRANSPORT_OK) {
        Device1_Log("transport RX",
                    SecureTransport_StatusToString(transportStatus));
        if (transportStatus == SECURE_TRANSPORT_PROTOCOL_ERROR) {
            Device1_Log("protocol RX",
                        SecureProtocol_StatusToString(
                            SecureTransport_GetLastProtocolStatus(&transport)));
        }
        return;
    }

    message.sourceId = sourceId;
    message.messageType = messageType;
    message.payloadLength = plainLength;
    if (osMessageQueuePut(applicationRxQueueHandle, &message, 0u, 0u) != osOK) {
        Device1_Log("RX queue", "full");
    }
    persist_rx_checkpoint();
}

void Device1_RadioTask(void *argument)
{
    RadioTxMessage_t message;
    (void)argument;

    while (!initialize_radio()) {
        osDelay(1000u);
    }

    for (;;) {
        if (osMessageQueueGet(radioTxQueueHandle, &message, NULL, 0u) == osOK) {
            transmit_queued_message(&message);
        }
        poll_received_frame();
        osDelay(2u);
    }
}

void Device1_ApplicationTask(void *argument)
{
    uint32_t lastTransmissionTick = osKernelGetTickCount();
    uint32_t lastHeartbeatTick = lastTransmissionTick;
    static const uint8_t demoHeartbeat[] = "Hello World !";
    ApplicationRxMessage_t received;
    RadioTxMessage_t outgoing;
    (void)argument;

    for (;;) {
        uint32_t now = osKernelGetTickCount();
        if ((now - lastTransmissionTick) >= 1000u) {
            outgoing.destinationId = REMOTE_DEVICE_ID;
            outgoing.messageType = MESSAGE_TYPE_TEMPERATURE;
            outgoing.payloadLength = 5u;
            put_i16_be(&outgoing.payload[0], 240);   /* 24.0 C */
            put_u16_be(&outgoing.payload[2], 3300u);
            outgoing.payload[4] = 0x01u;
            if (osMessageQueuePut(radioTxQueueHandle, &outgoing, 0u, 0u) != osOK) {
                Device1_Log("TX queue", "full");
            }
            lastTransmissionTick = now;
        }

        if ((now - lastHeartbeatTick) >= 5000u) {
            outgoing.destinationId = REMOTE_DEVICE_ID;
            outgoing.messageType = MESSAGE_TYPE_HEARTBEAT;
            outgoing.payloadLength = (uint16_t)(sizeof(demoHeartbeat) - 1u);
            memcpy(outgoing.payload, demoHeartbeat, outgoing.payloadLength);
            if (osMessageQueuePut(radioTxQueueHandle, &outgoing, 0u, 0u) != osOK) {
                Device1_Log("TX queue", "full");
            }
            lastHeartbeatTick = now;
        }

        if (osMessageQueueGet(applicationRxQueueHandle, &received,
                              NULL, 0u) == osOK &&
            received.messageType == MESSAGE_TYPE_COMMAND_SET_LED &&
            received.payloadLength == 1u) {
            HAL_GPIO_WritePin(hardware.ledPort,
                              hardware.ledPin,
                              received.payload[0] != 0u ?
                                  GPIO_PIN_SET : GPIO_PIN_RESET);
        }
        osDelay(10u);
    }
}
