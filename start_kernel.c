#include <stdlib.h>
#include <stdint.h>
#include "clock.h"

void start_kernel(uint32_t dtbaddr, uint32_t kerneladdr)
{
    asm volatile ("cpsid i");
    systick_deinit();  /* 关闭中断等,避免内核启动时初始化定时器时马上产生中断导致异常 */

	void (*kernel)(uint32_t reserved, uint32_t mach, uint32_t dt) = (void (*)(uint32_t, uint32_t, uint32_t))(kerneladdr | 1);

	kernel(0, ~0UL, dtbaddr);
}
