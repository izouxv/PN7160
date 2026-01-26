#ifndef TML_H
#define TML_H

#include "tool.h"

#define TIMEOUT_INFINITE 0
#define TIMEOUT_10MS 10
#define TIMEOUT_100MS 100
#define TIMEOUT_1S 1000
#define TIMEOUT_2S 2000

typedef enum
{
    ERROR = 0,
    SUCCESS = !ERROR
} Status;

void tml_Connect(void);
void tml_Disconnect(void);
void tml_EnterDwlMode(void);
void tml_LeaveDwlMode(void);
void tml_Send(uint8_t *pBuffer, uint16_t BufferLen, uint16_t *pBytesSent);
void tml_Receive(uint8_t *pBuffer, uint16_t BufferLen, uint16_t *pBytes, uint16_t timeout);
Status tml_Init(void);
Status tml_DeInit(void);
Status tml_Tx(uint8_t *pBuffer, uint16_t BufferLen);
Status tml_Rx(uint8_t *pBuffer, uint16_t BufferLen, uint16_t *pBytesRead);
Status tml_WaitForRx(uint32_t timeout);
#endif // TML_H
