# ELISE bare-metal library

A layered C99 library for the STM32F1 (VL-Discovery F100, F103 boards, Blue Pill)
built on **libopencm3**, where **every layer above the hardware also runs on a PC**.
It applies the TP rule (`uart_f1.c` vs `uart_pc.c`, one `main` linked against either)
to every peripheral, every driver and a full interactive shell.

```
make run                      # interactive shell, simulated board, in your terminal
make TARGET=f100 APP=cli flash
make test                     # ~80 000 unit checks + 48 end-to-end shell tests
```

## Layers

```
 apps/        main() programs: cli, counters, gps_nav, thermostat, traffic_light ...
   |
 cli/         shell engine (line editor, history, TAB) + command groups
 drivers/     external chips: EEPROM, SPI flash, OLED, LCD, 1-Wire, encoder, stepper ...
   |
 hal/         hal_*.h interfaces (GPIO, time, UART, timer, PWM, ADC, DAC, SPI, I2C,
   |          EXTI, IRQ, watchdog, flash, RTC, sys) + board.h + portable helpers
   |
 port/f1/     libopencm3 implementation  <-- choose one at link time -->  port/pc/
              (real registers)                                       (simulation)
 core/        pure C, no hardware at all: usable from every layer
```

Rules that keep it portable:

* Only `port/` includes libopencm3 or touches registers.
* `core/` includes nothing but itself and `<stdint.h>`-level headers. It prints
  through `io.h`, which is wired to a UART (or stdout, or a test buffer) by the app.
* Drivers talk to the hardware **only** through `hal_*()` calls, so the same
  EEPROM driver runs against the real I2C peripheral and against the simulated chip.
* Pins are named by the board (`BOARD_LED_GREEN`), never hard-coded in a driver.
* No `malloc`, no floating point, no libc `printf`: fixed-point math (`fixmath.h`),
  a 2 kB printf engine (`fmt.h`), static buffers.

## Targets

| `TARGET`   | board                    | chip         | clock  | define               |
|------------|--------------------------|--------------|--------|----------------------|
| `pc`       | simulation (default)     | -            | 24 MHz | `BOARD_PC`           |
| `f100`     | STM32VL-Discovery        | STM32F100RB  | 24 MHz | `BOARD_VLDISCOVERY`  |
| `f103`     | custom TP board (FTDI)   | STM32F103RB  | 72 MHz | `BOARD_F103_CUSTOM`  |
| `bluepill` | Blue Pill                | STM32F103C8  | 72 MHz | `BOARD_BLUEPILL`     |

```
make TARGET=f100 APP=blink                        # build/f100/blink.elf + .bin + size
make TARGET=f100 APP=cli flash                    # st-flash (f103: ftdi_cpu_prog + stm32flash)
make TARGET=f103 APP=cli flash FLASHER=serial     # FLASHER = ftdi | stlink | serial
make TARGET=f100 APP=cli OPENCM3_DIR=/path/to/libopencm3
make all-apps TARGET=bluepill                     # build every app
make apps                                         # list apps
```

### From the TP folder (one level up)

The TP `Makefile` drives the library too, with the same `make flash` as the TPs.
`BOARD` is `f103` (default, FTDI flashing) or `f100` (VL-Discovery, st-flash):

```
make flash                                 # main_led.c, unchanged behaviour
make BOARD=f100 PROG=uart flash            # a TP on the VL-Discovery
make PROG=hello USE_LIB=1 flash            # your main_hello.c using hal_* / io_printf
make lib-flash APP=cli                     # a ready-made app from lib/apps
make lib-flash APP=cli BOARD=f100
make lib-pc APP=cli                        # same app on the PC
```

With `USE_LIB=1` the TP files (`uart_f1.c`, `tp2.c`...) are still linked, so `my_puts()`
and the library can be mixed in one program.

### The TPs themselves on the PC (`tp_pc/`)

`main_led.c`, `tp2.c`, `cgpio.c`... compile unchanged against a PC version of
`uart_f1.c` (`tp_uart_pc.c`, same `uart.h` API) and stand-in libopencm3 GPIO headers
(`tp_pc/include/`) that drive the simulated ports:

```
cd lib/tp_pc
make run                     # main_led.c + every tpN.c + cgpio.c
make run PROG=uart           # main_uart.c
make run QUIET=1             # without the "[sim] PC1 -> 1" lines
make run TP_SRCS="tp2.c cgpio.c"   # pick the TP files yourself
```

`OPENCM3_DIR` defaults to the path used in the course Makefile. The library is archived
(`libelise.a`) and built with `-ffunction-sections -Wl,--gc-sections`, so an app only pays
for what it calls: `blink` is 2.4 kB, the full shell 61 kB.

Flash/RAM of the apps on the F100 (`-Os`):

| app              | flash  | RAM    | shows |
|------------------|--------|--------|-------|
| blink            | 2.5 kB | 0.2 kB | GPIO + SysTick delays |
| counters         | 6.1 kB | 0.6 kB | TP2 counters on `counter.h` |
| traffic_light    | 7.5 kB | 1.3 kB | table FSM + EXTI + event queue + soft timers |
| reaction         | 8.3 kB | 0.8 kB | EXTI timestamps, random, statistics |
| thermostat       | 8.4 kB | 0.6 kB | LM75 + fixed-point PID + PWM, ASCII chart |
| modbus_node      | 9.4 kB | 1.0 kB | Modbus RTU slave on RS-485 |
| lorenz           | 9.6 kB | 1.6 kB | Q16.16 Lorenz attractor on the OLED, fixed vs float cycles |
| datalogger       | 11 kB  | 0.6 kB | periodic sampling to SPI flash, CRC records, CSV dump |
| menu_demo        | 12 kB  | 1.7 kB | OLED menu, rotary encoder, settings in flash |
| gps_nav          | 15 kB  | 1.9 kB | NMEA GPS + geo maths + OLED navigation |
| scpi_instrument  | 15 kB  | 1.4 kB | SCPI voltmeter / frequency meter / generator for PyVISA |
| cli              | 65 kB  | 5.6 kB | everything, interactive (`CLI_LITE` on the Blue Pill: 55 kB) |

## The PC simulation

`TARGET=pc` links `port/pc/`, a behavioural model of the board:

* a 1 ms `SIGALRM` plays SysTick; timer, EXTI and UART "interrupts" run inside the
  signal handler, and `hal_irq_save()` blocks the signal exactly like PRIMASK does;
* the terminal is the console UART (raw mode, Enter sends `\r` like picocom);
* GPIO has the F1 register model: `peek 0x4001100C` reads GPIOC_ODR,
  `poke 0x40011010 0x100` sets PC8 through BSRR, CRL/CRH decode the pin modes;
* ADC channels take constants or waveforms (sine, triangle, square, ramp, noise),
  the internal temperature sensor and VREFINT follow the datasheet, DAC1/2 are
  wired to ADC channels 4/5;
* chip models sit on the buses: **24C02** EEPROM (pages, 5 ms write cycle with NACK),
  **LM75**, **DS1307**, **SSD1306** OLED (viewable with `sim oled`) on I2C1, and a
  **W25Q16** SPI flash (JEDEC ID, page program, sector erase, busy flag) on SPI1/PA4;
* internal flash is saved in `flash_sim.bin`, so settings survive a restart;
* `reset` and a watchdog timeout re-execute the program and report the reset cause;
* when stdin is a pipe, the program powers off at the end of the script, which is
  how `tests/test_cli.sh` drives the shell.

Drive it from the shell with `sim ...`: `sim pin PA0 1`, `sim press PA0`,
`sim wave 1 sine 2048 1000 500`, `sim temp 31.5`, `sim log gpio on`, `sim status`,
`sim oled`, `sim uart 2 loop on`.

## The shell (`apps/cli.c`)

```
elise> help
elise> gpio set PC8 ; gpio port C
elise> calc (1<<8)|(1<<9)            768  0x00000300
elise> i2c scan 1                    i2cdetect-style map
elise> eeprom write 0x10 "hello" ; eeprom read 0 32
elise> adc stats 1 256               min/max/mean/sd + histogram of the noise
elise> pwm init t3c3 1000 ; pwm duty t3c3 12.5
elise> reg TIM3                      peripheral registers by name
elise> watch 500 adc read 1          q to stop
elise> top                           scheduler tasks, run counts, worst-case us
elise> adc fft 1 8000 7              spectrum of ADC channel 1 (Hann + Q15 FFT)
elise> adc freq 1                    frequency, Vpp, Vrms, Vdc
elise> bench                         cycles of crc32 / fft / sin / printf (DWT CYCCNT)
elise> oled demo ; sim oled          draw on the SSD1306, view it in the terminal
```

Line editing (arrows, Home/End, Ctrl-A/E/U/K/W/L), history (up/down, `!!`), TAB
completion of commands and sub-commands, unique-prefix abbreviations (`gp tog PC9`),
`;` to chain, quotes for spaces. Commands come in groups (`cli_cmds.h`): an app
registers only the groups it needs.

Command groups: `system` (info uptime reset log top task wdg time bench), `gpio` (gpio led
button), `analog` (adc incl. fft/freq/scope/stats, dac with waveforms), `timers` (timer pwm servo tone), `memory` (peek poke dump
reg bit field), `buses` (i2c spi uart), `clock` (date alarm stopwatch), `devices`
(eeprom flash temp rtcx config ow play oled nmea geo), `tools` (calc let conv bits crc
morse seg7 rand b64), `counters` (count), `simulator` (sim, PC only).

## Module catalogue

### core/ (portable, no hardware)

| module | what |
|---|---|
| `bits.h` | BIT/FIELD macros, popcount, parity, clz/ctz, reverse, rotate, bswap, sign-extend, Morton |
| `ringbuf` | lock-free SPSC byte FIFO, power-of-two masking (ISR to main loop) |
| `fmt` | printf engine (`%d %u %x %b %s %c %p`, width, precision, flags, `l`/`ll`), snprintf, fixed-point printing |
| `io` | console abstraction (`io_printf`, `io_getc`) over function pointers, ANSI helpers |
| `strutil` | bounded string functions, tokenizer with quotes, number parsing (hex/bin/oct, fixed point, bool) |
| `counter` | wrap/saturate/bounce counters, TP2 displays (dec, bin, hex, BCD, Gray), 16 to 32-bit extension |
| `codes` | BCD, Gray, 7-segment, Morse, Manchester, Hamming(7,4), ZigZag, Base64 |
| `crc` | CRC8 Maxim/SMBus, CRC16 CCITT/Modbus/XMODEM, CRC32 (nibble tables), Fletcher, Adler, Internet, NMEA |
| `fixmath` | Q16.16 / Q1.15, CORDIC sin/cos/atan2, table sine, isqrt, hypot, saturating ops, map, gcd, div10 |
| `filter` | moving average, EMA (shift), median, biquad IIR, FIR, hysteresis, slew limiter, peak hold, oversampling |
| `pid` | fixed-point PID, derivative on measurement, anti-windup |
| `debounce` | 32 inputs at once with vertical counters, click/double/long/repeat detection |
| `softtimer` | many one-shot/periodic timers on one ms tick, wrap-safe |
| `sched` | cooperative scheduler with run counts and worst-case execution time |
| `fsm` | table-driven state machine with enter/exit hooks and trace |
| `evq` | ISR-to-main event queue |
| `rand` | xorshift32, unbiased ranges, shuffle, LFSR16 PRBS |
| `framing` | COBS and SLIP packet framing for UART links |
| `hexdump`, `stats`, `log` | memory dump, running stats + histogram, levelled colour logging + ASSERT |
| `datetime` | Unix time <-> calendar, parsing, weekday, durations |
| `expr` | C integer expression evaluator (used by `calc`) |
| `nmea`, `geo` | GPS sentence parser/builder, distance, bearing, cross-track in integer maths |
| `gfx`, `font5x7` | monochrome graphics: lines, circles, text, bitmaps, arrows, terminal preview |
| `list`, `pool`, `bitset`, `sort` | intrusive lists, block allocator, bit arrays, heapsort/quickselect/bsearch |
| `modbus` | Modbus RTU slave (functions 1-6, 15, 16) + request builder |
| `dds` | phase-accumulator waveform synthesis |
| `trace` | microsecond event tracing from ISRs |
| `menu` | hierarchical menus for LCD/OLED (values, toggles, actions) |
| `dsp` | Q15 radix-2 FFT, Hann window, Goertzel, RMS, zero-crossing frequency |
| `scpi` | IEEE 488.2 / SCPI parser: short/long forms, suffixes, error queue, units |

### hal/ + port/

`hal_gpio` (single pin and BSRR port-wide writes), `hal_time` (SysTick ms/us, delays,
tick hook), `hal_uart` (interrupt RX ring, 3 UARTs), `hal_timer` (periodic interrupts,
exact PSC/ARR search), `hal_pwm` (8 channels on TIM3/TIM4, per-mille duty, servo us),
`hal_adc` (mV, VDDA via VREFINT, die temperature), `hal_dac`, `hal_spi` (4 modes,
prescaler from a max frequency), `hal_i2c` (register level with timeouts on every wait,
the RM0008 N=1/2/>2 read sequences, probe, register helpers), `hal_exti` (per-line
callbacks), `hal_irq` (nesting-safe critical sections), `hal_wdg`, `hal_flash` (user
pages at the end of flash), `hal_rtc` (seconds counter + alarm), `hal_sys` (clock,
reset cause, unique ID, stack painting, DWT cycle counter, safe peek/poke).

### drivers/

`led` (blink/flash/heartbeat patterns), `button`, `soft_i2c` (bit-banged, clock
stretching, bus recovery), `eeprom24` (24C01..24C512, page splitting, ACK polling),
`w25q` (SPI NOR flash), `sensors_i2c` (LM75, DS1307), `hd44780` (4-bit LCD, bar graph),
`ssd1306` (OLED), `shiftreg` (74HC595 / 74HC165 chains), `max7219` (7-seg / matrix),
`onewire` (ROM search + DS18B20), `encoder` (quadrature state table), `stepper`
(28BYJ-48 wave/full/half + A4988, trapezoidal ramp), `melody` (buzzer tunes),
`config` (wear-levelled, power-safe key/value store in flash), `keypad` (matrix scan),
`pulse` (frequency/duty via EXTI timestamps, HC-SR04), `adc_capture` (timer-paced
sampling + Vpp/Vrms/frequency analysis).

### tools/

`scpi_client.py`: drives `scpi_instrument` through pyserial, PyVISA, or the PC
simulation (`--sim`), including a DAC-to-ADC transfer-curve sweep saved to CSV.

## Tests

```
make test
```

* `tests/test_core.c`: every core module, with ASan + UBSan (~77 800 checks:
  bit tricks, printf formats, CRC check values, CORDIC accuracy, PID closed loop,
  Hamming single-bit correction, COBS round trips, calendar round trips...).
* `tests/test_hal.c`: HAL helpers, the PC port, and the drivers against the chip
  models (EEPROM page wrap, SPI flash program/erase rules, config compaction...).
* `tests/test_cli.sh`: pipes 48 command lines into the shell and checks the answers.

## Status

* Everything compiles without warnings for the PC and the three STM32F1 boards, and
  all apps run in the PC simulation; the test suites pass under ASan/UBSan.
* The `port/f1` code follows RM0008 and the libopencm3 API but **has not been run on
  a board yet**. Check these first on hardware, in this order: `blink`, `cli` + `info`,
  `gpio`, `uart`, `timer`/`pwm`, `adc`, then I2C (`i2c scan`), SPI, flash (`config`),
  RTC and watchdog. The I2C read sequences and the RTC/backup-domain start-up are
  the parts most likely to need adjusting.
* See [docs/LEARNING_PATH.md](docs/LEARNING_PATH.md) for a guided tour with exercises.

## Adding things

**A command**: write `static int cmd_x(int argc, char *argv[])`, add a row to a
`cli_command_t` table, register the group. Return `CLI_ERR_USAGE` to print the usage.

**A driver**: only include `hal_*.h`; take the bus/pins as parameters; add a model to
`port/pc/sim_devices_pc.c` if you want to test it without hardware.

**A peripheral**: declare the API in `hal/hal_x.h`, implement it in `port/f1/x_f1.c`
with libopencm3 and in `port/pc/x_pc.c` as a model. Put arithmetic that both ports
need (prescalers, conversions) in `hal_common.c` so it is tested once.

**A chip family** (e.g. F4): add `port/f4/` with the same function names and a new
`TARGET` block in the Makefile; nothing above `port/` changes.

## Conventions

C99, 4 spaces, `module_verb_object()` names, `s_` for file statics, `UPPER_CASE`
macros with a module prefix, `U` suffix on unsigned constants, a `default` in every `switch`,
every `if` with braces, fixed-width types (`uint32_t`), `const`
tables in flash, no magic numbers (named `#define`s), Doxygen-style comments in the
headers explaining *why*. Builds with `-Wall -Wextra -Wshadow -Wconversion
-Wstrict-prototypes -Wmissing-prototypes` and no warnings on both GCC targets.
