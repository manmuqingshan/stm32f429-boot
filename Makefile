CROSS_COMPILE ?= arm-none-eabi-

CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
SIZE = $(CROSS_COMPILE)size
GDB = $(CROSS_COMPILE)gdb
OPENOCD = openocd
KERNEL_ADDR?=0x0800C000
DTB_ADDR?=0x08004000

CFLAGS := -mthumb -mcpu=cortex-m4 -fno-builtin --specs=nano.specs
#CFLAGS += -save-temps
#--specs=nosys.specs
#--specs=nano.specs
CFLAGS += -ffunction-sections -fdata-sections  -nostdlib
CFLAGS += -Os -std=gnu99 -Wall -nostartfiles -g  -Imemtester_mcu/memtester/inc -ICMSIS/ -I./ 
LINKERFLAGS :=  --gc-sections
obj-y += dma.o ili9341v.o lcd_itf.o lcd_test.o load.o string.o stm32f429-boot.o xmodem.o shell.o shell_func.o uart.o fifo.o clock.o spi.o gpio.o start_kernel.o sdram.o xprintf.o spiflash.o spiflash_itf.o memtester_mcu/memtester/src/memtester.o memtester_mcu/memtester/src/memtester_tests.o 

all: stm32f429-boot

%.o: %.c
	$(CC) -c $(CFLAGS) -DKERNEL_ADDR=$(KERNEL_ADDR) -DDTB_ADDR=$(DTB_ADDR) $< -o $@

stm32f429-boot: stm32f429-boot.o $(obj-y)
	$(LD) -T stm32f429.lds $(LINKERFLAGS) -o stm32f429-boot.elf $(obj-y)
	$(OBJCOPY) -Obinary stm32f429-boot.elf stm32f429-boot.bin
	$(SIZE) stm32f429-boot.elf

clean:
	@rm -f *.o *.elf *.bin *.lst *.i *.s memtester_mcu/memtester/src/*.i memtester_mcu/memtester/src/*.o memtester_mcu/memtester/src/*.s

flash_stm32f429-boot: stm32f429-boot
	$(OPENOCD) -f board/stm32f429discovery.cfg \
	  -c "init" \
	  -c "reset init" \
	  -c "flash probe 0" \
	  -c "flash info 0" \
	  -c "flash write_image erase stm32f429-boot.bin 0x08000000" \
	  -c "reset run" \
	  -c "shutdown"

debug_stm32f429-boot: stm32f429-boot
	$(GDB) stm32f429-boot.elf -ex "target remote :3333" -ex "monitor reset halt"