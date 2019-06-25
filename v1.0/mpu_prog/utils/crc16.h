#ifndef __CRC16_H__
#define __CRC16_H__

#include <stdint.h>

uint16_t Crc16Reset(void);
uint16_t Crc16(uint8_t * pcBlock, uint16_t len, uint16_t crc);

#endif
