#include "string.h"
#include <stdlib.h>
#include <stdio.h>
#include "spiflash.h"
#include "xprintf.h"

#define FLASH_CMD_WEL 0x06
#define FLASH_CMD_PAGEPROGRAM 0x02
#define FLASH_CMD_READ 0x03
#define FLASH_CMD_ERASESECTOR 0x20
#define FLASH_CMD_READSR1 0x05
#define FLASH_CMD_READSR2 0x35
#define FLASH_CMD_READSR3 0x15

#define FLASH_CMD_WRITESR1 0x01
#define FLASH_CMD_WRITESR2 0x31
#define FLASH_CMD_WRITESR3 0x11

#define FLASH_SR1_BUSY  (1 << 0)

int flash_read_sr1(flash_dev_st* dev, uint8_t* sr)
{
	uint8_t cmd[2] = {FLASH_CMD_READSR1,0xFF};
	uint8_t rx[2];
	dev->spi_trans(cmd, rx, 2, 1);
	*sr = rx[1];
	return 0;
}

int flash_write_enable(flash_dev_st* dev)
{
	uint8_t sr;
	int timeout = 10000;
	uint8_t cmd = FLASH_CMD_WEL;
	dev->spi_trans(&cmd, 0, 1, 1);
	do{
		flash_read_sr1(dev, &sr);
	}while(((sr & 0x02) == 0) && (timeout--));
	if(timeout <= 0){
		return -1;
	}else{
		return 0;
	}
}

int flash_read_sr2(flash_dev_st* dev, uint8_t* sr)
{
	uint8_t cmd[2] = {FLASH_CMD_READSR2,0xFF};
	uint8_t rx[2];
	dev->spi_trans(cmd, rx, 2, 1);
	*sr = rx[1];
	return 0;
}

int flash_read_sr3(flash_dev_st* dev, uint8_t* sr)
{
	uint8_t cmd[2] = {FLASH_CMD_READSR3,0xFF};
	uint8_t rx[2];
	dev->spi_trans(cmd, rx, 2, 1);
	*sr = rx[1];
	return 0;
}


int flash_write_sr1(flash_dev_st* dev, uint8_t val)
{
	uint8_t cmd[2] = {FLASH_CMD_WRITESR1,0};
	cmd[1] = val;
	dev->spi_trans(cmd, 0, 2, 1);
	return 0;
}

int flash_write_sr2(flash_dev_st* dev, uint8_t val)
{
	uint8_t cmd[2] = {FLASH_CMD_WRITESR2,0};
	cmd[1] = val;
	dev->spi_trans(cmd, 0, 2, 1);
	return 0;
}


int flash_write_sr3(flash_dev_st* dev, uint8_t val)
{
	uint8_t cmd[2] = {FLASH_CMD_WRITESR3,0};
	cmd[1] = val;
	dev->spi_trans(cmd, 0, 2, 1);
	return 0;
}


int flash_wait_busy(flash_dev_st* dev)
{
	uint8_t sr = 0;
	do{
		flash_read_sr1(dev, &sr);
	} while(FLASH_SR1_BUSY & sr);
	return 0;
}


int flash_erase_sector(flash_dev_st* dev, uint32_t addr)
{
	uint8_t cmd[4];
	flash_write_enable(dev);
    cmd[0] = FLASH_CMD_ERASESECTOR;
	cmd[1] = (uint8_t)(addr >> 16 & 0xFF);
	cmd[2] = (uint8_t)(addr >> 8  & 0xFF);
	cmd[3] = (uint8_t)(addr >> 0  & 0xFF);
	dev->spi_trans(cmd, 0, 4, 1);
	flash_wait_busy(dev);
	return 0;
}


int flash_pageprogram(flash_dev_st* dev, uint8_t* buffer, uint32_t addr, uint32_t len)
{
	uint8_t cmd[4];
    cmd[0] = FLASH_CMD_PAGEPROGRAM;
	cmd[1] = (uint8_t)(addr >> 16 & 0xFF);
	cmd[2] = (uint8_t)(addr >> 8  & 0xFF);
	cmd[3] = (uint8_t)(addr >> 0  & 0xFF);
	flash_write_enable(dev);
	dev->spi_trans(cmd, 0, 4, 0);
	dev->spi_trans(buffer, 0, len, 1);
	flash_wait_busy(dev);
	return 0;
}
	

uint32_t flash_read(flash_dev_st* dev, uint8_t* buffer, uint32_t addr, uint32_t len)
{
    uint8_t cmd[4];
    cmd[0] = FLASH_CMD_READ;
    cmd[1] = (uint8_t)(addr >> 16 & 0xFF);
    cmd[2] = (uint8_t)(addr >> 8  & 0xFF);
    cmd[3] = (uint8_t)(addr >> 0  & 0xFF);
	dev->spi_trans(cmd, 0, 4, 0);
	dev->spi_trans(0, buffer, len, 1);
	return len;
}


uint32_t flash_write(flash_dev_st* dev, uint8_t* buffer, uint32_t addr, uint32_t len)
{
   /** 
	*   |                     |                    |                  |                        |
	*             |                                                                   |
	*             <                           len                                     >      
    *             addr
	*   <sec_head >                                                    <  sec_tail  >
	*   start_addr                                                     end_addr
    */
	uint32_t sec_head;
	uint32_t sec_tail = 0;
	uint32_t sec_mid_num;
	uint32_t start_addr;
	uint32_t end_addr;
	
	uint32_t fill;
	
	start_addr = addr & (~(dev->sector_size-1)); /* 开始对齐地址 */
	end_addr = (addr+len) & (~(dev->sector_size-1)); /* 结束对齐地址 */
	sec_head = addr & (dev->sector_size-1);   /* 开始地址到开始对齐地址的距离 */
	sec_tail = (addr + len) & (dev->sector_size-1); /* 结束地址到结束对齐地址的距离 */
	xprintf("start_addr:0x%x,sec_head:0x%x,end_addr:0x%x,sec_tail:0x%x\r\n",
		start_addr,sec_head,end_addr,sec_tail);
	if(sec_head == 0){
		if(sec_tail == 0){
			/* head和tail都是0,即都是对齐的
			 * 此时sec_mid_num不可能为0, 因为len不为0
			 */
			sec_mid_num = (end_addr - start_addr) >> dev->sector_bits;
		}else{
			/* head对齐，tail不对齐 
			 * 此时sec_mid_num可能为0，开始结束位于同一个sec
			 */
			sec_mid_num = (end_addr - start_addr) >> dev->sector_bits;
		}
	}else{
		if(sec_tail == 0){
			/* head不对齐，tail对齐 
			 * 此时sec_mid_num不可能为0，因为len不为0
			 */
			sec_mid_num = (end_addr - start_addr) >> dev->sector_bits;
			sec_mid_num--;
		}else{
			/* head不对齐，tail不对齐 
			 * 此时sec_mid_num可能为0,开始结束位于同一个sec
			 */
			sec_mid_num = (end_addr - start_addr) >> dev->sector_bits;
			if(sec_mid_num > 0){
				sec_mid_num--;
			}else{
				/* head tail可能位于同一个sector 且都不对齐
				 * 此时其实不需要head和tail都进行读出-修改-写入，可以特殊处理，在tail处理时进行判断
				 */
			}
		}
	}
	xprintf("sec_mid_num=%d\r\n",sec_mid_num);
	/* head */
	if(sec_head > 0)
    {
		/* 读出sec_head部分，这部分不修改  */
		flash_read(dev,dev->buffer, start_addr, sec_head); 
		/* 填充sec_head到sec末尾部分，如果len不足则只填充len部分 */
		fill =( len > (dev->sector_size-sec_head)) ? (dev->sector_size-sec_head) : len;
		memcpy(dev->buffer+sec_head,buffer,fill);
		/* 擦除-编程 */
		flash_erase_sector(dev,start_addr);
        for (uint32_t j=0; j<16; j++) {
			flash_pageprogram(dev,dev->buffer + (j<<dev->page_bits),start_addr + (j<<dev->page_bits), dev->page_size);  
    	}
		xprintf("write head 0x%x,len %d\r\n",start_addr+sec_head,fill);
		buffer += fill;   /* buffer偏移 */
		/* 定位到下一个sec，这里理论上应该是start_addr+sec_head+fill 
		 * 这里直接偏移到dev->sector_size也没关系，因为如果fill不到下一个sec，说明后续就没有了
		 */
		start_addr += dev->sector_size;
	}
	/* mid */
	for(uint32_t i=0; i<sec_mid_num; i++)
    {
		/* sec 擦除->编程 */
	    flash_erase_sector(dev,start_addr);
        for (uint32_t j=0; j<16; j++) {
			flash_pageprogram(dev, buffer + (j<<dev->page_bits), start_addr + (j<<dev->page_bits), dev->page_size);  
        }
		xprintf("write mid 0x%x\r\n",start_addr);
		/* buffer和start_addr偏移sec大小 */
		buffer += dev->sector_size;
		start_addr += dev->sector_size;
	}
	/* tail */
	if((sec_mid_num == 0) && (sec_head != 0) && (sec_tail != 0)){
		return len; /* head tail位于同一个sec,且都不对齐则前面sec_head已经处理,这里无需再处理 */
	}
	if(sec_tail > 0)
    {
		/* 读出tail到sec末尾部分，这部分不修改 */
		flash_read(dev,dev->buffer+sec_tail, start_addr+sec_tail, dev->sector_size - sec_tail); 
		memcpy(dev->buffer,buffer,sec_tail);
		/* 擦除->编程 */
		flash_erase_sector(dev,start_addr);
        for (uint32_t j=0; j<16; j++) {
			flash_pageprogram(dev, dev->buffer + (j<<dev->page_bits),start_addr + (j<<dev->page_bits), dev->page_size);  
        }
		xprintf("write tail 0x%x,len:%d\r\n",start_addr,sec_tail);
	}
	return len;
}

