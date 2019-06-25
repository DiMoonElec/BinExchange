#ifndef __UART_H__
#define __UART_H__

#include <stdint.h>


//Инициализация UART
void UART_Init(void);

//void UART_Deinit(void);

//Положить символ в буфер
int UART_Put(uint8_t c);

//Получить символ из буфера
int UART_Get(void);

//Очистить буфер приемника
void UART_RxClear(void);

//Кол-во принятых байт
uint8_t UART_RxCount(void);

//Количество еще не переданных байт
uint8_t UART_NotTxNum(void);

//Сколько байт свободно в буфере передатчика
uint8_t UART_TxBuffNumFree(void);

/*
void UartIntDisable(void);
void UartIntEnable(void);
*/
#endif
