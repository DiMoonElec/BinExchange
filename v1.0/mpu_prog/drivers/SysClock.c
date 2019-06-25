#include "stm32f0xx.h"
#include <stdint.h>
#include "SysClock.h"

static int hse_error = 0;

int SysClockInit(void)
{
  __IO int StartUpCounter;
  
  hse_error = 0;
  
  RCC->CR |= RCC_CR_CSSON; //Включаем систему безопасности тактирования
  
  ////////////////////////////////////////////////////////////
  //Запускаем кварцевый генератор
  ////////////////////////////////////////////////////////////
  
  RCC->CR |= (1<<RCC_CR_HSEON_Pos); //Запускаем генератор HSE
  
  //Ждем успешного запуска или окончания тайм-аута
  for(StartUpCounter=0; ; StartUpCounter++)
  {
    //Если успешно запустилось, то 
    //выходим из цикла
    if(RCC->CR & (1<<RCC_CR_HSERDY_Pos))
      break;
    
    //Если не запустилось, то
    //отключаем все, что включили
    //и возвращаем ошибку
    if(StartUpCounter > 0x1000)
    {
      RCC->CR &= ~(1<<RCC_CR_HSEON_Pos); //Останавливаем HSE
      return 1;
    }
  }
  
  ////////////////////////////////////////////////////////////
  //Настраиваем и запускаем PLL
  ////////////////////////////////////////////////////////////
  
  //Настраиваем PLL
  RCC->CFGR2 = 0; //PREDIV input clock not divided
  RCC->CFGR |= (0x04<<RCC_CFGR_PLLMUL_Pos) //PLL множитель равен 6
            | (0x01<<RCC_CFGR_PLLSRC_Pos); //Тактирование PLL от HSE
  
  
  RCC->CR |= (1<<RCC_CR_PLLON_Pos); //Запускаем PLL
  
  //Ждем успешного запуска или окончания тайм-аута
  for(StartUpCounter=0; ; StartUpCounter++)
  {
    //Если успешно запустилось, то 
    //выходим из цикла
    if(RCC->CR & (1<<RCC_CR_PLLRDY_Pos))
      break;
    
    //Если по каким-то причинам не запустился PLL, то
    //отключаем все, что включили
    //и возвращаем ошибку
    if(StartUpCounter > 0x1000)
    {
      RCC->CR &= ~(1<<RCC_CR_HSEON_Pos); //Останавливаем HSE
      RCC->CR &= ~(1<<RCC_CR_PLLON_Pos); //Останавливаем PLL
      return 2;
    }
  }
  
  ////////////////////////////////////////////////////////////
  //Настраиваем FLASH и делители
  ////////////////////////////////////////////////////////////
  
  //Устанавливаем 1 цикла ожидания для Flash
  //так как частота ядра у нас будет 24 MHz < SYSCLK <= 48 MHz
  FLASH->ACR |= (0x01<<FLASH_ACR_LATENCY_Pos); 
  FLASH->ACR |= FLASH_ACR_PRFTBE; //Prefetch buffer enable
  
  //Делители
  RCC->CFGR |= (0x00<<RCC_CFGR_PPRE_Pos) //Делитель шины APB отключен
            | (0x00<<RCC_CFGR_HPRE_Pos); //Делитель AHB отключен
  
  
  RCC->CFGR |= (0x02<<RCC_CFGR_SW_Pos); //Переключаемся на работу от PLL
  
  //Ждем, пока переключимся
  while((RCC->CFGR & RCC_CFGR_SWS_Msk) != (0x02<<RCC_CFGR_SWS_Pos))
  {
  }
  
  //Настройка и переклбючение сисемы
  //на внешний кварцевый генератор
  //и PLL запершилось успехом.
  //Выходим
  return 0;
}




int GetHSEErrorStatus(void)
{
  return hse_error;
}




void NMI_Handler(void)
{
  if((RCC->CIR & RCC_CIR_CSSF) != 0) //если сработал CSS
  {
    RCC->CIR = RCC_CIR_CSSC; //сбрасываем флаг прерывания CSS
    hse_error = 1;
  }
}