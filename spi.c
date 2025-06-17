#include "stm32f429xx.h"
//#include "stm32f4_regs.h"
#include "gpio.h"
#include "clock.h"
#include "spi.h"
#include "dma.h"
#include "xprintf.h"


static uint32_t reg_base[6]={0x40013000,0x40003800,0x40003C00,0x40013400,0x40015000,0x40015400};

/**
 * SPI5_TX可走 CH2 STREAM4 或者
 *             CH7 STREAM6 */
#define SPI5_TX_DMA DMA2_BASE
#define SPI5_TX_DMA_CH 2
#define SPI5_TX_DMA_STREAM 4

void spi_init(int id, spi_cfg_st* cfg){
	
	volatile uint16_t* cr1 = (uint16_t*)(reg_base[id-1] + 0x00);
	//volatile uint16_t* cr2 = (uint16_t*)(reg_base[id-1] + 0x04);
	volatile uint32_t *RCC_APB1ENR = (void *)(RCC_BASE + 0x40);
	volatile uint32_t *RCC_APB2ENR = (void *)(RCC_BASE + 0x44);
	volatile uint32_t *RCC_AHB1ENR = (void *)(RCC_BASE + 0x30);

	switch(id){
		case 1:
			/* 引脚时钟使能 引脚初始化 
			* PA4  SPI1_NSS  GPIO
			* PA5  SPI1_SCK  AF5
			* PA6  SPI1_MISO AF5 
			* PA7  SPI1_MOSI AF5
			*/
			*RCC_AHB1ENR |= (1u<<0); /* GPIOA */
			gpio_set((void*)GPIOA_BASE, 'A', 4, 0, GPIOx_MODER_MODERy_GPOUTPUT,GPIOx_OSPEEDR_OSPEEDRy_HIGH, GPIOx_PUPDR_PULLUP);
			gpio_write((void*)GPIOA_BASE, 'A', 4, 1);	
			gpio_set_alt((void*)GPIOA_BASE, 'A', 5, 0, GPIOx_OSPEEDR_OSPEEDRy_HIGH, 0, 0x5);
			gpio_set_alt((void*)GPIOA_BASE, 'A', 6, 0, GPIOx_OSPEEDR_OSPEEDRy_HIGH, 0, 0x5);
			gpio_set_alt((void*)GPIOA_BASE, 'A', 7, 0, GPIOx_OSPEEDR_OSPEEDRy_HIGH, 0, 0x5);
		break;
		case 5:
			/**
			 * PF7 SPI5_SCK   AF5
			 * PF9 SPI5_MOSI  AF5
			 * PC2 CS  GPIO
			 * PD13 数据/命令 GPIO
			 */
			*RCC_AHB1ENR |= (1u<<5); /* GPIOF */
			gpio_set_alt((void*)GPIOA_BASE, 'F', 7, 0, GPIOx_OSPEEDR_OSPEEDRy_HIGH, 0, 0x5);
			gpio_set_alt((void*)GPIOA_BASE, 'F', 9, 0, GPIOx_OSPEEDR_OSPEEDRy_HIGH, 0, 0x5);

			*RCC_AHB1ENR |= (1u<<2); /* GPIOC */
			gpio_set((void*)GPIOA_BASE, 'C', 2, 0, GPIOx_MODER_MODERy_GPOUTPUT,GPIOx_OSPEEDR_OSPEEDRy_HIGH, GPIOx_PUPDR_PULLUP);
			gpio_write((void*)GPIOA_BASE, 'C', 2, 1);
		break;
	}

	/* SPI时钟初始化 */
	if(id==1){
		*RCC_APB2ENR |= (1u<<12); /* SPI1 */
	}else if(id == 2){
		*RCC_APB1ENR |= (1u<<14); /* SPI2 */
	}else if(id == 3){
		*RCC_APB1ENR |= (1u<<15); /* SPI3 */
	}else if(id == 5){
		*RCC_APB2ENR |= (1u<<20); /* SPI5 */
	}
	/* SPI配置 */
	uint16_t tmp = 0;
	uint32_t div = 0;
	if(cfg->databits == 16){
		tmp |= (1u<<11); /* 0: 8-bit data frame format is selected for transmission/reception 1: 16-bit */
	}
	tmp |= (1u<<9); /* 1: Software slave management enabled */
	tmp |= (1u<<8);
	if(cfg->msb == 0){
		tmp |= (1u<<7);  /* 0: MSB transmitted first 1: LSB transmitted first */
	}
	tmp |= (1u<<6);  /* 1: Peripheral enabled */
	if((id == 1) || (id == 5)){
		div = clock_get_apb2() / cfg->baud;
	}else{
		div = clock_get_apb1() / cfg->baud;	
	}
	/* 5:3 */
	if(div <= 2){
		tmp |= (0u<<3);
	}else if(div <= 4){
		tmp |= (1u<<3);
	}else if(div <= 8){
		tmp |= (2u<<3);
	}else if(div <= 16){
		tmp |= (3u<<3);
	}else if(div <= 32){
		tmp |= (4u<<3);
	}else if(div <= 64){
		tmp |= (5u<<3);
	}else if(div <= 128){
		tmp |= (6u<<3);
	}else{
		tmp |= (7u<<3);	
	}
	tmp |= (1u<<2);  /* 1: Master configuration */
	if(cfg->mode & 0x02){
		tmp |= (1u<<1);  /* 0: CK to 0 when idle 1: CK to 1 when idle */
	}
	if(cfg->mode & 0x01){
		tmp |= (1u<<0);  /* 0: The first clock transition is the first data capture edge 1: The second clock transition is the first data capture edge */
	}	
	*cr1 = tmp;
}

uint32_t spi_transfer(int id, uint8_t* tx, uint8_t* rx, uint32_t len, int flag)
{
	volatile uint16_t* sr = (uint16_t*)(reg_base[id-1] + 0x08);
	volatile uint16_t* dr = (uint16_t*)(reg_base[id-1] + 0x0C);
    uint8_t rx_tmp;
	uint8_t tx_tmp = 0xFF;
	/* CS拉低 */
	switch(id){
		case 1:
		gpio_write((void*)GPIOA_BASE, 'A', 4, 0);
		break;
		case 5:
		gpio_write((void*)GPIOA_BASE, 'C', 2, 0);
		break;
	}	
	for(uint32_t i=0; i<len; i++){
		if(tx != (uint8_t*)0){
            *dr = (uint16_t)tx[i];
		}else{
			*dr = (uint16_t)tx_tmp; /* 无tx也要发送数据 */
		}
		while((*sr & (uint16_t)0x01) == 0); /* wait RXNE */
		rx_tmp = (uint8_t)*dr;
		if(rx != (uint8_t*)0){
			rx[i] = rx_tmp;
		}
	}

	if(flag){
		/* CS拉高 */
		switch(id){
			case 1:
			gpio_write((void*)GPIOA_BASE, 'A', 4, 1);
			break;
			case 5:
			gpio_write((void*)GPIOA_BASE, 'C', 2, 1);
			break;
		}	
	}
    return len;
}

static  dma_st s_dma_cfg={

};

void (*spi5_tx_dma_cb)(void* param) = 0;
void *spi5_tx_dma_param = 0;

uint32_t spi_transfer_dma(int id, uint8_t* tx, uint8_t* rx, uint32_t len, int flag,void (*cb)(void* param), void* param)
{
	/* CS拉低 */
	switch(id){
		case 1:
		gpio_write((void*)GPIOA_BASE, 'A', 4, 0);
		break;
		case 5:
		gpio_write((void*)GPIOA_BASE, 'C', 2, 0);
		break;
	}	

	/* 准备DMA */
	switch(id)
	{
		case 5:
			{
			spi5_tx_dma_cb = cb;
			spi5_tx_dma_param = param;

			s_dma_cfg.cnt = len;
			s_dma_cfg.m0addr = (uint32_t)tx;
			s_dma_cfg.m1addr = (uint32_t)tx;
			s_dma_cfg.paddr = reg_base[id-1] + 0x0C;  /* SPI_DR */
			s_dma_cfg.fifo_cfg.dmdis = 1;  /* 使用FIFO  源和目的数据宽度不一样时必须使用FIFO */
			s_dma_cfg.fifo_cfg.fth = DMA_FIFO_FTH_1_2;
			s_dma_cfg.fifo_cfg.feie = 0;
			s_dma_cfg.cfg.chsel = SPI5_TX_DMA_CH;
			s_dma_cfg.cfg.circ = 0;
			s_dma_cfg.cfg.ct = 0;
			s_dma_cfg.cfg.dbm = 0;
			s_dma_cfg.cfg.dir = DMA_DIR_M2P;
			s_dma_cfg.cfg.dmeie = 0;
			s_dma_cfg.cfg.htie = 0;
			s_dma_cfg.cfg.mburst = DMA_MBURST_SINGLE;
			s_dma_cfg.cfg.minc = DMA_MINC_MSIZE;
			s_dma_cfg.cfg.msize = DMA_MSIZE_BYTE;
			s_dma_cfg.cfg.pburst = DMA_PBURST_SINGLE;
			s_dma_cfg.cfg.pfctrl = DMA_PFCTRL_DMA;
			s_dma_cfg.cfg.pinc = DMA_PINC_FIXED;
			s_dma_cfg.cfg.pincos = DMA_PINCOS_PSIZE;
			s_dma_cfg.cfg.pl = DMA_PL_VERYHIGH;
			s_dma_cfg.cfg.psize = DMA_PSIZE_BYTE;
			s_dma_cfg.cfg.tcie = 1;
			s_dma_cfg.cfg.teie = 0;

			dma_cfg(SPI5_TX_DMA, SPI5_TX_DMA_STREAM, &s_dma_cfg);
			dma_int_flag_clr(SPI5_TX_DMA, SPI5_TX_DMA_STREAM, DMA_INT_TC);
			//NVIC_SetPriority(DMA2_Stream4_IRQn,2,0);
			NVIC_EnableIRQ(DMA2_Stream4_IRQn);

			volatile uint16_t* cr2 = (uint16_t*)(reg_base[id-1] + 0x04);
			*cr2 |= 0x02; /* TXDMAEN */
			}
		break;
	}

	if(flag){
		/* CS拉高 */
		switch(id){
			case 1:
			gpio_write((void*)GPIOA_BASE, 'A', 4, 1);
			break;
			case 5:
			gpio_write((void*)GPIOA_BASE, 'C', 2, 1);
			break;
		}	
	}
    return len;
}

void dma2_stream4_irqhandler(void){
	//xprintf("dma2_stream4_irqhandler\r\n");
	if(dma_int_is_set(SPI5_TX_DMA, SPI5_TX_DMA_STREAM, DMA_INT_TC)){
		//xprintf("tc\r\n");
		dma_int_flag_clr(SPI5_TX_DMA, SPI5_TX_DMA_STREAM, DMA_INT_TC);
		if(spi5_tx_dma_cb != 0){
			spi5_tx_dma_cb(spi5_tx_dma_param);
		}
	}
}