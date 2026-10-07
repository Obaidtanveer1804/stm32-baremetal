# stm32-baremetal

Bare-metal C programming on the **STM32VL-Discovery** (STM32F100RB, Cortex-M3, 24 MHz)
using [libopencm3](https://github.com/libopencm3/libopencm3). Coursework for the ELISE
microcontrollers TP (*Baremetal C Programming on STM32*, J.-M. Friedt).

Code is organised so platform-independent logic (`main_*.c`) is separated from
platform-dependent drivers (`uart_f1.c`, `uart_f4.c`, `uart_pc.c`, `spi_*.c`, ...). The same
`main` can be linked against a hardware driver or a PC stub.

Based on [jmfriedt/stm32](https://github.com/jmfriedt/stm32) (remote `upstream`).

## Hardware

| Board | MCU | Notes |
|---|---|---|
| STM32VL-Discovery | STM32F100RB | LEDs PC8 (green) / PC9 (blue), on-board ST-Link/V1 |
| Custom board (optional) | STM32F103 | LEDs PC1 / PC2, flashed over serial |

## Prerequisites

- `arm-none-eabi-gcc`, `arm-none-eabi-newlib`, `make`
- `openocd` and/or `stlink-tools` (`st-flash`)
- libopencm3, cloned and built for the F1 family:

  ```sh
  git clone https://github.com/libopencm3/libopencm3
  cd libopencm3 && make TARGETS=stm32/f1
  ```

## Build

`REPERTOIRE` in the `Makefile` points to the libopencm3 directory. Override it for your layout:

```sh
make REPERTOIRE=/path/to/libopencm3
```

Other targets: `Makefile.stm32f100` (VL-Discovery), `Makefile.stm32f1` (F103), `Makefile.stm32f4`,
`Makefile.stellaris`, `Makefile.pc` (host stubs), e.g. `make -f Makefile.pc`.

Output: `test.elf` and `test.bin`.

## Flash

```sh
openocd -f board/stm32vldiscovery.cfg -c "program test.elf verify reset exit"
# or
st-flash write test.bin 0x8000000
```

### Troubleshooting

- **`Can not connect to target` / `unable to connect to the target` / `LIBUSB_ERROR_PIPE`**:
  the ST-Link/V1 (`0483:3744`) must not be claimed by the kernel as a USB drive. Add

  ```
  options usb-storage quirks=483:3744:i
  ```

  to `/etc/modprobe.d/usb-storage.conf`, reload `usb_storage` (or reboot) and replug the board.
- Chip ID reported as `0x000` by `st-info --probe`: the ST-Link cannot reach the target. Check
  the CN3 jumpers (both fitted), try another cable/port, or try holding RESET while connecting.
- After an openocd flash the board may hang: unplug and replug it.

## Repository layout

| Path | Purpose |
|---|---|
| `main_uart.c` | UART/LED example (start here) |
| `uart_*.c`, `spi_*.c`, `uart.h`, `spi.h` | Platform-specific drivers behind a common interface |
| `factoriel.c` | Small program for GDB exercises |
| `lorenz/` | Fixed-point vs floating-point Lorenz system |
| `*.ld` | Linker scripts |
| `docs/upstream-notes.md` | Original upstream README (GDB/QEMU instructions) |

## Git workflow

- `main`: untouched baseline snapshot of the upstream code; not developed on.
- `stm32`: integration branch; all work lands here.
- `feature/<name>`: one branch per feature, branched from `stm32` and merged back via pull request.

```sh
git switch stm32 && git pull
git switch -c feature/<name>
# ... work, commit ...
git push -u origin feature/<name>
gh pr create --base stm32
```

## License

See [LICENSE](LICENSE).
