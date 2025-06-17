#include <stdlib.h>
#include <stdint.h>

#include "stm32f4_regs.h"
#include "clock.h"
#include "uart.h"
#include "gpio.h"
#include "start_kernel.h"
#include "sdram.h"
#include "xprintf.h"
#include "memtester.h"
#include "spiflash_itf.h"
#include "shell.h"
#include "shell_func.h"
#include "load.h"
#include "lcd_test.h"
#include "lcd_itf.h"

#include "spi.h"

#if defined(USE_IS42S16320F)
	#define SDRAM_SIZE (64ul*1024ul*1024ul)
#else
	#define SDRAM_SIZE (8ul*1024ul*1024ul)
#endif

static void *gpio_base = (void *)GPIOA_BASE;

static void xprintf_out(int ch){
	uint8_t c=(uint8_t)ch;
	uart_send(1, &c, 1);
}

#if 0
static void spiflash_test(void){
	uint8_t buffer[16];
	for(int i=0;i<sizeof(buffer);i++){
		buffer[i] = (uint8_t)i;
	}
	flash_itf_write(buffer, 0, sizeof(buffer));
	for(int i=0;i<sizeof(buffer);i++){
		buffer[i] = 0;
	}
	flash_itf_read(buffer, 0, sizeof(buffer));
	for(int i=0;i<sizeof(buffer);i++){
		if(buffer[i] != (uint8_t)i){
			xprintf("spiflash test err\r\n");
			return;
		}
	}
	xprintf("spiflash test ok\r\n");
}
#endif

static uint32_t shell_read(uint8_t *buff, uint32_t len)
{
	return uart_read(1,buff,len);
}

static void shell_write(uint8_t *buff, uint32_t len)
{
	uart_send(1,buff,len);
}

static int wait_key(uint32_t timeout){
	uint32_t cur;
	uint32_t pre = get_ticks();
	uint8_t c;
	xprintf("Press Enter Key To Go To SHELL...\r\n");
	while(1){
		if(uart_read(1,&c,1) > 0){
			if(c == 0x0D){
				return 1;
			}
		}
		cur = get_ticks();
		if((cur - pre) >= timeout*1000){
			return 0;
		}
	}
}

static volatile int s_logo_sync_done = 0;  /* 必须加volatile 否则后面的while()判断永远不会满足 */
void logo_sync_dnoe(void* param){
    xprintf("logo sync done\r\n");
    s_logo_sync_done = 1;
}

static int user_main(void)
{
	volatile uint32_t *FLASH_KEYR = (void *)(FLASH_BASE + 0x04);
	volatile uint32_t *FLASH_CR = (void *)(FLASH_BASE + 0x10);

	if (*FLASH_CR & FLASH_CR_LOCK) {
		*FLASH_KEYR = 0x45670123;
		*FLASH_KEYR = 0xCDEF89AB;
	}
	*FLASH_CR &= ~(FLASH_CR_ERRIE | FLASH_CR_EOPIE | FLASH_CR_PSIZE_MASK);
	*FLASH_CR |= FLASH_CR_PSIZE_X32;
	*FLASH_CR |= FLASH_CR_LOCK;

	xdev_out(xprintf_out);
	clock_setup();

	gpio_set_usart(gpio_base, 'A', 9, 7);
	gpio_set_usart(gpio_base, 'A', 10, 7);
	uart_init(1, 1000000);

	systick_init();
	asm volatile ("cpsie i");
	asm volatile ("cpsie f");

	/* 地址线 */
	/* PF0~PF5 A0~A5 */
	gpio_set_fmc(gpio_base, 'F', 0);
	gpio_set_fmc(gpio_base, 'F', 1);
	gpio_set_fmc(gpio_base, 'F', 2);
	gpio_set_fmc(gpio_base, 'F', 3);
	gpio_set_fmc(gpio_base, 'F', 4);
	gpio_set_fmc(gpio_base, 'F', 5);
	/* PF12~15 A6~A9 */
	gpio_set_fmc(gpio_base, 'F', 12);
	gpio_set_fmc(gpio_base, 'F', 13);
	gpio_set_fmc(gpio_base, 'F', 14);
	gpio_set_fmc(gpio_base, 'F', 15);
	/* PG0~PG1 A10~A11 PG2~A12 */
	gpio_set_fmc(gpio_base, 'G', 0);
	gpio_set_fmc(gpio_base, 'G', 1);
	gpio_set_fmc(gpio_base, 'G', 2);
	/* 数据线 */
	/* PD14~15 PD0~1 AF2 */
	gpio_set_fmc(gpio_base, 'D', 14);
	gpio_set_fmc(gpio_base, 'D', 15);
	/* PD0~PD1 D2~D3 AF12 */
	gpio_set_fmc(gpio_base, 'D', 0);
	gpio_set_fmc(gpio_base, 'D', 1);
	/* PE7~15 D4~12 */
	gpio_set_fmc(gpio_base, 'E', 7);
	gpio_set_fmc(gpio_base, 'E', 8);
	gpio_set_fmc(gpio_base, 'E', 9);
	gpio_set_fmc(gpio_base, 'E', 10);
	gpio_set_fmc(gpio_base, 'E', 11);
	gpio_set_fmc(gpio_base, 'E', 12);
	gpio_set_fmc(gpio_base, 'E', 13);
	gpio_set_fmc(gpio_base, 'E', 14);
	gpio_set_fmc(gpio_base, 'E', 15);
	/* PD8~10 D13~15 AF12 */
	gpio_set_fmc(gpio_base, 'D', 8);
	gpio_set_fmc(gpio_base, 'D', 9);
	gpio_set_fmc(gpio_base, 'D', 10);

	/* 控制线 */
	/* PE0~1 NBL0~1 AF2 */
	gpio_set_fmc(gpio_base, 'E', 0);
	gpio_set_fmc(gpio_base, 'E', 1);
	/* PB5 SDCKE AF12 */
	gpio_set_fmc(gpio_base, 'B', 5);
	/* PB6 SDNE AF12 */
	gpio_set_fmc(gpio_base, 'B', 6);
	/* PC0 SDNWE AF12 */
	gpio_set_fmc(gpio_base, 'C', 0);
	/* PF11 SDNRAS */
	gpio_set_fmc(gpio_base, 'F', 11);
	/* PG4~PG5 BA0~BA1 */
	gpio_set_fmc(gpio_base, 'G', 4);
	gpio_set_fmc(gpio_base, 'G', 5);
	/* PG8 SDCLK */
	gpio_set_fmc(gpio_base, 'G', 8);
	/* PG15 SDNCAS */
	gpio_set_fmc(gpio_base, 'G', 15);

	xprintf("sdram init ...\r\n");
	sdram_init();

	#if 0
	xprintf("start kernel ...\r\n");
	xprintf("%s\r\n", "String");
	xprintf("%d\r\n", 1234);			//"1234"
    xprintf("%6d,%3d%%\r\n", -200, 5);	//"  -200,  5%"
    xprintf("%-6u\r\n", 100);			//"100   "
    xprintf("%ld\r\n", 12345678);		//"12345678"
    //xprintf("%llu\r\n", 0x100000000);	//"4294967296"	<XF_USE_LLI>
    //xprintf("%lld\r\n", -1LL);			//"-1"			<XF_USE_LLI>
    xprintf("%04x\r\n", 0xA3);			//"00a3"
    xprintf("%08lX\r\n", 0x123ABC);		//"00123ABC"
    xprintf("%016b\r\n", 0x550F);		//"0101010100001111"
    xprintf("%*d\r\n", 6, 100);			//"   100"
    xprintf("%s\r\n", "String");		//"String"
    xprintf("%5s\r\n", "abc");			//"  abc"
    xprintf("%-5s\r\n", "abc");			//"abc  "
    xprintf("%-5s\r\n", "abcdefg");		//"abcdefg"
    xprintf("%-5.5s\r\n", "abcdefg");	//"abcde"
    xprintf("%-.5s\r\n", "abcdefg");	//"abcde"
    xprintf("%-5.5s\r\n", "abc");		//"abc  "
    xprintf("%c\r\n", 'a');				//"a"
    //xprintf("%12f\r\n", 10.0);			//"   10.000000"	<XF_USE_FP>
    //xprintf("%.4E\r\n", 123.45678);		//"1.2346E+02"	<XF_USE_FP>
	#endif
	
	#if 0
	static uint32_t pre=0,cur=0;
	while(1){
		cur = s_ms;
		if(cur - pre >= 1000){
			pre = cur;
			xprintf("%d\r\n",cur);
		}

	}
	#endif

	#if 0
	uint32_t ts0;
	uint32_t ts1;
	volatile uint32_t* src=(uint32_t*)0x90000000;
	volatile uint32_t* dst=(uint32_t*)(0x90000000+SDRAM_SIZE/2);
	ts0 = get_ticks();
	for(uint32_t i=0;i<SDRAM_SIZE/8;i++)
	{
		*dst++ = *src++;
	}
	ts1 = get_ticks();
	xprintf("copy %dKB/S\r\n",(SDRAM_SIZE/2048)*1000ul/(ts1-ts0));
	#endif

	#if 0
	if(0 == memtester_main((ulv*)0x90000000, 0x01, SDRAM_SIZE, 1)){
		xprintf("sdram test ok!\r\n");
	}else{
		xprintf("sdram test err!\r\n");
	}
	#endif
	
	flash_itf_init();
	//spiflash_test();
	
    lcd_itf_init();
	//lcd_test();

	if(wait_key(3)){
		shell_set_itf(shell_read, shell_write, (shell_cmd_cfg*)g_shell_cmd_list_ast, 1);
		while(1){
			shell_exec();
		}
	}

	/* SPIFLASH boot */
	load_mem_itf_set(flash_itf_write,flash_itf_read);
	load_load_sections();

	lcd_itf_set_cb(logo_sync_dnoe);
	lcd_itf_swap();
    lcd_itf_sync_dma();
	while(s_logo_sync_done == 0);
    clock_delay(100);

	asm volatile ("cpsid i");
	systick_deinit();  /* 关闭中断等,避免内核启动时初始化定时器时马上产生中断导致异常 */

	load_boot();  /* 此时如果有有效dtb和kernel加载则直接启动 */

	/* @todo 其他boot */
	//load_mem_itf_set(xxx_write,xxx_read);
	//load_load_sections();
	//load_boot(); 

	/* 如果没有dtb和kernel加载则按照默认地址启动 */
	start_kernel(DTB_ADDR, KERNEL_ADDR);

	return 0;
}

static void noop(void)
{
	uint8_t c = 'E';
	uart_send(1, &c, 1);
	while (1) {
	}
}

extern unsigned int _end_text;
extern unsigned int _start_data;
extern unsigned int _end_data;
extern unsigned int _start_bss;
extern unsigned int _end_bss;
extern unsigned int _data_load_addr;
extern unsigned int _data_start;
extern unsigned int _data_end;

void reset(void)
{
	unsigned int *src, *dst;

	asm volatile ("cpsid i");

	src = &_data_load_addr;
	dst = &_data_start;
	while (dst < &_data_end) {
		*dst++ = *src++;
	}

	dst = &_start_bss;
	while (dst < &_end_bss) {
		*dst++ = 0;
	}

	user_main();
}


extern unsigned long _stack_top;

__attribute__((section(".vector_table")))
__attribute__((used)) static void (*vector_table[16 + 91])(void) = {
	(void (*))&_stack_top,
	reset,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	tick,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	uart1_irqhandler,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	dma2_stream4_irqhandler,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
};
