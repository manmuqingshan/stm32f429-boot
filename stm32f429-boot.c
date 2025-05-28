#include <stdlib.h>
#include <stdint.h>

#include "stm32f4_regs.h"
#include "usart.h"
#include "gpio.h"
#include "start_kernel.h"
#include "sdram.h"
#include "xprintf.h"
#include "memtester.h"

#if defined(USE_IS42S16320F)
	#define SDRAM_SIZE (64ul*1024ul*1024ul)
#else
	#define SDRAM_SIZE (8ul*1024ul*1024ul)
#endif

#define CONFIG_HSE_HZ	8000000
#define CONFIG_PLL_M	8
#define CONFIG_PLL_N	360
#define CONFIG_PLL_P	2
#define CONFIG_PLL_Q	7
/* 
 * VCO input frequency = PLL input clock frequency / PLLM with 2 ≤PLLM ≤63
 * 这里是CONFIG_HSE_HZ / CONFIG_PLL_M ,PPLM=8在范围内
 * 
 * VCO output frequency = VCO input frequency × PLLN  
 * 必须在100~432MHz之间
 * 即(CONFIG_HSE_HZ / CONFIG_PLL_M) * CONFIG_PLL_N = 360MHz在范围内
 * 
 * PLL output clock frequency = VCO frequency / PLLP with PLLP = 2, 4, 6, or 8
 * 这里PLLP=2 所以360M/2=180M 在范围<=180MHz范围内
 * 
*/
#define PLLCLK_HZ (((CONFIG_HSE_HZ / CONFIG_PLL_M) * CONFIG_PLL_N) / CONFIG_PLL_P)
#if PLLCLK_HZ == 180000000
#define FLASH_LATENCY	5
#else
#error PLL clock does not match 180 MHz
#endif

static volatile uint32_t s_ms=0;

static void *usart_base = (void *)USART1_BASE;
static void *gpio_base = (void *)GPIOA_BASE;

void tick(void)
{
	volatile uint32_t* ctrl = (volatile uint32_t*)SYSTICK_BASE;
	if(*ctrl & 0x10000){ /* 读清除标志bit16 */
		s_ms++;
	}
}

uint32_t get_ticks(void){
	return s_ms;
}


static void systick_init(void){
	volatile uint32_t* ctrl = (volatile uint32_t*)SYSTICK_BASE;
	volatile uint32_t* load = (volatile uint32_t*)(SYSTICK_BASE+0x04);
	volatile uint32_t* val = (volatile uint32_t*)(SYSTICK_BASE+0x08);
	//volatile uint32_t* calib = (volatile uint32_t*)(SYSTICK_BASE+0x0C);
	*load = PLLCLK_HZ/1000-1;  /* 24位 最大 16777216 */
	*val = 0;
	*ctrl = (1u<<2) | (1u<<1) | (1u<<0);
}


static void systick_deinit(void){
	volatile uint32_t* ctrl = (volatile uint32_t*)SYSTICK_BASE;
	*ctrl = 0;
}

static void clock_setup(void)
{
	volatile uint32_t *RCC_CR = (void *)(RCC_BASE + 0x00);
	volatile uint32_t *RCC_PLLCFGR = (void *)(RCC_BASE + 0x04);
	volatile uint32_t *RCC_CFGR = (void *)(RCC_BASE + 0x08);
	volatile uint32_t *FLASH_ACR = (void *)(FLASH_BASE + 0x00);
	volatile uint32_t *RCC_AHB1ENR = (void *)(RCC_BASE + 0x30);
	volatile uint32_t *RCC_AHB2ENR = (void *)(RCC_BASE + 0x34);
	volatile uint32_t *RCC_AHB3ENR = (void *)(RCC_BASE + 0x38);
	volatile uint32_t *RCC_APB1ENR = (void *)(RCC_BASE + 0x40);
	volatile uint32_t *RCC_APB2ENR = (void *)(RCC_BASE + 0x44);
	volatile uint32_t *RCC_AHB1LPENR= (void *)(RCC_BASE + 0x50);
	volatile uint32_t *PWR_CR= (void *)(0x40007000 + 0x00);
	volatile uint32_t *PWR_CSR= (void *)(0x40007000 + 0x04);

	uint32_t val;

	*RCC_CR |= RCC_CR_HSEON;
	while (!(*RCC_CR & RCC_CR_HSERDY)) {
	}

	val = *RCC_CFGR;
	val &= ~RCC_CFGR_HPRE_MASK;
	//val |= 0 << 4; // not divided
	val &= ~RCC_CFGR_PPRE1_MASK;
	val |= 0x5 << 10; // divided by 4
	val &= ~RCC_CFGR_PPRE2_MASK;
	val |= 0x4 << 13; // divided by 2
	*RCC_CFGR = val;

	/* __HAL_RCC_PWR_CLK_ENABLE 先使能PWR模块时钟,否则后续PWR操作都不会生效 */
	*RCC_APB1ENR |= (uint32_t)1u<<28;

	/* 
	 * VCAP1和VCAP2引脚要通过电容接GND
	 * VOS[1:0]选Scale 1/2 mode 默认是Scale 1 mode
	 * These bits can be modified only when the PLL is OFF.  
	 */
	*PWR_CR = (*PWR_CR & (~((uint32_t)3u<<14))) | ((uint32_t)3u<<14);

	val = 0;
	val |= RCC_PLLCFGR_PLLSRC_HSE;
	val |= CONFIG_PLL_M;
	val |= CONFIG_PLL_N << 6;
	val |= ((CONFIG_PLL_P >> 1) - 1) << 16;
	val |= CONFIG_PLL_Q << 24;
	*RCC_PLLCFGR = val;

	*RCC_CR |= RCC_CR_PLLON;
	while (!(*RCC_CR & RCC_CR_PLLRDY));
	/* 
	 * 跑180MHz,需要设置Over-drive 模式 见手册P124
	 * 当前使用HSI or HSE，默认就是HSI
	 * 此前已经配置PLLCFGR且PLLON
	 * VOS[1:0]选Scale 3 mode 默认是Scale 1 mode
	 * set PWR_CR.ODEN=1, wait PWR_CSR.ODRDY=1 
	 * set PWR_CR.ODSWEN=1, wait PWR_CSR.ODSWRDY=1
	 */
	*PWR_CR |= ((uint32_t)1u<<16);
	while((*PWR_CSR & ((uint32_t)1u<<16)) == 0);
	*PWR_CR |= ((uint32_t)1u<<17);
	while((*PWR_CSR & ((uint32_t)1u<<17)) == 0);

	/* 手册P81 电压2.7~3.6V最少设置LATENCY=5
	 */
	*FLASH_ACR = FLASH_ACR_ICEN | FLASH_ACR_PRFTEN | FLASH_LATENCY;

	*RCC_CFGR &= ~RCC_CFGR_SW_MASK;
	*RCC_CFGR |= RCC_CFGR_SW_PLL;
	while ((*RCC_CFGR & RCC_CFGR_SWS_MASK) != RCC_CFGR_SWS_PLL) {
	}

	/*  Enable all clocks, unused ones will be gated at end of kernel boot */
	*RCC_AHB1ENR |= 0x7ef417ff;
	*RCC_AHB2ENR |= 0xf1;
	*RCC_AHB3ENR |= 0x1;
	*RCC_APB1ENR |= 0xf6fec9ff;
	*RCC_APB2ENR |= 0x4777f33;

	/* Clear bit OTGHSULPILPEN in register AHB1LPENR when OTG HS in FS mode with internal PHY */
  	/* https://my.st.com/public/STe2ecommunities/mcu/Lists/cortex_mx_stm32/Flat.aspx?RootFolder=%2Fpublic%2FSTe2ecommunities%2Fmcu%2FLists%2Fcortex_mx_stm32%2FPower%20consumption%20without%20low%20power&FolderCTID=0x01200200770978C69A1141439FE559EB459D7580009C4E14902C3CDE46A77F0FFD06506F5B&currentviews=469 */
	*RCC_AHB1LPENR &= ~RCC_AHB1LPENR_OTGHSULPILPEN;
}

void xprintf_out(int ch){
	char c=(char)ch;
	usart_putch(usart_base, c);
}

int main(void)
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
	usart_setup(usart_base, PLLCLK_HZ/2,115200);

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

	if(0 == memtester_main((ulv*)0x90000000, 0x01, SDRAM_SIZE, 1)){
		xprintf("sdram test ok!\r\n");
	}else{
		xprintf("sdram test err!\r\n");
	}
	asm volatile ("cpsid i");
	systick_deinit();  /* 关闭中断等,避免内核启动时初始化定时器时马上产生中断导致异常 */
	start_kernel();

	return 0;
}

static void noop(void)
{
	usart_putch(usart_base, 'E');
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

	main();
}


extern unsigned long _stack_top;

__attribute__((section(".vector_table")))
void (*vector_table[16 + 91])(void) = {
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
