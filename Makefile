# Select program: make PROG=led | make PROG=uart
# Select board  : make BOARD=f103 (custom board, default) | f100 (VL-Discovery) | pc
#   make BOARD=f100 QEMU=1 qemu   -> VL-Discovery image for qemu (no PLL setup)
#   make BOARD=pc run             -> runs on the PC (lib/tp_pc simulator)
# Switching board or QEMU: run "make clean" first (same .o names).
PROG ?= led
BOARD ?= f103
PROJ_NAME = $(PROG)
SERIAL ?= /dev/ttyUSB0
CC=arm-none-eabi-gcc
SRCS=uart_f1.o main_$(PROG).o tp2.o tp3.o cgpio.o newtonsq.o fact.o spi_f1.o syscalls_template.o
REPERTOIRE ?= $(HOME)/umlp-elise/microcontrollers/libopencm3/libopencm3
# PATH:=/home/jmfriedt/enseignement/sqnx/platforms/stm32/qemu/qemu_stm32/arm-softmmu/:$(PATH)

ifeq ($(BOARD),f103)
 BOARD_DEF = -DBOARD_F103_CUSTOM
 LDSCRIPT  = lib/ld/stm32f103rb.ld   # 128K flash (last 4K reserved), 20K RAM
else ifeq ($(BOARD),f100)
 BOARD_DEF = -DBOARD_VLDISCOVERY -DSTM32F10X_LD_VL
 LDSCRIPT  = lib/ld/stm32f100rb.ld   # 128K flash (last 4K reserved), 8K RAM
else ifeq ($(BOARD),pc)
 BOARD_DEF = -DBOARD_PC
else
 $(error unknown BOARD '$(BOARD)': use f103, f100 or pc)
endif
ifeq ($(QEMU),1)
 BOARD_DEF += -DQEMU
endif

COMMONS = -mthumb -mcpu=cortex-m3
DRAPEAU =-g -Wall
DRAPEAU +=$(COMMONS) -DSTM32F1 $(BOARD_DEF)
DRAPEAU +=-I$(REPERTOIRE)/include
LDFLAGS = $(COMMONS) -g -L$(REPERTOIRE)/lib -lm --static -T$(LDSCRIPT) -nostartfiles
LDFLAGS +=-lopencm3_stm32f1 -lc -lnosys

all:$(PROJ_NAME).bin

# Make does not see BOARD/QEMU changes: a stamp per configuration forces a rebuild
BOARD_STAMP = .board.$(BOARD)$(if $(filter 1,$(QEMU)),.qemu)
$(BOARD_STAMP):
	rm -f .board.* *.o *.elf *.bin
	touch $@

$(SRCS) $(PROJ_NAME).elf: tpboard.h $(BOARD_STAMP)

# PC build: the TP files run unchanged on the simulator of lib/tp_pc
pc run:
	$(MAKE) -C lib/tp_pc PROG=$(PROG) run

$(PROJ_NAME).bin: $(PROJ_NAME).elf
	arm-none-eabi-objcopy -Obinary $(PROJ_NAME).elf $(PROJ_NAME).bin

$(PROJ_NAME).elf: $(SRCS)
	$(CC) -o $(PROJ_NAME).elf $(SRCS) $(LDFLAGS)

main_led.o: main_led.c
	$(CC) $(DRAPEAU) -c main_led.c

uart_f1.o: uart_f1.c
	$(CC) $(DRAPEAU) -c uart_f1.c

fact.o: fact.c fact.h
	$(CC) $(DRAPEAU) -c fact.c

newtonsq.o: newtonsq.c
	$(CC) $(DRAPEAU) -c newtonsq.c

spi_f1.o: spi_f1.c
	$(CC) $(DRAPEAU) -c spi_f1.c

main_uart.o: main_uart.c
	$(CC) $(DRAPEAU) -c main_uart.c

tp2.o: tp2.c tp2.h
	$(CC) $(DRAPEAU) -c tp2.c

tp3.o: tp3.c tp3.h
	$(CC) $(DRAPEAU) -c tp3.c

cgpio.o: cgpio.c cgpio.h
	$(CC) $(DRAPEAU) -c cgpio.c

main_spi.o: main_spi.c
	$(CC) $(DRAPEAU) -c main_spi.c

main_racine.o: main_racine.c
	$(CC) $(DRAPEAU) -c main_racine.c

racine.o: racine/racine.c
	$(CC) $(DRAPEAU) -c racine/racine.c -o racine.o

fixed.o: racine/fixed.c
	$(CC) $(DRAPEAU) -c racine/fixed.c -o fixed.o

syscalls_template.o: syscalls_template.c
	$(CC) $(DRAPEAU) -c syscalls_template.c

clean:
	rm -f *.o $(PROJ_NAME).elf $(PROJ_NAME).bin .board.*

flash: $(PROJ_NAME).bin
ifeq ($(BOARD),f100)
	st-flash $(if $(STLINK),--serial $(STLINK)) --reset write $(PROJ_NAME).bin 0x8000000   # VL-Discovery: on-board ST-Link (STLINK=<serial> if two are plugged)
else
	ftdi_cpu_prog -r 2 -b 1 -d $(SERIAL) -m 0
	sleep 2
	stm32flash -w $(PROJ_NAME).bin -v $(SERIAL)
	ftdi_cpu_prog -r 2 -b 1 -d $(SERIAL) -m 1
endif

monqemu: $(PROJ_NAME).bin
	which qemu-system-arm
	qemu-system-arm -M stm32-p103 -serial stdio -serial stdio -kernel $(PROJ_NAME).bin -nographic

qemu-gdb: $(PROJ_NAME).bin
	qemu-system-arm -M stm32vldiscovery -s -S -serial stdio -kernel $(PROJ_NAME).bin
#	qemu-system-arm -M stm32-p103 -s -serial stdio -kernel $(PROJ_NAME).bin

qemu: $(PROJ_NAME).bin
	qemu-system-arm -M stm32vldiscovery -serial stdio -kernel $(PROJ_NAME).bin
