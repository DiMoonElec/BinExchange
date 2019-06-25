#include "fifo.h"


int8_t fifo_put(fifo_t *fifo, void *data, uint8_t offset, uint8_t count)
{
  if ((FIFO_LENGTH - fifo->count) < count)                      //Проверяем, достаточно ли места
    return -1;                                                  //Нет? Возвращаем "-1" и уходим
  for (uint8_t i = offset; i < (offset + count); i++)                //Цикл заполнения
  {
    fifo->data[fifo->tail++] =((uint8_t*) data)[i];                  //Кладем сами данные и сразу tail++
    fifo->count++;                                              //Увеличиваем переменную-счетчик
    if (fifo->tail == FIFO_LENGTH)                              //Если уперлись в границу длины
    {
      fifo->tail=0;                                             //Обнуляем хвост
    }                                                           //Т.е. "сворачиваем" буфер в кольцо
  }
  return 0;                                                     //Возвращаем "ОК"
}

int8_t fifo_get(fifo_t *fifo, void *data, uint8_t offset, uint8_t count)
{
  if (fifo->count < count)                                      //Проверяем, можем ли мы выдать столько,
    return -1;                                                  //сколько у нас просят
  for (uint8_t i = offset; i < (offset + count); i++)                //Цикл записи
  {
    ((uint8_t*)data)[i] = fifo->data[fifo->head++];                  //Пишем байт по указанному адресу, head++
    fifo->count--;                                              //Уменьшаем счетчик байт
    if (fifo->head == FIFO_LENGTH)                              //Если уперлись в границу длины
    {
      fifo->head=0;                                             //Обнуляем голову
    }
  }
  return 0;
}

uint8_t fifo_count(fifo_t *fifo)
{
  return fifo->count;
}

void fifo_init(fifo_t *fifo)
{
  fifo->head = fifo->tail = fifo->count = 0;
}