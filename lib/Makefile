# ---------------------------------------------------------------------------
# ELISE bare-metal library - one Makefile for every target and every app
#
#   make                          # PC simulation of apps/cli.c  -> build/pc/cli.pc
#   make run APP=counters         # build and run an app on the PC
#   make TARGET=f100 APP=blink    # STM32VL-Discovery            -> build/f100/blink.bin
#   make TARGET=f100 APP=cli flash
#   make TARGET=f103 APP=cli flash                 # custom board, FTDI (as the TPs)
#   make TARGET=f100 APP=cli flash FLASHER=serial  # any board, UART bootloader
#   make test                     # unit tests on the PC
#   make all-apps TARGET=f100     # build every app
#   make apps                     # list the apps
#
# TARGET : pc | f100 (VL-Discovery) | f103 (custom TP board) | bluepill
# APP    : any apps/<name>.c
# FLASHER: ftdi   - FTDI drives RESET/BOOT0, then stm32flash (default f103)
#          stlink - st-flash over SWD                       (default f100, bluepill)
#          serial - stm32flash only, set BOOT0=1 and reset by hand
# The library is archived (libelise.a), so the linker only pulls the
# objects an app really uses: a blink stays tiny even with 60 modules.
# ---------------------------------------------------------------------------

TARGET      ?= pc
APP         ?= cli
OPENCM3_DIR ?= $(HOME)/umlp-elise/microcontrollers/libopencm3/libopencm3
SERIAL      ?= /dev/ttyUSB0
ifeq ($(TARGET),f103)
FLASHER     ?= ftdi
else
FLASHER     ?= stlink
endif
V           ?= 0

BUILD   := build/$(TARGET)
LIBNAME := $(BUILD)/libelise.a

# ------------------------------- sources ----------------------------------
LIB_DIRS := core hal drivers cli
INCLUDES := $(addprefix -I,$(LIB_DIRS))
LIB_SRCS := $(foreach d,$(LIB_DIRS),$(wildcard $(d)/*.c))

ifeq ($(TARGET),pc)
    PORT      := pc
    CC        := gcc
    AR        := ar
    BOARD_DEF := BOARD_PC
    ARCHFLAGS :=
    OPT       ?= -O2 -g
    LDFLAGS   :=
    LDLIBS    :=
    EXT       := pc
else
    PORT      := f1
    CC        := arm-none-eabi-gcc
    AR        := arm-none-eabi-ar
    OBJCOPY   := arm-none-eabi-objcopy
    SIZE      := arm-none-eabi-size
    ARCHFLAGS := -mthumb -mcpu=cortex-m3 -msoft-float
    OPT       ?= -Os -g
    EXT       := elf
    ifeq ($(TARGET),f100)
        BOARD_DEF := BOARD_VLDISCOVERY
        CHIP    := -DSTM32F10X_LD_VL
        LDSCRIPT:= ld/stm32f100rb.ld
    else ifeq ($(TARGET),f103)
        BOARD_DEF := BOARD_F103_CUSTOM
        CHIP    :=
        LDSCRIPT:= ld/stm32f103rb.ld
    else ifeq ($(TARGET),bluepill)
        BOARD_DEF := BOARD_BLUEPILL
        CHIP    := -DCLI_LITE
        LDSCRIPT:= ld/stm32f103c8.ld
    else
        $(error unknown TARGET '$(TARGET)': use pc, f100, f103 or bluepill)
    endif
    INCLUDES += -I$(OPENCM3_DIR)/include
    LDFLAGS  := $(ARCHFLAGS) -T$(LDSCRIPT) -L$(OPENCM3_DIR)/lib -nostartfiles \
                --specs=nano.specs -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(APP).map
    LDLIBS   := -lopencm3_stm32f1 -lc -lnosys
endif

INCLUDES += -Iport/$(PORT)
LIB_SRCS += $(wildcard port/$(PORT)/*.c)
LIB_OBJS := $(patsubst %.c,$(BUILD)/%.o,$(LIB_SRCS))

APP_SRC  := apps/$(APP).c
APP_OBJ  := $(BUILD)/apps/$(APP).o
APP_BIN  := $(BUILD)/$(APP).$(EXT)
APPS     := $(basename $(notdir $(wildcard apps/*.c)))

WARN     := -Wall -Wextra -Wshadow -Wpointer-arith -Wstrict-prototypes \
            -Wmissing-prototypes -Wconversion -Wno-sign-conversion
CFLAGS   := -std=c99 $(OPT) $(WARN) $(ARCHFLAGS) -ffunction-sections -fdata-sections \
            -D$(BOARD_DEF) -DSTM32F1 $(CHIP) $(INCLUDES) -MMD -MP

ifeq ($(V),0)
    Q := @
    E := @echo
else
    Q :=
    E := @true
endif

# -------------------------------- rules -----------------------------------
.PHONY: all lib run test flash flash-stlink size clean apps all-apps help

all: $(APP_BIN) $(if $(filter pc,$(TARGET)),,$(BUILD)/$(APP).bin size)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(E) "  CC    $<"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

# just the archive (used by the TP Makefile one level up: USE_LIB=1)
lib: $(LIBNAME)

$(LIBNAME): $(LIB_OBJS)
	$(E) "  AR    $@"
	$(Q)rm -f $@
	$(Q)$(AR) rcs $@ $^

$(APP_BIN): $(APP_OBJ) $(LIBNAME)
	$(E) "  LD    $@"
	$(Q)$(CC) $(LDFLAGS) -o $@ $(APP_OBJ) $(LIBNAME) $(LDLIBS)

$(BUILD)/$(APP).bin: $(APP_BIN)
	$(Q)$(OBJCOPY) -O binary $< $@

size: $(APP_BIN)
	$(Q)$(SIZE) $<

run: $(APP_BIN)
ifeq ($(TARGET),pc)
	./$(APP_BIN)
else
	$(error 'make run' is for TARGET=pc; use 'make flash' for the board)
endif

# Unit tests: always native gcc, sanitizers on
TEST_CFLAGS := -std=c99 -O1 -g -Wall -Wextra -fsanitize=address,undefined \
               -DBOARD_PC -Icore -Ihal -Idrivers -Icli -Iport/pc -Itests
test:
	@mkdir -p build/test
	$(Q)gcc $(TEST_CFLAGS) -o build/test/test_core tests/test_core.c $(wildcard core/*.c)
	./build/test/test_core
	$(Q)gcc $(TEST_CFLAGS) -o build/test/test_hal tests/test_hal.c \
	    $(wildcard core/*.c hal/*.c drivers/*.c cli/*.c port/pc/*.c)
	cd build/test && ./test_hal < /dev/null
	$(Q)./tests/test_cli.sh

flash: $(BUILD)/$(APP).bin
ifeq ($(FLASHER),ftdi)
	ftdi_cpu_prog -r 2 -b 1 -d $(SERIAL) -m 0
	sleep 2
	stm32flash -w $< -v $(SERIAL)
	ftdi_cpu_prog -r 2 -b 1 -d $(SERIAL) -m 1
else ifeq ($(FLASHER),serial)
	stm32flash -w $< -v -g 0x0 $(SERIAL)
else ifeq ($(FLASHER),stlink)
	st-flash --reset write $< 0x8000000
else
	$(error unknown FLASHER '$(FLASHER)': use ftdi, stlink or serial)
endif

flash-openocd: $(APP_BIN)
	openocd -f board/stm32vldiscovery.cfg -c "program $< verify reset exit"

apps:
	@echo $(APPS)

all-apps:
	@for a in $(APPS); do $(MAKE) --no-print-directory TARGET=$(TARGET) APP=$$a || exit 1; done

clean:
	rm -rf build

help:
	@sed -n '2,20p' Makefile

-include $(LIB_OBJS:.o=.d) $(APP_OBJ:.o=.d)
