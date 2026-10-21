/**
 * @file    modbus_node.c
 * @brief   The board as a Modbus RTU slave on RS-485 (MAX485 module):
 *          a PLC / SCADA / Python (pymodbus, mbpoll) can read its
 *          sensors and drive its outputs.
 *
 *   USART2 PA2 TX -> DI, PA3 RX <- RO, PA1 -> DE+RE (high = transmit)
 *   slave address 1, 19200 8N1
 *
 *   input registers (0x04)    0..3 ADC ch0..3 raw   4 MCU temp (0.1 C)
 *                             5 VDDA mV             6-7 uptime s (hi, lo)
 *   holding registers (0x03/0x06/0x10)
 *                             0 LED blink period ms (0 = off)
 *                             1 PWM duty on T3C1 (per-mille)
 *   coils (0x01/0x05/0x0F)    0 blue LED   1 green LED
 *
 *   test from a PC:  mbpoll -m rtu -a 1 -b 19200 -t 3 -r 1 -c 8 /dev/ttyUSB0
 *
 * PC simulation: a built-in "master" sends a few requests over the
 * simulated UART and prints the answers.
 */
#include "hal.h"
#include "io.h"
#include "log.h"
#include "modbus.h"
#include "hexdump.h"
#ifdef BOARD_PC
#include "sim.h"
#endif

#define MB_UART        HAL_UART2
#define MB_BAUD        (19200U)
#define MB_ADDRESS     (1U)
#define MB_DE_PIN      PA(1)
#define PWM_CH         HAL_PWM_T3C1

#define IR_COUNT       (8U)
#define HR_COUNT       (2U)
#define COIL_COUNT     (2U)

static uint16_t       s_input[IR_COUNT];
static uint16_t       s_holding[HR_COUNT] = { 500U, 0U };
static uint8_t        s_coils[1];
static modbus_slave_t s_mb;

static void rs485_send(const uint8_t *frame, size_t len, void *ctx)
{
    (void)ctx;
    hal_gpio_set(MB_DE_PIN);            /* driver on */
    hal_uart_write(MB_UART, frame, len);
    hal_uart_tx_wait(MB_UART);          /* last stop bit out before DE low */
    hal_gpio_clear(MB_DE_PIN);
#ifdef BOARD_PC
    io_puts("slave -> ");
    for (size_t i = 0U; i < len; i++) {
        io_printf("%02X ", frame[i]);
    }
    io_puts(IO_NEWLINE);
#endif
}

static void on_write(uint8_t function, uint16_t addr, uint16_t count, void *ctx)
{
    (void)ctx;
    LOG_I("mb", "fc %02X wrote %u item(s) at %u", (unsigned)function, (unsigned)count, (unsigned)addr);
    hal_pwm_set_duty(PWM_CH, s_holding[1]);
}

static void refresh_inputs(void)
{
    for (uint8_t ch = 0U; ch < 4U; ch++) {
        s_input[ch] = hal_adc_read(ch);
    }
    s_input[4] = (uint16_t)hal_adc_temp_c10();
    s_input[5] = (uint16_t)hal_adc_vdda_mv();
    const uint32_t up = hal_time_ms() / 1000U;
    s_input[6] = (uint16_t)(up >> 16);
    s_input[7] = (uint16_t)up;
}

static void apply_outputs(void)
{
    static uint32_t last_blink;
    const bool      blue  = (s_coils[0] & 0x01U) != 0U;
    const bool      green = (s_coils[0] & 0x02U) != 0U;
    hal_gpio_write(BOARD_LED_GREEN, green);
    if (s_holding[0] == 0U) {
        hal_gpio_write(BOARD_LED_BLUE, blue);
    } else if ((hal_time_ms() - last_blink) >= s_holding[0]) {
        last_blink = hal_time_ms();
        hal_gpio_toggle(BOARD_LED_BLUE);
    } else {
        /* waiting for the next blink */
    }
}

#ifdef BOARD_PC
static void master_send(const uint8_t *req, size_t len)
{
    io_puts(IO_NEWLINE "master -> ");
    for (size_t i = 0U; i < len; i++) {
        io_printf("%02X ", req[i]);
    }
    io_puts(IO_NEWLINE);
    sim_uart_inject(MB_UART, req, len);
}

static void bench(uint32_t step)
{
    uint8_t req[16];
    switch (step) {
    case 1U: master_send(req, modbus_build_request(req, MB_ADDRESS, 0x04U, 0U, 8U)); break;
    case 2U: master_send(req, modbus_build_request(req, MB_ADDRESS, 0x05U, 1U, 0xFF00U)); break;
    case 3U: master_send(req, modbus_build_request(req, MB_ADDRESS, 0x06U, 1U, 250U)); break;
    case 4U: master_send(req, modbus_build_request(req, MB_ADDRESS, 0x03U, 0U, 2U)); break;
    case 5U: master_send(req, modbus_build_request(req, MB_ADDRESS, 0x03U, 7U, 1U)); break;   /* exception */
    case 6U:
        io_printf(IO_NEWLINE "frames %lu, crc errors %lu, exceptions %lu, PWM duty %lu" IO_NEWLINE,
                  (unsigned long)s_mb.frames_ok, (unsigned long)s_mb.crc_errors,
                  (unsigned long)s_mb.exceptions, (unsigned long)hal_pwm_get_duty(PWM_CH));
        sim_shutdown(0);
        break;
    default: break;
    }
}
#endif

int main(void)
{
    hal_sys_init();
    hal_time_init();
    hal_console_init(BOARD_CONSOLE_BAUD);
    io_init(hal_console_putc, hal_console_getc);
    log_init(hal_time_ms);
    (void)hal_adc_init();
    (void)hal_uart_init(MB_UART, MB_BAUD);
    (void)hal_pwm_init(PWM_CH, 1000U);
    hal_gpio_mode(MB_DE_PIN, HAL_GPIO_OUTPUT);
    hal_gpio_clear(MB_DE_PIN);
    hal_gpio_mode(BOARD_LED_BLUE, HAL_GPIO_OUTPUT);
    hal_gpio_mode(BOARD_LED_GREEN, HAL_GPIO_OUTPUT);

    modbus_init(&s_mb, MB_ADDRESS, rs485_send, NULL);
    modbus_set_baud(&s_mb, MB_BAUD);
    s_mb.input          = s_input;
    s_mb.input_count    = IR_COUNT;
    s_mb.holding        = s_holding;
    s_mb.holding_count  = HR_COUNT;
    s_mb.coils          = s_coils;
    s_mb.coil_count     = COIL_COUNT;
    s_mb.on_write       = on_write;
    LOG_I("mb", "Modbus RTU slave %u at %lu baud", (unsigned)MB_ADDRESS, (unsigned long)MB_BAUD);

    uint32_t last_refresh = 0U;
#ifdef BOARD_PC
    uint32_t bench_step = 0U;
    uint32_t bench_t    = hal_time_ms();
#endif
    for (;;) {
        int c;
        while ((c = hal_uart_getc(MB_UART)) >= 0) {
            modbus_rx_byte(&s_mb, (uint8_t)c, hal_time_us());
        }
        (void)modbus_poll(&s_mb, hal_time_us());
        if ((hal_time_ms() - last_refresh) >= 100U) {
            last_refresh = hal_time_ms();
            refresh_inputs();
        }
        apply_outputs();
#ifdef BOARD_PC
        if ((hal_time_ms() - bench_t) >= 200U) {
            bench_t = hal_time_ms();
            bench(++bench_step);
        }
#endif
    }
    return 0;
}
