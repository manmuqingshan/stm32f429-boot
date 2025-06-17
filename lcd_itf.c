#include "stm32f4_regs.h"
#include "gpio.h"
#include "clock.h"
#include "ili9341v.h"
#include "lcd_itf.h"
#include "spi.h"
#include "xprintf.h"

#define LCD_SPI  5

static ili9341v_dev_st s_lcd_itf_dev;

static void port_lcd_set_dcx(uint8_t val)
{
	if(val){
		gpio_write((void*)GPIOA_BASE, 'D', 13, 1);
	}else{
		gpio_write((void*)GPIOA_BASE, 'D', 13, 0);
	}
}                

static void port_lcd_set_bk(uint8_t val)
{

}

static void port_lcd_set_reset(uint8_t val)
{

}

static void port_lcd_spi_write(uint8_t* buffer, uint32_t len, int flag)
{
	spi_transfer(LCD_SPI,buffer,0,len,flag);
}    

static  int s_dma_tog = 0;
static  uint32_t s_dma_len = 0;
static  uint8_t* s_dma_buffer;
#define DMA_LEN 65535 

static void port_lcd_cb(void* param);

static void port_lcd_spi_write_dma(uint8_t* buffer, uint32_t len,void (*cb)(void* param))
{
	uint32_t sendlen;
	xprintf("start dma len:%d\r\n",len);
	s_dma_len = len;
	s_dma_tog = 0;
	s_dma_buffer = buffer;
	if(len >= DMA_LEN){
		sendlen = DMA_LEN;
	}else{
		sendlen = len;
	}
	s_dma_len -= sendlen;
	s_dma_buffer += sendlen;
	spi_transfer_dma(LCD_SPI,buffer,0,sendlen,0,port_lcd_cb,&s_lcd_itf_dev);
}   

static void port_lcd_cb(void* param){
	ili9341v_dev_st* dev = (ili9341v_dev_st*)param;
	uint32_t sendlen;
	uint8_t* buffer = s_dma_buffer;
	if(s_dma_len == 0){
		dev->enable(0);   /* 这里可能需要等待发送完，因为数据可能在FIFO和移位寄存器中 */
		if(s_lcd_itf_dev.cb != 0){
			s_lcd_itf_dev.cb(param);
		}
		/* 其他处理 */
		//xprintf("dma done\r\n");
	}else{
		if(s_dma_len >= DMA_LEN){
			sendlen = DMA_LEN;
		}else{
			sendlen = s_dma_len;
		}
		s_dma_len -= sendlen;
		s_dma_buffer += sendlen;
		spi_transfer_dma(LCD_SPI,buffer,0,sendlen,0,port_lcd_cb,&s_lcd_itf_dev);
		xprintf("dma %d\r\n",s_dma_tog++);
	}
}

static void port_lcd_spi_enable(uint8_t val)
{
	(void)val;
}    

static void port_lcd_delay_ms(uint32_t t)
{
	clock_delay(t);
}

static void port_lcd_init(void)
{
	volatile uint32_t *RCC_AHB1ENR = (void *)(RCC_BASE + 0x30);
	/* 
     * PD13 数据/命令 GPIO
	 */
	*RCC_AHB1ENR |= (1u<<3); /* GPIOD */
	*RCC_AHB1ENR |= (1u<<2); /* GPIOC */

	gpio_set((void*)GPIOA_BASE, 'D', 13, 0, GPIOx_MODER_MODERy_GPOUTPUT,GPIOx_OSPEEDR_OSPEEDRy_HIGH, GPIOx_PUPDR_PULLUP);
	gpio_write((void*)GPIOA_BASE, 'D', 13, 1);
	
	spi_cfg_st cfg={
		.baud = 90000000ul,
		.databits = 8,
		.mode = 3,
		.msb = 1,
	};
	spi_init(LCD_SPI, &cfg);

	port_lcd_set_bk(0);
	port_lcd_set_dcx(1);
	
	port_lcd_set_reset(0);
	/* 延时 */
	port_lcd_set_reset(1);
	/* 延时 */
	port_lcd_set_bk(1);
}           

static void port_lcd_deinit(void)
{

}        
    


/******************************************************************************
 *                        以下是LCD设备实例
 * 
******************************************************************************/

/* 设备实例 */
static ili9341v_dev_st s_lcd_itf_dev =
{
    .set_dcx = port_lcd_set_dcx,
    .set_reset = port_lcd_set_reset,
    .write = port_lcd_spi_write,
	.write_dma = port_lcd_spi_write_dma,
    .enable = port_lcd_spi_enable,
    .delay = port_lcd_delay_ms,
    .init = port_lcd_init,
    .deinit = port_lcd_deinit,

    .buffer = (uint16_t*)(0x94000000-0x100000),  /* 最后1M作为显存 */
};

/******************************************************************************
 *                        以下是对外操作接口
 * 
******************************************************************************/


/**
 * \fn lcd_itf_init
 * 初始化
 * \retval 0 成功
 * \retval 其他值 失败
*/
int lcd_itf_init(void)
{
    return ili9341v_init(&s_lcd_itf_dev);
}

/**
 * \fn lcd_itf_deinit
 * 解除初始化
 * \retval 0 成功
 * \retval 其他值 失败
*/
int lcd_itf_deinit(void)
{
    return ili9341v_deinit(&s_lcd_itf_dev);
}

/**
 * \fn lcd_itf_sync
 * 刷新显示
 * \retval 0 成功
 * \retval 其他值 失败
*/
int lcd_itf_sync(void)
{
    return ili9341v_sync(&s_lcd_itf_dev, 0, LCD_HSIZE-1, 0, LCD_VSIZE-1, s_lcd_itf_dev.buffer, LCD_HSIZE*LCD_VSIZE*2);
}


/**
 * \fn lcd_itf_set_cb
 * 设置回调函数
*/
void lcd_itf_set_cb(void (*cb)(void* param)){
	s_lcd_itf_dev.cb = cb;
}

/**
 * \fn lcd_itf_sync_dma
 * 刷新显示 dma方式
 * \retval 0 成功
 * \retval 其他值 失败
*/
int lcd_itf_sync_dma(void)
{
    return ili9341v_sync_dma(&s_lcd_itf_dev, 0, LCD_HSIZE-1, 0, LCD_VSIZE-1, s_lcd_itf_dev.buffer, LCD_HSIZE*LCD_VSIZE*2);
}


/**
 * \fn lcd_itf_set_pixel
 * 写点
 * \param[in] x x坐标位置
 * \param[in] y y坐标位置
 * \param[in] rgb565 颜色
*/
void lcd_itf_set_pixel(uint16_t x, uint16_t y, uint16_t rgb565)
{
    //if(x >= LCD_HSIZE)
    //{
    //    return -1;
    //}
    //if(y >= LCD_VSIZE)
    //{
    //    return -1;
    //}
    s_lcd_itf_dev.buffer[y*LCD_HSIZE + x] = rgb565;
}

/**
 * \fn lcd_itf_set_pixel_0
 * 写点
 * \param[in] offset 偏移位置
 * \param[in] rgb565 颜色
*/
void lcd_itf_set_pixel_0(uint32_t offset, uint16_t rgb565)
{
    s_lcd_itf_dev.buffer[offset] = rgb565;
}

/**
 * \fn lcd_itf_get_pixel
 * 读点
 * \param[in] x x坐标位置
 * \param[in] y y坐标位置
 * \return rgb565颜色
*/
uint16_t lcd_itf_get_pixel(uint16_t x, uint16_t y)
{
    uint16_t color = s_lcd_itf_dev.buffer[y*LCD_HSIZE + x]; 
    return color;
}

/**
 * \fn lcd_itf_fill_direct
 * 直接填充区域
 * \param[in] x x开始坐标位置
 * \param[in] w 宽度
 * \param[in] y y开始坐标位置
 * \param[in] h 高度
 * \param[in] buffer rgb565颜色缓存区
*/
void lcd_itf_fill_direct(uint16_t x, uint16_t w, uint16_t y, uint16_t h, uint16_t* buffer)
{
		ili9341v_sync(&s_lcd_itf_dev, x, x+w-1, y, y+h-1, buffer, w*h*2);
}

/**
 * \fn lcd_itf_fill
 * 填充区域为指定颜色
 * \param[in] x x开始坐标位置
 * \param[in] w 宽度
 * \param[in] y y开始坐标位置
 * \param[in] h 高度
 * \param[in] buffer rgb565颜色缓存区
*/
void lcd_itf_fill(uint16_t x, uint16_t w, uint16_t y, uint16_t h, uint16_t rgb)
{
	uint16_t* s = s_lcd_itf_dev.buffer; 
	uint16_t* p;
	for(int i=0; i<h; i++){
		p = s + (y+i)*LCD_HSIZE + x;
		for(int j=0; j<w; j++){
			*p++ = rgb;
		}
	}
}

/**
 * \fn lcd_itf_set_pixel_direct
 * 直接写点
 * \param[in] x x坐标位置
 * \param[in] y y坐标位置
 * \param[in] rgb565 颜色
*/
void lcd_itf_set_pixel_direct(uint16_t x, uint16_t y, uint16_t rgb565)
{
		uint16_t tmp = rgb565;
		ili9341v_sync(&s_lcd_itf_dev, x, x, y, y, &tmp, 2);
}
