#ifndef SPI_H
#define SPI_H

#include <stdint.h>

typedef struct{
	uint8_t databits; /** 8 or 16 */
	uint8_t msb;      /** 1:msb 0:lsb */
	uint8_t mode;     /** 0,1,2,3 */
	uint32_t baud;    
} spi_cfg_st;

void spi_init(int id, spi_cfg_st* cfg);
uint32_t spi_transfer(int id, uint8_t* tx, uint8_t* rx, uint32_t len, int flag);
uint32_t spi_transfer_dma(int id, uint8_t* tx, uint8_t* rx, uint32_t len, int flag,void (*cb)(void* param), void* param);
void dma2_stream4_irqhandler(void);

#endif 
