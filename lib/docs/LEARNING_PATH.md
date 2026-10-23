# Learning path through the library

Each step names the files to read, the idea they demonstrate, and exercises you can
check on the PC (`make run APP=...` or the `cli` app) before trying them on the board.
Read the header first (it explains *why*), then the `.c`.

---

## Stage 1: C for microcontrollers (weeks 1-4)

### 1.1 Bits and registers: `core/bits.h`
* `BIT(n)`, `FIELD_SET(reg, pos, width, v)`: every peripheral register is a set of
  bit fields. `FIELD_SET` is a read-modify-write: clear the field with `~mask`,
  OR in the new value.
* Tricks worth knowing by heart: `x & (x - 1)` clears the lowest set bit,
  `x & -x` keeps it, `(x & (x - 1)) == 0` tests a power of two, `x & (n - 1)` is
  `x % n` when n is a power of two.
* **Exercises**: in the shell, `bits 0x300`, `conv 0xA5`, `field 0x40011004 0 4`
  (mode of PC8 in GPIOC_CRH). Write `bits_count_trailing_ones()` and test it in
  `tests/test_core.c`.

### 1.2 Fixed-width types and printing without printf: `core/fmt.c`, `core/io.c`
* A printf engine is a state machine over the format string. Numbers are produced
  backwards (`n % base`, `n / base`) then reversed.
* `io.h` decouples *what* to print from *where*: the same `counter_print_hex()` writes
  to a UART on the board and to stdout on the PC. This is the TP2 rule, generalised.
* **Exercises**: add `%e`-like scientific output for fixed-point values; make
  `io_printf` count characters and check it against `fmt_snprintf`.

### 1.3 Strings and parsing: `core/strutil.c`, `core/expr.c`
* Every function that writes takes the buffer size: no overflow is possible.
* `str_to_u32` detects overflow *before* multiplying: `value > (MAX - d) / base`.
* `expr.c` is a recursive-descent parser: one function per precedence level.
* **Exercises**: `calc (1<<8)|(1<<9)`, `calc 24000000/(23+1)/(999+1)`. Add the `**`
  power operator with the right precedence.

### 1.4 Number codes: `core/codes.c`, `core/counter.c`
* BCD, Gray (`b ^ (b >> 1)`), 7-segment tables, Hamming(7,4) single-error correction.
* **Exercises**: `count 0 15 1 300 gray` and check that one bit changes per step;
  `seg7 HELLO`; flip a bit of a Hamming code word and decode it.

---

## Stage 2: the first peripherals (weeks 5-10)

### 2.1 GPIO: `hal/hal_gpio.h`, `port/f1/gpio_f1.c`
* F1 pins are configured by 4-bit fields in CRL/CRH (MODE + CNF). Pull-up vs
  pull-down is chosen by the ODR bit, a classic F1 surprise.
* BSRR sets and resets bits in one store: no read-modify-write, safe with interrupts.
  `hal_gpio_port_toggle()` builds the BSRR word from ODR to toggle in one write.
* **Exercises**: `gpio mode PC8 out ; gpio set PC8 ; peek GPIOC+0x0C` then
  `poke GPIOC+0x10 0x01000000` (reset PC8 through BSRR). `sim leds on` to watch.

### 2.2 Time: `port/f1/time_f1.c`
* SysTick reloads every 1 ms; `hal_time_us()` interpolates inside the millisecond
  with the counter value, re-reading if the tick changed meanwhile.
* All comparisons are `(now - start) >= duration`: correct across the 32-bit wrap.
* **Exercise**: explain why `now >= start + duration` fails after 49.7 days.

### 2.3 UART with interrupts: `port/f1/uart_f1.c`, `core/ringbuf.c`
* The RXNE interrupt pushes into a ring buffer, the main loop pops. One writer per
  index, so no locking is needed. Indices run freely and are masked.
* **Exercises**: `uart init 2 9600 ; sim uart 2 loop on ; uart send 2 hi ; uart recv 2`.
  Make TX interrupt driven too (TXE interrupt + a second ring).

### 2.4 Timers: `hal/hal_common.c` (`hal_timer_calc`), `port/f1/timer_f1.c`
* f = f_clk / ((PSC + 1)(ARR + 1)). The search prefers an exact split
  (72 MHz / 1 Hz = 1125 x 64000) before rounding.
* Timer clock is x2 when the APB prescaler is not 1 (RM0008 7.2).
* **Exercises**: `timer calc 50`, `timer start 2 1000 PB12`, `reg TIM2`,
  `poke TIM2+0x28 1` and see the frequency halve.

### 2.5 PWM: `port/f1/pwm_f1.c`
* PWM mode 1: output high while CNT < CCR. Duty = CCR / (ARR + 1).
* On the VL-Discovery the LEDs PC8/PC9 are TIM3 CH3/CH4 after a full remap.
* **Exercises**: `pwm init t3c3 1000 ; pwm fade t3c3`, `servo t3c1 90`, `play birthday`.

### 2.6 ADC: `port/f1/adc_f1.c`
* ADC clock <= 14 MHz, longest sample time for high-impedance sources and the
  temperature sensor. VDDA is measured with VREFINT, so readings stay correct when
  the supply is not exactly 3.3 V.
* **Exercises**: `adc scan`, `adc stats 1 1000` with `sim noise 8`, then use
  `filt_oversample()` to gain 2 bits of resolution and compare the standard deviation.

---

## Stage 3: interrupts and program structure (weeks 11-16)

### 3.1 EXTI and critical sections: `port/f1/exti_f1.c`, `hal/hal_irq.h`
* 16 lines shared by all ports: PA0 and PB0 cannot both interrupt.
* `hal_irq_save()` returns the previous PRIMASK so critical sections can nest.
* **Exercise**: `apps/reaction.c`: why is the timestamp taken *in* the ISR?

### 3.2 Debouncing: `core/debounce.c`
* Vertical counters debounce 32 inputs with 8 logic operations.
* **Exercise**: draw the cnt0/cnt1 truth table and check 4 samples are needed.

### 3.3 Event-driven design: `core/fsm.c`, `core/evq.c`, `core/softtimer.c`
* `apps/traffic_light.c` has no delay at all: ISRs post events, the main loop
  dispatches them to a table-driven state machine, then sleeps (WFI).
* **Exercise**: add a "night mode" state (amber blinking) entered by a long press.

### 3.4 Cooperative scheduling: `core/sched.c`, `apps/cli.c`
* Run-to-completion tasks with periods; `top` shows each task's worst-case time.
* **Exercise**: add a task that blocks for 50 ms and watch `top` and the shell lag.

---

## Stage 4: buses and external chips (semester 2)

### 4.1 SPI: `port/f1/spi_f1.c`, `drivers/w25q.c`, `drivers/max7219.c`
* Full duplex: every byte sent clocks one in. Chip select is a GPIO.
* NOR flash: erase sets 1s by 4 KiB sectors, program clears bits by 256-byte pages,
  write-enable before each operation, poll BUSY after.
* **Exercises**: `flash id`, `flash erase 0`, write twice to the same address and
  explain the result (bits only go 1 -> 0).

### 4.2 I2C: `port/f1/i2c_f1.c`, `drivers/soft_i2c.c`, `drivers/eeprom24.c`
* The F1 I2C needs exact sequences for reading 1, 2 or N bytes (RM0008 26.3.3);
  every wait has a timeout so a missing chip cannot hang the program.
* `soft_i2c.c` shows the protocol bit by bit: START, 8 bits, ACK, STOP.
* EEPROM page writes wrap inside the page; the chip NACKs while writing (ACK polling).
* **Exercises**: `i2c scan 1`, `eeprom test`, `i2c reg 1 0x48 0 2` (LM75 raw).

### 4.3 1-Wire: `drivers/onewire.c`
* Time slots of a few us, interrupts masked per slot. ROM search walks a binary tree.

### 4.4 Displays: `drivers/hd44780.c`, `drivers/ssd1306.c`, `core/gfx.c`
* HD44780 4-bit init: "0x3 three times then 0x2" works from any state.
* Page-organised framebuffer: byte = 8 vertical pixels. Bresenham lines, midpoint circles.
* **Exercises**: `oled demo ; sim oled`, `apps/lorenz.c`, `apps/menu_demo.c`.

---

## Stage 5: numbers without an FPU (with the fixed-point TP)

### 5.1 Fixed point: `core/fixmath.c`
* Q16.16: multiply in 64 bits then shift back; divide by shifting the numerator first.
* CORDIC computes sin/cos/atan2 with shifts and adds only; a quarter-wave table with
  interpolation is faster for sine.
* **Exercises**: `bench` on the board: compare `sin CORDIC` and `sin table`;
  `apps/lorenz.c`: fixed vs soft-float cycles per step.

### 5.2 Filters and control: `core/filter.c`, `core/pid.c`
* EMA with a shift: `y += (x - y) >> k` is a first-order low-pass with no multiply.
* PID: derivative on measurement, integrator clamping (anti-windup).
* **Exercise**: tune the gains in `apps/thermostat.c` and plot the overshoot.

### 5.3 Signal processing: `core/dsp.c`, `drivers/adc_capture.c`
* Timer-paced sampling, Hann window, Q15 FFT with 1/2 scaling per stage.
* **Exercises**: `sim wave 1 square 2048 800 4 ; adc fft 1 8000 8`: find the odd
  harmonics at 3f, 5f, 7f with amplitudes 1/3, 1/5, 1/7.

---

## Stage 6: systems (beyond the course)

* Protocols: `core/framing.c` (COBS/SLIP), `core/modbus.c`, `core/scpi.c` with
  `tools/scpi_client.py` (PyVISA, like the instrumentation TPs).
* Persistent data: `drivers/config.c` (log-structured, wear-levelled, power-safe).
* Memory: `core/pool.c`, `core/list.h`, `cli_scratch()`; stack painting in `sys_f1.c`.
* Navigation: `core/nmea.c`, `core/geo.c`, `apps/gps_nav.c`.

## How to verify on the board

1. `make TARGET=f100 APP=blink flash`: both LEDs blink at 1 Hz.
2. `make TARGET=f100 APP=cli flash`, then `picocom -b 115200 /dev/ttyUSB0` with a
   USB-serial adapter on PA9 (TX) / PA10 (RX) and GND.
3. `info` (clock and reset cause), `bench` (real cycle counts), `gpio monitor PA0`
   while pressing the USER button, `adc read temp`.
