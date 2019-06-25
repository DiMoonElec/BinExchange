#include "stm32f0xx.h"
#include <stdint.h>
#include "uart.h"
#include "fifo.h"

/*
#define UART_RXBUFF_LEN 16
#define UART_TXBUFF_LEN 16
*/

static fifo_t rx_data;
static fifo_t tx_data;

/****************************************************************/

#define USARTx                  USART1
#define USARTx_IRQn             USART1_IRQn
#define USARTx_IRQHandler       USART1_IRQHandler


/****************************************************************/
#define UART_TXEIE_EN()         USARTx->CR1 |= USART_CR1_TXEIE
#define UART_RXNEIE_EN()        USARTx->CR1 |= USART_CR1_RXNEIE

#define UART_TXEIE_DIS()        USARTx->CR1 &= ~USART_CR1_TXEIE
#define UART_RXNEIE_DIS()       USARTx->CR1 &= ~USART_CR1_RXNEIE

/****************************************************************/

/*
void UART_Deinit(void)
{
  
  RCC->AHBRSTR |= RCC_AHBRSTR_GPIOARST;
  RCC->APB2RSTR |= RCC_APB2RSTR_USART1RST;
  
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  
  RCC->AHBRSTR &= ~(RCC_AHBRSTR_GPIOARST);
  RCC->APB2RSTR &= ~(RCC_APB2RSTR_USART1RST);
}
*/

void UART_Init(void)
{
  uint32_t tmp;
  
  /**********************/
  //FIFO Init
  fifo_init(&rx_data);
  fifo_init(&tx_data);
  
  /**********************/
  RCC->AHBENR |= RCC_AHBENR_GPIOAEN; //Тактирование GPIOA
  RCC->APB2ENR |= RCC_APB2ENR_USART1EN; //Тактирование USART1
  
  /**********************/
  //GPIO INIT
  //PA9 - TX1
  //PA10 - RX1
  //Alternate function: 1 (USART1), see DataSheet Table 12 Page 33
  tmp = GPIOA->AFR[1];
  tmp &= ~(GPIO_AFRH_AFSEL9 | GPIO_AFRH_AFSEL10);
  tmp |= (1 << GPIO_AFRH_AFSEL9_Pos)
       | (1 << GPIO_AFRH_AFSEL10_Pos);
  GPIOA->AFR[1] = tmp;
  
  //PA3 (RX): Pull-up
  tmp = GPIOA->PUPDR;
  tmp &= ~(GPIO_PUPDR_PUPDR10);
  tmp |= 0x01 << GPIO_PUPDR_PUPDR10_Pos;
  GPIOA->PUPDR = tmp;
  
  //mode: 10: Alternate function mode
  tmp = GPIOA->MODER;
  tmp &= ~(GPIO_MODER_MODER9 | GPIO_MODER_MODER10);
  tmp |= (0x02 << GPIO_MODER_MODER9_Pos)
       | (0x02 << GPIO_MODER_MODER10_Pos);
  GPIOA->MODER = tmp;
  
  /**********************/
  //UART INIT
  RCC->CFGR3 &= ~(RCC_CFGR3_USART1SW); //0x00: PCLK selected as USART1 clock source (default)
  
  USARTx->BRR = 0x1388; //0x0341; //9600, Fclk=8MHz, See RM Table 87 Page 610
  
  USARTx->CR2 |= USART_CR2_SWAP;
  
  USARTx->CR1 = USART_CR1_UE //USART enabled
    | USART_CR1_TE //Transmitter enable
    | USART_CR1_RE //Receiver enable
    | USART_CR1_TXEIE //Interrupt TX Empty
    | USART_CR1_RXNEIE; //Interrupt RX No Empty
  
  NVIC_EnableIRQ(USARTx_IRQn); //enable USART interrupts
}


#if 0
void UART_Deinit(void)
{
  
  USARTx->CR1 &= ~(USART_CR1_UE
                 | USART_CR1_TE
                 | USART_CR1_RE);
  
  uint32_t tmp;
  
  tmp = GPIOA->MODER;
  tmp &= ~(GPIO_MODER_MODER2 | GPIO_MODER_MODER3); //input mode
  GPIOA->MODER = tmp;
}
#endif

int UART_Get(void)
{
  int tmp;
  NVIC_DisableIRQ(USARTx_IRQn);
  
  if(fifo_get(&rx_data, &tmp, 0, 1) == -1)
    tmp = -1;
      
  NVIC_EnableIRQ(USARTx_IRQn);
  
  return tmp;
}

int UART_Put(uint8_t c)
{
  int ret = 0;
  
  NVIC_DisableIRQ(USARTx_IRQn);
  
  ret = fifo_put(&tx_data, &c, 0, 1);
  UART_TXEIE_EN();
  
  NVIC_EnableIRQ(USARTx_IRQn);
  
  return ret;
}

void UART_RxClear(void)
{
  NVIC_DisableIRQ(USARTx_IRQn);
  
  fifo_init(&rx_data);
  
  NVIC_EnableIRQ(USARTx_IRQn);
}

uint8_t UART_RxCount(void)
{
  uint8_t ret;
  NVIC_DisableIRQ(USARTx_IRQn);
  
  ret = fifo_count(&rx_data);
  
  NVIC_EnableIRQ(USARTx_IRQn);
  
  return ret;
}

uint8_t UART_NotTxNum(void)
{
  uint8_t ret;
  NVIC_DisableIRQ(USARTx_IRQn);
  
  ret = fifo_count(&tx_data);
  
  NVIC_EnableIRQ(USARTx_IRQn);
  
  return ret;
}

uint8_t UART_TxBuffNumFree(void)
{
  uint8_t ret;
  NVIC_DisableIRQ(USARTx_IRQn);
  
  ret = fifo_count(&tx_data);
  
  NVIC_EnableIRQ(USARTx_IRQn);
  
  return FIFO_LENGTH - ret; 
}

/*
void UartIntDisable(void)
{
  NVIC_DisableIRQ(USARTx_IRQn);
}

void UartIntEnable(void)
{
  NVIC_EnableIRQ(USARTx_IRQn);
}
*/

/*****************************************************/

void USARTx_IRQHandler(void)
{
  static uint8_t tmp;
  
  if((USARTx->CR1 & USART_CR1_RXNEIE) 
   &&(USARTx->ISR & USART_ISR_RXNE)) //если что-то пришло
  {
    tmp = USARTx->RDR;
    fifo_put(&rx_data, &tmp, 0, 1);
  }
  
  if((USARTx->CR1 & USART_CR1_TXEIE)
   &&(USARTx->ISR & USART_ISR_TXE)) //если все отправили и есть что еще отправить
   {
     if(fifo_get(&tx_data, &tmp, 0, 1) == -1)
     {
       UART_TXEIE_DIS();
     }
     else
     {
       USARTx->TDR = tmp;
     }
   }
}
