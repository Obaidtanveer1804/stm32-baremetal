# ---------------------------------------------------------------------------
# Run the TP programs of the folder above (main_led.c, tp2.c, cgpio.c...)
# on the PC, unchanged, on top of the ELISE simulator.
#
#   make                       main_led.c + tp*.c + cgpio.c  -> ./led.pc
#   make run                   build and run (Ctrl-C to stop)
#   make run PROG=uart         main_uart.c
#   make run QUIET=1           without the "[sim] PC1 -> 1" lines
#   make run TP_SRCS="tp2.c cgpio.c"         choose the TP files yourself
#
# What replaces what:
#   uart_f1.c                  -> tp_uart_pc.c (same uart.h API)
#   <libopencm3/stm32/gpio.h>  -> include/  (same names, simulated ports)
# Your .c/.h files are only read, never modified.
# ---------------------------------------------------------------------------
PROG    ?= led
TP_DIR  ?= ../..
# default: every tpN.c of the TP folder (tp2.c, tp3.c...) and cgpio.c
TP_SRCS ?= $(notdir $(wildcard $(TP_DIR)/tp*.c $(TP_DIR)/cgpio.c))
QUIET   ?= 0
LIB_DIR := ..

# QUIET=1 and QUIET=0 objects kept apart, so switching needs no clean
BUILD   := build/quiet$(QUIET)
LIB_A   := $(LIB_DIR)/build/pc/libelise.a

CC      := gcc
# gnu17: the TP headers use old-style "void f();" prototypes (C23 would
# read them as "no arguments"), exactly like arm-none-eabi-gcc does.
TP_FLAGS  := -std=gnu17 -g -O1 -Wall -DSTM32F1 -Iinclude -I$(TP_DIR)
LIB_FLAGS := -DBOARD_PC $(addprefix -I$(LIB_DIR)/,core hal drivers cli port/pc)
ifeq ($(QUIET),1)
    LIB_FLAGS += -DTP_PC_QUIET
endif

TP_OBJS  := $(addprefix $(BUILD)/,$(patsubst %.c,%.o,$(notdir main_$(PROG).c $(TP_SRCS))))
PC_OBJS  := $(BUILD)/tp_uart_pc.o $(BUILD)/opencm3_gpio_pc.o
BIN      := $(PROG).pc

.PHONY: all run clean FORCE

all: $(BIN)

run: $(BIN)
	./$(BIN)

# always relinked (instant): QUIET=0/1 builds share the same program name
$(BIN): $(TP_OBJS) $(PC_OBJS) $(LIB_A) FORCE
	$(CC) -o $@ $(TP_OBJS) $(PC_OBJS) $(LIB_A)

# your TP files: compiled as they are, with the stand-in libopencm3 headers
$(BUILD)/%.o: $(TP_DIR)/%.c | $(BUILD)
	$(CC) $(TP_FLAGS) -c $< -o $@

# the PC versions of the hardware files
$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(TP_FLAGS) $(LIB_FLAGS) -Wextra -c $< -o $@

# the simulator + library (only recompiles what changed)
$(LIB_A): FORCE
	@$(MAKE) --no-print-directory -C $(LIB_DIR) TARGET=pc lib

$(BUILD):
	mkdir -p $@

FORCE:

clean:
	rm -rf build *.pc
