#include <stdlib.h>
#include <stdint.h>

void start_kernel(uint32_t dtbaddr, uint32_t kerneladdr)
{
	void (*kernel)(uint32_t reserved, uint32_t mach, uint32_t dt) = (void (*)(uint32_t, uint32_t, uint32_t))(kerneladdr | 1);

	kernel(0, ~0UL, dtbaddr);
}
