/**
 * @file    uart_f1.c
 * @brief   hal_uart for USART1..3: RXNE interrupt feeds a ring buffer,
 *          TX is blocking (simple and deterministic).
 */
#include "hal_uart.h"
#include "hal_gpio.h"
#include "ringbuf.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/nvic.h>

typedef struct {
    uint32_t            base;
    enum rcc_periph_clken clock;
    uint8_t             irq;
    hal_pin_t           tx;
    hal_pin_t           rx;
} uart_hw_t;

static const uart_hw_t uart_hw[HAL_UART_COUNT] = {
    { USART1, RCC_USART1, NVIC_USART1_IRQ, PA(9),  PA(10) },
    { USART2, RCC_USART2, NVIC_USART2_IRQ, PA(2),  PA(3)  },
    { USART3, RCC_USART3, NVIC_USART3_IRQ, PB(10), PB(11) },
};

static uint8_t           s_rx_storage[HAL_UART_COUNT][HAL_UART_RX_BUFFER_SIZE];
static ringbuf_t         s_rx[HAL_UART_COUNT];
static volatile uint32_t s_rx_dropped[HAL_UART_COUNT];

hal_status_t hal_uart_init(hal_uart_t uart, uint32_t baud)
{
    if ((uart >= HAL_UART_COUNT) || (baud == 0U)) {
        return HAL_EINVAL;
    }
    const uart_hw_t *hw = &uart_hw[uart];

    (void)ringbuf_init(&s_rx[uart], s_rx_storage[uart], HAL_UART_RX_BUFFER_SIZE);
    s_rx_dropped[uart] = 0U;

    rcc_periph_clock_enable(hw->clock);
    hal_gpio_mode(hw->tx, HAL_GPIO_AF);
    hal_gpio_mode(hw->rx, HAL_GPIO_INPUT_PULLUP);   /* idle high when unplugged */

    usart_set_baudrate(hw->base, baud);
    usart_set_databits(hw->base, 8);
    usart_set_stopbits(hw->base, USART_STOPBITS_1);
    usart_set_parity(hw->base, USART_PARITY_NONE);
    usart_set_flow_control(hw->base, USART_FLOWCONTROL_NONE);
    usart_set_mode(hw->base, USART_MODE_TX_RX);
    usart_enable_rx_interrupt(hw->base);
    nvic_enable_irq(hw->irq);
    usart_enable(hw->base);
    return HAL_OK;
}

void hal_uart_putc(hal_uart_t uart, char ch)
{
    if (uart < HAL_UART_COUNT) {
        usart_send_blocking(uart_hw[uart].base, (uint16_t)(uint8_t)ch);
    }
}

void hal_uart_write(hal_uart_t uart, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    while (len-- > 0U) {
        hal_uart_putc(uart, (char)*p++);
    }
}

void hal_uart_puts(hal_uart_t uart, const char *s)
{
    while (*s != '\0') {
        hal_uart_putc(uart, *s++);
    }
}

int hal_uart_getc(hal_uart_t uart)
{
    uint8_t b;
    if ((uart < HAL_UART_COUNT) && ringbuf_get(&s_rx[uart], &b)) {
        return (int)b;
    }
    return -1;
}

uint32_t hal_uart_rx_available(hal_uart_t uart)
{
    return (uart < HAL_UART_COUNT) ? ringbuf_count(&s_rx[uart]) : 0U;
}

uint32_t hal_uart_rx_dropped(hal_uart_t uart)
{
    return (uart < HAL_UART_COUNT) ? s_rx_dropped[uart] : 0U;
}

void hal_uart_rx_flush(hal_uart_t uart)
{
    if (uart < HAL_UART_COUNT) {
        ringbuf_reset(&s_rx[uart]);
    }
}

void hal_uart_tx_wait(hal_uart_t uart)
{
    if (uart < HAL_UART_COUNT) {
        while ((USART_SR(uart_hw[uart].base) & USART_SR_TC) == 0U) {
        }
    }
}

/* ------------------------- interrupt handlers --------------------- */

static void uart_isr(hal_uart_t uart)
{
    const uint32_t base = uart_hw[uart].base;
    const uint32_t sr   = USART_SR(base);

    /* reading SR then DR clears RXNE and the error flags (ORE, FE...) */
    if ((sr & (USART_SR_RXNE | USART_SR_ORE)) != 0U) {
        const uint8_t byte = (uint8_t)USART_DR(base);
        if (!ringbuf_put(&s_rx[uart], byte)) {
            s_rx_dropped[uart]++;
        }
        if ((sr & USART_SR_ORE) != 0U) {
            s_rx_dropped[uart]++;     /* hardware overrun: a byte was lost */
        }
    }
}

void usart1_isr(void) { uart_isr(HAL_UART1); }
void usart2_isr(void) { uart_isr(HAL_UART2); }
void usart3_isr(void) { uart_isr(HAL_UART3); }
