#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "Crypto.h"
#include "DeviceKeys.h"
#include "SecureProtocol.h"
#include "SecureTransport.h"

static unsigned tests_run=0u,tests_failed=0u;
#define CHECK(expr) do{++tests_run;if(!(expr)){++tests_failed;printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr);}}while(0)
#define CHECK_STATUS(actual,expected) do{SecureProtocolStatus_t a_=(actual);++tests_run;if(a_!=(expected)){++tests_failed;printf("FAIL %s:%d: got %s expected %s\n",__FILE__,__LINE__,SecureProtocol_StatusToString(a_),SecureProtocol_StatusToString(expected));}}while(0)
#define CHECK_TRANSPORT_STATUS(actual,expected) do{SecureTransportStatus_t a_=(actual);++tests_run;if(a_!=(expected)){++tests_failed;printf("FAIL %s:%d: got %s expected %s\n",__FILE__,__LINE__,SecureTransport_StatusToString(a_),SecureTransport_StatusToString(expected));}}while(0)

static void init_pair(SecureProtocolContext_t*tx,SecureProtocolContext_t*rx,uint32_t session){
 uint8_t k1[16],k2[16];CHECK(DeviceKeys_GetMasterKey(DEVICE_ID_1,k1));CHECK(DeviceKeys_GetMasterKey(DEVICE_ID_2,k2));
 CHECK_STATUS(SecureProtocol_Init(tx,DEVICE_ID_1,session,k1),SECURE_PROTOCOL_OK);CHECK_STATUS(SecureProtocol_AddPeer(tx,DEVICE_ID_2,k2),SECURE_PROTOCOL_OK);
 CHECK_STATUS(SecureProtocol_Init(rx,DEVICE_ID_2,0x00001055u,k2),SECURE_PROTOCOL_OK);CHECK_STATUS(SecureProtocol_AddPeer(rx,DEVICE_ID_1,k1),SECURE_PROTOCOL_OK);
}
static void test_ctr_vector(void){
 const uint8_t key[16]={0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
 const uint8_t iv[16]={0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff};
 const uint8_t pt[64]={0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a,0xae,0x2d,0x8a,0x57,0x1e,0x03,0xac,0x9c,0x9e,0xb7,0x6f,0xac,0x45,0xaf,0x8e,0x51,0x30,0xc8,0x1c,0x46,0xa3,0x5c,0xe4,0x11,0xe5,0xfb,0xc1,0x19,0x1a,0x0a,0x52,0xef,0xf6,0x9f,0x24,0x45,0xdf,0x4f,0x9b,0x17,0xad,0x2b,0x41,0x7b,0xe6,0x6c,0x37,0x10};
 const uint8_t ct[64]={0x87,0x4d,0x61,0x91,0xb6,0x20,0xe3,0x26,0x1b,0xef,0x68,0x64,0x99,0x0d,0xb6,0xce,0x98,0x06,0xf6,0x6b,0x79,0x70,0xfd,0xff,0x86,0x17,0x18,0x7b,0xb9,0xff,0xfd,0xff,0x5a,0xe4,0xdf,0x3e,0xdb,0xd5,0xd3,0x5e,0x5b,0x4f,0x09,0x02,0x0d,0xb0,0x3e,0xab,0x1e,0x03,0x1d,0xda,0x2f,0xbe,0x03,0xd1,0x79,0x21,0x70,0xa0,0xf3,0x00,0x9c,0xee};
 uint8_t out[64],back[64];CHECK(AES_CTR_Encrypt(key,iv,pt,out,sizeof(out))==CRYPTO_OK);CHECK(memcmp(out,ct,64u)==0);CHECK(AES_CTR_Decrypt(key,iv,out,back,64u)==CRYPTO_OK);CHECK(memcmp(back,pt,64u)==0);
}
static void test_cmac_vectors(void){
 const uint8_t key[16]={0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
 const uint8_t msg[16]={0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a};
 const uint8_t e0[16]={0xbb,0x1d,0x69,0x29,0xe9,0x59,0x37,0x28,0x7f,0xa3,0x7d,0x12,0x9b,0x75,0x67,0x46};
 const uint8_t e1[16]={0x07,0x0a,0x16,0xb4,0x6b,0x4d,0x41,0x44,0xf7,0x9b,0xdd,0x9d,0xd0,0x4a,0x28,0x7c};
 uint8_t m[16];CHECK(Crypto_CalculateCmac(key,NULL,0u,m)==CRYPTO_OK);CHECK(memcmp(m,e0,16u)==0);CHECK(Crypto_CalculateCmac(key,msg,16u,m)==CRYPTO_OK);CHECK(memcmp(m,e1,16u)==0);
}
static void test_short_and_nonblock(void){uint8_t key[16]={1,7,3,9,5,11,13,17,19,23,29,31,37,41,43,47},iv[16]={0},p[23],c[23],b[23];for(unsigned i=0;i<23u;++i)p[i]=(uint8_t)i;CHECK(AES_CTR_Encrypt(key,iv,p,c,23u)==CRYPTO_OK);CHECK(memcmp(p,c,23u)!=0);CHECK(AES_CTR_Decrypt(key,iv,c,b,23u)==CRYPTO_OK);CHECK(memcmp(p,b,23u)==0);}
static void test_protocol_success_replay_loss(void){
 SecureProtocolContext_t tx,rx;uint8_t f0[32],f1[32],f2[32],l0,l1,l2,out[11],outLen,src;SecureMessageType_t type;const uint8_t p[5]={0,240,12,228,1};init_pair(&tx,&rx,0x000001AAu);
 CHECK_STATUS(SecureProtocol_CreateFrame(&tx,2,MESSAGE_TYPE_TEMPERATURE,p,5,f0,32,&l0),SECURE_PROTOCOL_OK);CHECK(l0==26u);CHECK_STATUS(SecureProtocol_ProcessFrame(&rx,f0,l0,&type,out,sizeof(out),&outLen,&src),SECURE_PROTOCOL_OK);CHECK(type==MESSAGE_TYPE_TEMPERATURE&&src==1u&&outLen==5u&&memcmp(out,p,5u)==0);
 CHECK_STATUS(SecureProtocol_ProcessFrame(&rx,f0,l0,&type,out,sizeof(out),&outLen,&src),SECURE_PROTOCOL_REPLAY_DETECTED);
 CHECK_STATUS(SecureProtocol_CreateFrame(&tx,2,MESSAGE_TYPE_TEMPERATURE,p,5,f1,32,&l1),SECURE_PROTOCOL_OK);CHECK_STATUS(SecureProtocol_CreateFrame(&tx,2,MESSAGE_TYPE_TEMPERATURE,p,5,f2,32,&l2),SECURE_PROTOCOL_OK);CHECK_STATUS(SecureProtocol_ProcessFrame(&rx,f2,l2,&type,out,sizeof(out),&outLen,&src),SECURE_PROTOCOL_OK);(void)f1;(void)l1;
}
static void test_different_counters_ciphertext(void){SecureProtocolContext_t tx,rx;uint8_t a[32],b[32],la,lb;const uint8_t p[5]={0,240,12,228,1};init_pair(&tx,&rx,0x000003CCu);CHECK_STATUS(SecureProtocol_CreateFrame(&tx,2,MESSAGE_TYPE_TEMPERATURE,p,5,a,32,&la),SECURE_PROTOCOL_OK);CHECK_STATUS(SecureProtocol_CreateFrame(&tx,2,MESSAGE_TYPE_TEMPERATURE,p,5,b,32,&lb),SECURE_PROTOCOL_OK);CHECK(la==lb&&memcmp(&a[13],&b[13],5u)!=0);}
static void fresh_valid(uint8_t frame[32],uint8_t*len,SecureProtocolContext_t*rx){SecureProtocolContext_t tx;const uint8_t p[5]={0,240,12,228,1};init_pair(&tx,rx,0x000004DDu);CHECK_STATUS(SecureProtocol_CreateFrame(&tx,2,MESSAGE_TYPE_TEMPERATURE,p,5,frame,32,len),SECURE_PROTOCOL_OK);}
static SecureProtocolStatus_t process(SecureProtocolContext_t*rx,uint8_t*f,uint8_t len){uint8_t out[11],ol,src;SecureMessageType_t t;return SecureProtocol_ProcessFrame(rx,f,len,&t,out,sizeof(out),&ol,&src);}
static void test_auth_tampering(void){uint8_t f[32],len;SecureProtocolContext_t rx;fresh_valid(f,&len,&rx);f[3]^=1u;CHECK_STATUS(process(&rx,f,len),SECURE_PROTOCOL_AUTHENTICATION_FAILED);fresh_valid(f,&len,&rx);f[13]^=1u;CHECK_STATUS(process(&rx,f,len),SECURE_PROTOCOL_AUTHENTICATION_FAILED);fresh_valid(f,&len,&rx);f[len-1u]^=1u;CHECK_STATUS(process(&rx,f,len),SECURE_PROTOCOL_AUTHENTICATION_FAILED);}
static void test_wrong_key(void){uint8_t f[32],len,k2[16],wrong[16]={0x8a,0x19,0x71,0x44,0x20,0x5e,0xdd,0x93,0x12,0xac,0x67,0x09,0x31,0xfe,0x58,0xb0};SecureProtocolContext_t goodRx,badRx,tx;const uint8_t p[1]={1};uint8_t k1[16];CHECK(DeviceKeys_GetMasterKey(1,k1));CHECK(DeviceKeys_GetMasterKey(2,k2));init_pair(&tx,&goodRx,0x00000511u);CHECK_STATUS(SecureProtocol_CreateFrame(&tx,2,MESSAGE_TYPE_COMMAND_SET_LED,p,1,f,32,&len),SECURE_PROTOCOL_OK);CHECK_STATUS(SecureProtocol_Init(&badRx,2,0x00001022u,k2),SECURE_PROTOCOL_OK);CHECK_STATUS(SecureProtocol_AddPeer(&badRx,1,wrong),SECURE_PROTOCOL_OK);CHECK_STATUS(process(&badRx,f,len),SECURE_PROTOCOL_AUTHENTICATION_FAILED);}
static void test_counter_not_updated_on_auth_failure(void){uint8_t f[32],bad[32],len;SecureProtocolContext_t rx;fresh_valid(f,&len,&rx);memcpy(bad,f,len);bad[11]^=1u;CHECK_STATUS(process(&rx,bad,len),SECURE_PROTOCOL_AUTHENTICATION_FAILED);CHECK_STATUS(process(&rx,f,len),SECURE_PROTOCOL_OK);}
static void test_semantic_rejections(void){uint8_t f[32],len,bad[32];SecureProtocolContext_t rx;fresh_valid(f,&len,&rx);memcpy(bad,f,len);bad[2]=3u;CHECK_STATUS(process(&rx,bad,len),SECURE_PROTOCOL_WRONG_DESTINATION);fresh_valid(f,&len,&rx);memcpy(bad,f,len);bad[1]=3u;CHECK_STATUS(process(&rx,bad,len),SECURE_PROTOCOL_UNKNOWN_DEVICE);fresh_valid(f,&len,&rx);memcpy(bad,f,len);bad[0]=2u;CHECK_STATUS(process(&rx,bad,len),SECURE_PROTOCOL_INVALID_VERSION);fresh_valid(f,&len,&rx);CHECK_STATUS(process(&rx,f,(uint8_t)(len-1u)),SECURE_PROTOCOL_INVALID_LENGTH);memcpy(bad,f,len);bad[12]=12u;CHECK_STATUS(process(&rx,bad,len),SECURE_PROTOCOL_INVALID_LENGTH);}
static void test_max_payload_and_serialization(void){SecureProtocolContext_t tx,rx;uint8_t p[12]={0},f[32],len,v,src,dst,t,pl;uint32_t sid,counter;const uint8_t*cipher,*tag;init_pair(&tx,&rx,0x00000666u);CHECK_STATUS(SecureProtocol_CreateFrame(&tx,2,MESSAGE_TYPE_HEARTBEAT,p,12,f,32,&len),SECURE_PROTOCOL_INVALID_LENGTH);CHECK_STATUS(SecureProtocol_CreateFrame(&tx,2,MESSAGE_TYPE_HEARTBEAT,p,11,f,32,&len),SECURE_PROTOCOL_OK);CHECK(len==32u);CHECK_STATUS(SecureProtocol_DeserializeFrame(f,len,&v,&src,&dst,&t,&sid,&counter,&cipher,&pl,&tag),SECURE_PROTOCOL_OK);CHECK(v==1u&&src==1u&&dst==2u&&t==MESSAGE_TYPE_HEARTBEAT&&sid==0x00000666u&&counter==0u&&pl==11u&&cipher==&f[13]&&tag==&f[24]);}
static void test_session_policy(void){SecureProtocolContext_t txOld,rx,txNew;uint8_t f[32],len;const uint8_t p[1]={7};init_pair(&txOld,&rx,0x000001A1u);CHECK_STATUS(SecureProtocol_CreateFrame(&txOld,2,MESSAGE_TYPE_HEARTBEAT,p,1,f,32,&len),SECURE_PROTOCOL_OK);CHECK_STATUS(process(&rx,f,len),SECURE_PROTOCOL_OK);{SecureProtocolContext_t dummy;init_pair(&txNew,&dummy,0x000002B2u);}CHECK_STATUS(SecureProtocol_CreateFrame(&txNew,2,MESSAGE_TYPE_HEARTBEAT,p,1,f,32,&len),SECURE_PROTOCOL_OK);CHECK_STATUS(process(&rx,f,len),SECURE_PROTOCOL_OK);CHECK_STATUS(SecureProtocol_CreateFrame(&txOld,2,MESSAGE_TYPE_HEARTBEAT,p,1,f,32,&len),SECURE_PROTOCOL_OK);CHECK_STATUS(process(&rx,f,len),SECURE_PROTOCOL_SESSION_REJECTED);}
static void test_session_generation(void){uint8_t k[16];const uint32_t uid[3]={0x12345678u,0x9abcdef0u,0x0badc0deu};uint32_t a,b;CHECK(DeviceKeys_GetMasterKey(1,k));CHECK_STATUS(SecureProtocol_GenerateSessionId(k,uid,123u,1000u,0x321u,&a),SECURE_PROTOCOL_OK);CHECK_STATUS(SecureProtocol_GenerateSessionId(k,uid,124u,1001u,0x322u,&b),SECURE_PROTOCOL_OK);CHECK(SecureProtocol_GetSessionGeneration(a)==123u&&SecureProtocol_GetSessionGeneration(b)==124u&&a!=b);}

static void init_transport_pair(SecureProtocolContext_t *txProtocol,
                                SecureProtocolContext_t *rxProtocol,
                                SecureTransportContext_t *txTransport,
                                SecureTransportContext_t *rxTransport,
                                uint32_t session)
{
    init_pair(txProtocol, rxProtocol, session);
    CHECK_TRANSPORT_STATUS(SecureTransport_Init(txTransport, txProtocol),
                           SECURE_TRANSPORT_OK);
    CHECK_TRANSPORT_STATUS(SecureTransport_Init(rxTransport, rxProtocol),
                           SECURE_TRANSPORT_OK);
}

static void test_transport_small_message(void)
{
    SecureProtocolContext_t txProtocol, rxProtocol;
    SecureTransportContext_t txTransport, rxTransport;
    SecureTransportTxState_t txState;
    const uint8_t input[] = {'s','m','a','l','l'};
    uint8_t frame[SECURE_MAX_FRAME_SIZE], frameLength = 0u;
    uint8_t output[SECURE_TRANSPORT_MAX_MESSAGE_SIZE], source = 0u;
    uint16_t outputLength = 0u;
    SecureMessageType_t type = MESSAGE_TYPE_NONE;
    bool complete = false;

    init_transport_pair(&txProtocol, &rxProtocol, &txTransport, &rxTransport,
                        0x00002001u);
    CHECK_TRANSPORT_STATUS(SecureTransport_BeginMessage(&txTransport, 2u,
                           MESSAGE_TYPE_HEARTBEAT, input, sizeof(input), &txState),
                           SECURE_TRANSPORT_OK);
    CHECK(!txState.fragmented);
    CHECK_TRANSPORT_STATUS(SecureTransport_CreateNextFrame(&txTransport, &txState,
                           frame, sizeof(frame), &frameLength, &complete),
                           SECURE_TRANSPORT_OK);
    CHECK(complete);
    CHECK_TRANSPORT_STATUS(SecureTransport_ProcessFrame(&rxTransport, frame,
                           frameLength, &type, output, sizeof(output), &outputLength,
                           &source), SECURE_TRANSPORT_OK);
    CHECK(type == MESSAGE_TYPE_HEARTBEAT && source == 1u &&
          outputLength == sizeof(input) && memcmp(input, output, sizeof(input)) == 0);
}

static void test_transport_hello_world_fragmentation(void)
{
    SecureProtocolContext_t txProtocol, rxProtocol;
    SecureTransportContext_t txTransport, rxTransport;
    SecureTransportTxState_t txState;
    const uint8_t input[] = "Hello World !";
    uint8_t frame[SECURE_MAX_FRAME_SIZE], frameLength = 0u;
    uint8_t output[SECURE_TRANSPORT_MAX_MESSAGE_SIZE], source = 0u;
    uint16_t outputLength = 0u;
    SecureMessageType_t type = MESSAGE_TYPE_NONE;
    bool complete = false;
    unsigned frames = 0u;

    init_transport_pair(&txProtocol, &rxProtocol, &txTransport, &rxTransport,
                        0x00002002u);
    CHECK_TRANSPORT_STATUS(SecureTransport_BeginMessage(&txTransport, 2u,
                           MESSAGE_TYPE_HEARTBEAT, input, 13u, &txState),
                           SECURE_TRANSPORT_OK);
    CHECK(txState.fragmented);
    CHECK(txState.fragmentCount == 2u);

    while (!complete) {
        SecureTransportStatus_t rxStatus;
        CHECK_TRANSPORT_STATUS(SecureTransport_CreateNextFrame(&txTransport, &txState,
                               frame, sizeof(frame), &frameLength, &complete),
                               SECURE_TRANSPORT_OK);
        CHECK(frameLength <= SECURE_MAX_FRAME_SIZE);
        ++frames;
        rxStatus = SecureTransport_ProcessFrame(&rxTransport, frame, frameLength,
                                                &type, output, sizeof(output),
                                                &outputLength, &source);
        if (!complete) {
            CHECK_TRANSPORT_STATUS(rxStatus, SECURE_TRANSPORT_IN_PROGRESS);
        } else {
            CHECK_TRANSPORT_STATUS(rxStatus, SECURE_TRANSPORT_OK);
        }
    }
    CHECK(frames == 2u);
    CHECK(type == MESSAGE_TYPE_HEARTBEAT && source == 1u && outputLength == 13u &&
          memcmp(input, output, 13u) == 0);
}

static void test_transport_maximum_message(void)
{
    SecureProtocolContext_t txProtocol, rxProtocol;
    SecureTransportContext_t txTransport, rxTransport;
    SecureTransportTxState_t txState;
    uint8_t input[SECURE_TRANSPORT_MAX_MESSAGE_SIZE];
    uint8_t output[SECURE_TRANSPORT_MAX_MESSAGE_SIZE];
    uint8_t frame[SECURE_MAX_FRAME_SIZE], frameLength = 0u, source = 0u;
    uint16_t outputLength = 0u;
    SecureMessageType_t type = MESSAGE_TYPE_NONE;
    bool complete = false;
    unsigned frames = 0u;
    unsigned i;

    for (i = 0u; i < sizeof(input); ++i) input[i] = (uint8_t)(i ^ 0xA5u);
    init_transport_pair(&txProtocol, &rxProtocol, &txTransport, &rxTransport,
                        0x00002003u);
    CHECK_TRANSPORT_STATUS(SecureTransport_BeginMessage(&txTransport, 2u,
                           MESSAGE_TYPE_HEARTBEAT, input, sizeof(input), &txState),
                           SECURE_TRANSPORT_OK);
    CHECK(txState.fragmentCount == SECURE_TRANSPORT_MAX_FRAGMENTS);

    while (!complete) {
        SecureTransportStatus_t rxStatus;
        CHECK_TRANSPORT_STATUS(SecureTransport_CreateNextFrame(&txTransport, &txState,
                               frame, sizeof(frame), &frameLength, &complete),
                               SECURE_TRANSPORT_OK);
        CHECK(frameLength <= SECURE_MAX_FRAME_SIZE);
        ++frames;
        rxStatus = SecureTransport_ProcessFrame(&rxTransport, frame, frameLength,
                                                &type, output, sizeof(output),
                                                &outputLength, &source);
        CHECK_TRANSPORT_STATUS(rxStatus, complete ? SECURE_TRANSPORT_OK :
                                                   SECURE_TRANSPORT_IN_PROGRESS);
    }
    CHECK(frames == SECURE_TRANSPORT_MAX_FRAGMENTS);
    CHECK(outputLength == sizeof(input) && memcmp(input, output, sizeof(input)) == 0);
}

static void test_transport_missing_fragment_then_new_message(void)
{
    SecureProtocolContext_t txProtocol, rxProtocol;
    SecureTransportContext_t txTransport, rxTransport;
    SecureTransportTxState_t txState;
    const uint8_t first[] = "This message will lose one fragment.";
    const uint8_t second[] = "Second complete message.";
    uint8_t frame[SECURE_MAX_FRAME_SIZE], frameLength = 0u, source = 0u;
    uint8_t output[SECURE_TRANSPORT_MAX_MESSAGE_SIZE];
    uint16_t outputLength = 0u;
    SecureMessageType_t type = MESSAGE_TYPE_NONE;
    bool complete = false;
    unsigned frameIndex = 0u;

    init_transport_pair(&txProtocol, &rxProtocol, &txTransport, &rxTransport,
                        0x00002004u);
    CHECK_TRANSPORT_STATUS(SecureTransport_BeginMessage(&txTransport, 2u,
                           MESSAGE_TYPE_HEARTBEAT, first, sizeof(first)-1u, &txState),
                           SECURE_TRANSPORT_OK);
    while (!complete) {
        CHECK_TRANSPORT_STATUS(SecureTransport_CreateNextFrame(&txTransport, &txState,
                               frame, sizeof(frame), &frameLength, &complete),
                               SECURE_TRANSPORT_OK);
        if (frameIndex != 1u) {
            CHECK_TRANSPORT_STATUS(SecureTransport_ProcessFrame(&rxTransport, frame,
                                   frameLength, &type, output, sizeof(output),
                                   &outputLength, &source), SECURE_TRANSPORT_IN_PROGRESS);
        }
        ++frameIndex;
    }

    complete = false;
    CHECK_TRANSPORT_STATUS(SecureTransport_BeginMessage(&txTransport, 2u,
                           MESSAGE_TYPE_HEARTBEAT, second, sizeof(second)-1u, &txState),
                           SECURE_TRANSPORT_OK);
    while (!complete) {
        SecureTransportStatus_t rxStatus;
        CHECK_TRANSPORT_STATUS(SecureTransport_CreateNextFrame(&txTransport, &txState,
                               frame, sizeof(frame), &frameLength, &complete),
                               SECURE_TRANSPORT_OK);
        rxStatus = SecureTransport_ProcessFrame(&rxTransport, frame, frameLength,
                                                &type, output, sizeof(output),
                                                &outputLength, &source);
        CHECK_TRANSPORT_STATUS(rxStatus, complete ? SECURE_TRANSPORT_OK :
                                                   SECURE_TRANSPORT_IN_PROGRESS);
    }
    CHECK(outputLength == sizeof(second)-1u &&
          memcmp(second, output, sizeof(second)-1u) == 0);
}

static void test_transport_tampered_fragment_and_size_limit(void)
{
    SecureProtocolContext_t txProtocol, rxProtocol;
    SecureTransportContext_t txTransport, rxTransport;
    SecureTransportTxState_t txState;
    uint8_t tooLarge[SECURE_TRANSPORT_MAX_MESSAGE_SIZE + 1u] = {0};
    const uint8_t input[] = "Hello World !";
    uint8_t frame[SECURE_MAX_FRAME_SIZE], validFrame[SECURE_MAX_FRAME_SIZE];
    uint8_t frameLength = 0u, source = 0u;
    uint8_t output[SECURE_TRANSPORT_MAX_MESSAGE_SIZE];
    uint16_t outputLength = 0u;
    SecureMessageType_t type = MESSAGE_TYPE_NONE;
    bool complete = false;

    init_transport_pair(&txProtocol, &rxProtocol, &txTransport, &rxTransport,
                        0x00002005u);
    CHECK_TRANSPORT_STATUS(SecureTransport_BeginMessage(&txTransport, 2u,
                           MESSAGE_TYPE_HEARTBEAT, tooLarge, sizeof(tooLarge), &txState),
                           SECURE_TRANSPORT_MESSAGE_TOO_LARGE);
    CHECK_TRANSPORT_STATUS(SecureTransport_BeginMessage(&txTransport, 2u,
                           MESSAGE_TYPE_HEARTBEAT, input, 13u, &txState),
                           SECURE_TRANSPORT_OK);
    CHECK_TRANSPORT_STATUS(SecureTransport_CreateNextFrame(&txTransport, &txState,
                           frame, sizeof(frame), &frameLength, &complete),
                           SECURE_TRANSPORT_OK);
    memcpy(validFrame, frame, frameLength);
    frame[13] ^= 0x01u;
    CHECK_TRANSPORT_STATUS(SecureTransport_ProcessFrame(&rxTransport, frame,
                           frameLength, &type, output, sizeof(output), &outputLength,
                           &source), SECURE_TRANSPORT_PROTOCOL_ERROR);
    CHECK(SecureTransport_GetLastProtocolStatus(&rxTransport) ==
          SECURE_PROTOCOL_AUTHENTICATION_FAILED);
    CHECK_TRANSPORT_STATUS(SecureTransport_ProcessFrame(&rxTransport, validFrame,
                           frameLength, &type, output, sizeof(output), &outputLength,
                           &source), SECURE_TRANSPORT_IN_PROGRESS);
}

int main(void){test_ctr_vector();test_cmac_vectors();test_short_and_nonblock();test_protocol_success_replay_loss();test_different_counters_ciphertext();test_auth_tampering();test_wrong_key();test_counter_not_updated_on_auth_failure();test_semantic_rejections();test_max_payload_and_serialization();test_session_policy();test_session_generation();test_transport_small_message();test_transport_hello_world_fragmentation();test_transport_maximum_message();test_transport_missing_fragment_then_new_message();test_transport_tampered_fragment_and_size_limit();printf("Tests: %u, failed: %u\n",tests_run,tests_failed);return tests_failed?1:0;}
