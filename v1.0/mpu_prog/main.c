#include "stm32f0xx.h"
#include "binex.h"
#include "SysClock.h"

static uint8_t state = 0;

void TxTestProc(void)
{
  static uint8_t *tx_buff;
  static uint8_t *rx_buff;
  static uint16_t rx_len;
  
  switch(state)
  {
  case 0:
    BinEx_RxBegin(); //Запускаем прием данных
    state = 1;
    break;
  //////////////////////////////////////
  case 1:
    if(BinEx_RxStatus() == BINEX_READY) //Если получили пакет данных
      state = 2;
    else if(BinEx_RxStatus() == BINEX_ERROR) //Если возникла ошибка при приеме
      state = 3;
    break;
  //////////////////////////////////////  
  case 2:
    //отправляем принятый пакет
    if(BinEx_TxStatus() == BINEX_READY) //Если передатчик освободился после предыдущей передачи
    {
      //Получаем буферы приемника и передатчика
      tx_buff = BinEx_TxGetBuffPntr();
      rx_buff = BinEx_RxGetBuffPntr();
      
      rx_len = BinEx_RxDataLen();
      
      //Копируем буфер приемника в буфер передатчика
      for(int i=0; i<rx_len; i++)
      {
        tx_buff[i] = rx_buff[i];
      }
      
      //Запускаем передачу принятого пакета
      BinEx_TxBegin(rx_len);
      
      //переходим в исходное состояние
      state = 0;
    }
    break;
  //////////////////////////////////////
  case 3: //Ошибка приема
    if(BinEx_TxStatus() == BINEX_READY) //Если передатчик освободился после предыдущей передачи
    {
      //Получаем буферы приемника и передатчика
      tx_buff = BinEx_TxGetBuffPntr();
      
      tx_buff[0] = 1;
      //Запускаем передачу принятого пакета
      BinEx_TxBegin(1);
      
      //переходим в исходное состояние
      state = 0;
    }
    break;
  //////////////////////////////////////
  }
}

void main(void)
{
  SysClockInit();
  
  BinEx_Init();
  
  
  for(;;)
  {
    BinEx_Process();
    TxTestProc();
  }
}
