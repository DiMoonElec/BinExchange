#include "binex.h"
#include "uart.h"
#include "crc16.h"

/******************************************************************************/

#define BINEX_MAGIC_SYMBOL      0xF4

/******************************************************************************/

/*
Функции UART, используемые модулем.
Реализация UART должна включать кольцевой буфер 
минимум на 8 элементов
*/

//Инициализация UART
#define UART_INIT()             UART_Init()

//Прочитать символ из буфера
#define PUT_C(x)                UART_Put(x)

//Звписать символ в буфер
#define GET_C()                 UART_Get()

//Возвращает количество свободных байт в буфере передатчика
#define TX_BUFF_FREE()          UART_TxBuffNumFree()

//Возвращает true, если в буфере приемника есть непрочитанные байты
#define RX_BUFF_NOT_EMPTY()     (UART_RxCount() > 0)


/******************************************************************************/

#define TXDATA_OFFSET           2

static uint8_t tx_buff[BINEX_BUFFLEN + 4];
static BinExStatus_t tx_status;
static uint16_t tx_len;
static uint16_t tx_index;

static uint8_t state_tx;


enum 
{
  STX_MAIN = 0,
  STX_RUN,
  STX_SEND_HEADER,
  STX_SEND_DATA,
  STX_SEND_CHAR_DATA,
  TX_NUM_STATES
};


/******************************************************************************/


//Буфер приемника
static uint8_t rx_buff[BINEX_BUFFLEN];

//Статус принятого пакета
static BinExRxExtendedStatus_t rxpack_status; 

//Контрольная сумма пакета
static uint16_t rx_crc16;

//Длина принятого пакета
static uint16_t rx_len;

//Используется функциями приемника
static uint16_t rx_index;

//Статус приемника 
static BinExStatus_t rx_status;


//Состояние конечного автомата приемника
static uint8_t state_rx;

enum 
{
  SRX_MAIN = 0,
  SRX_RUN,
  RX_NUM_STATES
};



#define BINEX_PACK_START        (1<<0)
#define BINEX_CHAR_DETECT       (1<<1)

typedef struct 
{
  uint8_t flags;
  uint8_t data;
}BinExchangeChar_t;



/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

//Процесс передатчика
static void tx_proc(void)
{
  static uint16_t crc16_val = 0;
   
  
  switch(state_tx)
  {
  ////////////////////////////
  case STX_MAIN:
    if(tx_status == BINEX_RUN)
      state_tx = STX_RUN;
    break;
  ////////////////////////////
  case STX_RUN:
    //Расчет CRC передаваемых данных
    crc16_val = Crc16Reset();
    crc16_val = Crc16(tx_buff + TXDATA_OFFSET, tx_len, crc16_val);
    
    
    tx_buff[tx_len + 2] = (uint8_t)(crc16_val);
    tx_buff[tx_len + 3] = (uint8_t)(crc16_val >> 8);
   
    
    //Длина пакета
    tx_buff[0] = (uint8_t)(tx_len);
    tx_buff[1] = (uint8_t)(tx_len >> 8);
    
    state_tx = STX_SEND_HEADER;
    
    break;
  ////////////////////////////
  case STX_SEND_HEADER:
    //отправка заголовка пакета
    if(TX_BUFF_FREE() > 3)
    {
      PUT_C(BINEX_MAGIC_SYMBOL);
      PUT_C(0);
      
      tx_len += 4; //к передаваемым данным добавляется
      //длина пакета и CRC16
      
      tx_index = 0;
      
      state_tx = STX_SEND_DATA;
    }
    break;
  ////////////////////////////
  case STX_SEND_DATA:
    //передача основного массива данных
    if(tx_index < tx_len) //если есть что еще передать
    {
      state_tx = STX_SEND_CHAR_DATA;
    }
    else //если все передали
    {
      tx_status = BINEX_READY;
      state_tx = STX_MAIN;
    }
    break;
  ////////////////////////////
  case STX_SEND_CHAR_DATA:
    if(TX_BUFF_FREE() > 3)
    {
      PUT_C(tx_buff[tx_index]);
      
      //Если это спец. символ, то экранируем его
      if(tx_buff[tx_index] == BINEX_MAGIC_SYMBOL)
        PUT_C(BINEX_MAGIC_SYMBOL);
      
      tx_index++;
      state_tx = STX_SEND_DATA;
    }
    break;
  }
}

/******************************************************************************/

//Приемник байта данных
//Парсит управляющие последовательности протокола
static BinExchangeChar_t *GetByte(uint8_t ch)
{
  static uint8_t flag_PrevMagicChar = 0;
  static BinExchangeChar_t ret;
  
  if(!flag_PrevMagicChar)
  {
    if(ch != BINEX_MAGIC_SYMBOL)
    {
      //Не спец. символ, просто данные
      ret.flags = BINEX_CHAR_DETECT;
      ret.data = ch;
      
      flag_PrevMagicChar = 0;
    }
    else
    {
      //Спец. символ, ждем следующий
      ret.flags = 0;
      ret.data = ch;
      
      flag_PrevMagicChar = 1;
    }
  }
  else
  {
    if(ch != BINEX_MAGIC_SYMBOL) 
    {
      //Начало пакета
      ret.flags = BINEX_PACK_START;
      ret.data = ch;
      
      flag_PrevMagicChar = 0;
    }
    else
    {
      //Экранирование спец. символа
      ret.flags = BINEX_CHAR_DETECT;
      ret.data = ch;
      
      flag_PrevMagicChar = 0;
    }
  }
  
  return &ret;
}


//Приемник пакета 
//Парсит входной поток, реализует интерфейс обмена данными
//Если обнаруживает пакет данных, то возвращает 1
//Если пакет в входном потоке еще не обнаружен, то вохвращает 0
//
//Если пакет обнаружен, то актуальны следующие переменные:
//      rx_buff[] - принятый пакет данных
//      rxpack_status - статус принятого пакета (успех, ошибка)
//      rx_crc16 - контрольная сумма принятого пакета
//      rx_len - длина принятого пакета
static uint8_t PackParse(uint8_t ch)
{
  static BinExchangeChar_t *dta;
  
  enum 
  {
    PSTATE_MAIN = 0,
    PSTATE_GET_HEADER,
    PSTATE_GET_DATA,
    PSTATE_GET_CRC
  };
  
  static uint8_t state = PSTATE_MAIN;
  
  //static uint16_t dta;
  dta = GetByte(ch);
  
  switch(state)
  {
  case PSTATE_MAIN:
    if(dta->flags & BINEX_PACK_START) //если начало пакета
    {
      rx_index = 0;
      state = PSTATE_GET_HEADER;
    }
    break;
    ////////////////////////////
  case PSTATE_GET_HEADER:
    if(dta->flags & BINEX_CHAR_DETECT) //если получили символ
    {
      if(rx_index == 0)
      {
        rx_len = dta->data;
        rx_index++;
      }
      else
      {
        rx_len |= ((dta->data) << 8);
        rx_index = 0;
        
        
        //Если длина пакета слишком большая
        if(rx_len > BINEX_BUFFLEN)
        {
          rxpack_status = RXPACK_TOO_LONG;
          state = PSTATE_MAIN;
          return 1;
        }
        
        if(rx_len > 0) //если пакет не пустой
          state = PSTATE_GET_DATA;
        else
          state = PSTATE_GET_CRC;
      }
    }
    else if(dta->flags & BINEX_PACK_START) //если обнаружили начало пакета
    {
      rx_index = 0;
      state = PSTATE_GET_HEADER;
    }
    break;
    ////////////////////////////
  case PSTATE_GET_DATA:
    if(dta->flags & BINEX_CHAR_DETECT) //если получили символ
    {
      rx_buff[rx_index] = dta->data;
      rx_index++;
      
      if(rx_index >= rx_len) //если получили все данные
      {
        rx_index = 0;
        state = PSTATE_GET_CRC;
      }
    }
    else if(dta->flags & BINEX_PACK_START) //если обнаружили начало пакета
    {
      rx_index = 0;
      state = PSTATE_GET_HEADER;
    }
    break;
    ////////////////////////////
  case PSTATE_GET_CRC:
    if(dta->flags & BINEX_CHAR_DETECT) //если получили символ
    {
      if(rx_index == 0)
      {
        rx_crc16 = dta->data;
        rx_index++;
      }
      else
      {
        rx_crc16 |= ((dta->data) << 8);
        rx_index = 0;
        
        
        //проверяем CRC
        if(Crc16(rx_buff, rx_len, Crc16Reset()) == rx_crc16)
        {
          //CRC совпала
          rxpack_status = RXPACK_OK;
          state = PSTATE_MAIN;
          return 1;
        }
        else
        {
          rxpack_status = RXPACK_CRCERR;
          state = PSTATE_MAIN;
          return 1;
        }
      }
    }
    else if(dta->flags & BINEX_PACK_START) //если обнаружили начало пакета
    {
      rx_index = 0;
      state = PSTATE_GET_HEADER;
    }
    break;
    ////////////////////////////
  }
  
  return 0;
  
}


//Процесс приемника
static void rx_proc()
{
  static uint8_t u8_tmp;
  
  switch(state_rx)
  {
  case SRX_MAIN:
    if(rx_status == BINEX_RUN)
      state_rx = SRX_RUN;
    break;
    /////////////////////////////////
  case SRX_RUN:
    u8_tmp = 0; //счетчик обработанных байт, для препятствия зависаний при непренывном потоке данных в UART
    while(RX_BUFF_NOT_EMPTY() && (u8_tmp < BINEX_BUFFLEN))
    {
      if(PackParse(GET_C()) != 0) //Если приняли пакет
      {
        if(rxpack_status == RXPACK_OK)
          rx_status = BINEX_READY;
        else
          rx_status = BINEX_ERROR;
        
        state_rx = SRX_MAIN;
        break;
      }
      u8_tmp++;
    }
    break;
    /////////////////////////////////
  }
}

/******************************************************************************/

void BinEx_Init(void)
{
  UART_INIT();
  
  
  for(int i=0; i<sizeof(tx_buff); i++)
  {
    tx_buff[i] = 0;
  }
  
  tx_status = BINEX_READY;
  tx_len = 0;
  
  state_tx = STX_MAIN;
  
  
  ////////////////////////////////////////
  
  for(int i=0; i<sizeof(rx_buff); i++)
    rx_buff[i] = 0;
  
  rxpack_status = RXPACK_OK;
  rx_len = 0;
  rx_status = BINEX_READY;
  state_rx = SRX_MAIN;
}



void BinEx_Process(void)
{
  tx_proc();
  rx_proc();
}

BinExStatus_t BinEx_TxStatus(void)
{
  return tx_status;
}

BinExRetCode_t BinEx_TxBegin(uint16_t len)
{
  if(tx_status == BINEX_READY)
  {
    tx_len = len;
    tx_status = BINEX_RUN;
    return BINEX_OK;
  }
  
  return BINEX_BUSY;
}

uint8_t *BinEx_TxGetBuffPntr(void)
{
  if(tx_status == BINEX_READY)
    return (tx_buff + TXDATA_OFFSET);
  else
    return 0;
}

/******************************************************************************/

BinExStatus_t BinEx_RxStatus(void)
{
  return rx_status;
}

BinExRxExtendedStatus_t BinEx_RxExtendedStatus(void)
{
  return rxpack_status;
}


BinExRetCode_t BinEx_RxBegin(void)
{
  if(rx_status == BINEX_READY)
  {
    rx_status = BINEX_RUN;
  }
  
  return BINEX_OK;
}


uint8_t *BinEx_RxGetBuffPntr(void)
{
  if(rx_status == BINEX_READY)
    return rx_buff;
  else 
    return 0;
}

uint16_t BinEx_RxDataLen(void)
{
  return rx_len;
}





