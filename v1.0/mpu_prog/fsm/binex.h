#ifndef __BINEX_H__
#define __BINEX_H__

#include <stdint.h>

typedef enum
{
  BINEX_READY = 0,
  BINEX_RUN,
  BINEX_ERROR,
} BinExStatus_t;

typedef enum
{
  BINEX_OK = 0,
  BINEX_BUSY = 1
} BinExRetCode_t;


typedef enum
{
  RXPACK_OK = 0,
  RXPACK_CRCERR = 1,
  RXPACK_TOO_LONG = 2
} BinExRxExtendedStatus_t;

#define BINEX_BUFFLEN           32





void BinEx_Init(void);
void BinEx_Process(void);

BinExStatus_t BinEx_TxStatus(void); //статус передатчика
BinExRetCode_t BinEx_TxBegin(uint16_t len); //Запустить передачу данных
uint8_t *BinEx_TxGetBuffPntr(void); //получить указатель на буфер передатчика

BinExStatus_t BinEx_RxStatus(void); //Получить статус приемника
BinExRxExtendedStatus_t BinEx_RxExtendedStatus(void); //Получить статус приемника болле подробно
BinExRetCode_t BinEx_RxBegin(void); //Разрешить прием пакета
uint16_t BinEx_RxDataLen(void); //Получить длину принятого пакета
uint8_t *BinEx_RxGetBuffPntr(void); //Получить указатель на буфер приемника


#endif
