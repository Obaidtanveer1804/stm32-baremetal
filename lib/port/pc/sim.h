/**
 * @file    sim.h
 * @brief   PC simulation controls (only exists in the PC port).
 *
 * The PC port behaves like the board:
 *  - SIGALRM fires every millisecond = SysTick interrupt. Timer
 *    callbacks, EXTI callbacks and UART RX run "in interrupt context"
 *    (inside the signal handler); hal_irq_save() blocks the signal,
 *    exactly like masking interrupts with PRIMASK.
 *  - The terminal is the console UART (raw mode: no line buffering,
 *    no echo, Enter sends '\r' like a serial terminal).
 *  - GPIO / PWM / ADC / SPI / I2C activity can be logged ("sim log").
 *  - Inputs (buttons, ADC voltages) are driven with the sim_* calls,
 *    from a test program or from the CLI command "sim".
 *  - Device models (24C02 EEPROM, LM75 sensor, W25Q flash) answer on
 *    the I2C / SPI buses so the drivers can be tested without a board.
 */
#ifndef SIM_H
#define SIM_H

#include "hal.h"

/* -------------------------- logging ------------------------------- */
#define SIM_LOG_GPIO   (1U << 0)
#define SIM_LOG_PWM    (1U << 1)
#define SIM_LOG_ADC    (1U << 2)
#define SIM_LOG_SPI    (1U << 3)
#define SIM_LOG_I2C    (1U << 4)
#define SIM_LOG_UART   (1U << 5)
#define SIM_LOG_TIMER  (1U << 6)
#define SIM_LOG_FLASH  (1U << 7)
#define SIM_LOG_EXTI   (1U << 8)
#define SIM_LOG_SYS    (1U << 9)
#define SIM_LOG_ALL    (0xFFFFU)

void     sim_set_log(uint32_t mask);
uint32_t sim_get_log(void);
/** Parse "gpio", "pwm", ..., "all", "none" into a mask bit. */
bool     sim_parse_log(const char *name, uint32_t *mask);
const char *sim_log_name(uint32_t bit);
/** Print "[sim] ..." if @p category is enabled (async-signal safe). */
void     sim_log(uint32_t category, const char *fmt, ...)
            __attribute__((format(printf, 2, 3)));

/* --------------------------- GPIO --------------------------------- */
/** Drive an input pin from outside (a button, a signal generator). */
void     sim_gpio_input(hal_pin_t pin, bool level);
/** Stop driving the pin: it falls back to its pull-up / pull-down. */
void     sim_gpio_release(hal_pin_t pin);
/** Show pins as an ASCII LED bar after every change ("sim leds"). */
void     sim_gpio_show_leds(bool enable);

/* ---------------------------- ADC --------------------------------- */
typedef enum {
    SIM_WAVE_CONST = 0,
    SIM_WAVE_SINE,
    SIM_WAVE_TRIANGLE,
    SIM_WAVE_SQUARE,
    SIM_WAVE_RAMP,
    SIM_WAVE_NOISE,
    SIM_WAVE_COUNT
} sim_wave_t;

void        sim_adc_set(uint8_t channel, uint16_t raw);
void        sim_adc_wave(uint8_t channel, sim_wave_t wave, uint16_t center,
                         uint16_t amplitude, uint32_t period_ms);
/** Add +-noise LSB of random noise to every conversion. */
void        sim_adc_noise(uint16_t lsb);
bool        sim_parse_wave(const char *name, sim_wave_t *wave);
const char *sim_wave_name(sim_wave_t wave);

/* ---------------------------- UART -------------------------------- */
/** Non-console UARTs: echo TX back to RX (like a TX-RX jumper). */
void sim_uart_loopback(hal_uart_t uart, bool enable);
/** Pretend bytes arrived on a UART. */
void sim_uart_inject(hal_uart_t uart, const uint8_t *data, size_t len);

/* ------------------------- device models -------------------------- */
typedef struct sim_i2c_dev {
    const char *name;
    uint8_t     addr;                  /**< 7-bit address               */
    /** START + address: return false to NACK (e.g. EEPROM busy writing) */
    bool      (*start)(struct sim_i2c_dev *dev, bool read);
    bool      (*write)(struct sim_i2c_dev *dev, uint8_t byte);  /**< ACK? */
    uint8_t   (*read)(struct sim_i2c_dev *dev);
    void      (*stop)(struct sim_i2c_dev *dev);
    void       *ctx;
    struct sim_i2c_dev *next;
} sim_i2c_dev_t;

typedef struct sim_spi_dev {
    const char *name;
    hal_pin_t   cs;                    /**< active-low chip select pin  */
    void      (*select)(struct sim_spi_dev *dev, bool selected);
    uint8_t   (*transfer)(struct sim_spi_dev *dev, uint8_t mosi);
    void       *ctx;
    struct sim_spi_dev *next;
} sim_spi_dev_t;

void sim_i2c_attach(hal_i2c_t bus, sim_i2c_dev_t *dev);
void sim_spi_attach(hal_spi_t bus, sim_spi_dev_t *dev);
/** List attached devices through io.h. */
void sim_devices_list(void);
/**
 * Default bench, mirroring a typical TP breadboard:
 *   I2C1 0x50 : 24C02 EEPROM (256 B, 8-byte pages)
 *   I2C1 0x48 : LM75 temperature sensor (follows sim_temp)
 *   I2C1 0x68 : DS1307-like RTC (BCD time registers)
 *   SPI1 CS=PA4 : W25Q16 SPI NOR flash (2 MiB, JEDEC EF 40 15)
 *   I2C1 0x3C : SSD1306 128x64 OLED
 */
void sim_devices_install_default(void);
/** Print the simulated OLED screen in the terminal ("sim oled"). */
void sim_oled_show(void);
/** Temperature seen by the simulated sensors, in 0.1 C. */
void sim_set_temperature_c10(int32_t c10);
int32_t sim_get_temperature_c10(void);

/* ---------------------------- misc -------------------------------- */
/** Print the full simulator state (pins, PWM, timers...). */
void sim_status(void);
/** Clean shutdown: restore the terminal, save the flash file. */
void sim_shutdown(int code) __attribute__((noreturn));

#endif /* SIM_H */
