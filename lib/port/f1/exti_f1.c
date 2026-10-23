/**
 * @file    exti_f1.c
 * @brief   hal_exti: AFIO line routing, EXTI edge config, NVIC, and one
 *          shared dispatcher for the 7 EXTI interrupt vectors.
 */
#include "hal_exti.h"
#include "hal_gpio.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>

#define EXTI_LINES  (16U)

typedef struct {
    hal_exti_cb_t     cb;
    void             *arg;
    hal_pin_t         pin;
    volatile uint32_t count;
} exti_slot_t;

static exti_slot_t s_slot[EXTI_LINES];

static const uint32_t port_base[HAL_PORT_COUNT] = { GPIOA, GPIOB, GPIOC, GPIOD, GPIOE };

/* EXTI0..4 have their own vector, 5..9 and 10..15 share one. */
static uint8_t line_irq(uint32_t line)
{
    static const uint8_t first[5] = {
        NVIC_EXTI0_IRQ, NVIC_EXTI1_IRQ, NVIC_EXTI2_IRQ, NVIC_EXTI3_IRQ, NVIC_EXTI4_IRQ
    };
    if (line < 5U) {
        return first[line];
    }
    return (line < 10U) ? NVIC_EXTI9_5_IRQ : NVIC_EXTI15_10_IRQ;
}

hal_status_t hal_exti_attach(hal_pin_t pin, hal_exti_edge_t edge,
                             hal_exti_cb_t cb, void *arg)
{
    if (!hal_gpio_pin_valid(pin) || (cb == NULL)) {
        return HAL_EINVAL;
    }
    const uint32_t line = HAL_PIN_NUM(pin);
    if ((s_slot[line].cb != NULL) && (s_slot[line].pin != pin)) {
        return HAL_EBUSY;             /* line taken by another port */
    }
    const uint32_t mask = 1UL << line;
    enum exti_trigger_type trig = EXTI_TRIGGER_BOTH;
    if (edge == HAL_EXTI_RISING) {
        trig = EXTI_TRIGGER_RISING;
    } else if (edge == HAL_EXTI_FALLING) {
        trig = EXTI_TRIGGER_FALLING;
    } else {
        /* both */
    }

    s_slot[line].cb    = cb;
    s_slot[line].arg   = arg;
    s_slot[line].pin   = pin;
    s_slot[line].count = 0U;

    rcc_periph_clock_enable(RCC_AFIO);
    exti_select_source(mask, port_base[HAL_PIN_PORT(pin)]);
    exti_set_trigger(mask, trig);
    exti_reset_request(mask);
    exti_enable_request(mask);
    nvic_enable_irq(line_irq(line));
    return HAL_OK;
}

void hal_exti_detach(hal_pin_t pin)
{
    if (!hal_gpio_pin_valid(pin)) {
        return;
    }
    const uint32_t line = HAL_PIN_NUM(pin);
    if (s_slot[line].pin == pin) {
        exti_disable_request(1UL << line);
        s_slot[line].cb = NULL;
        /* shared vectors stay enabled; the line itself is masked */
        if (line < 5U) {
            nvic_disable_irq(line_irq(line));
        }
    }
}

void hal_exti_enable(hal_pin_t pin, bool enable)
{
    if (!hal_gpio_pin_valid(pin)) {
        return;
    }
    const uint32_t mask = 1UL << HAL_PIN_NUM(pin);
    if (enable) {
        exti_reset_request(mask);
        exti_enable_request(mask);
    } else {
        exti_disable_request(mask);
    }
}

uint32_t hal_exti_count(hal_pin_t pin)
{
    return hal_gpio_pin_valid(pin) ? s_slot[HAL_PIN_NUM(pin)].count : 0U;
}

/* Serve every pending line in [first, last]. */
static void exti_dispatch(uint32_t first, uint32_t last)
{
    const uint32_t pending = EXTI_PR;
    for (uint32_t line = first; line <= last; line++) {
        const uint32_t mask = 1UL << line;
        if ((pending & mask) != 0U) {
            EXTI_PR = mask;            /* write 1 to clear */
            s_slot[line].count++;
            if (s_slot[line].cb != NULL) {
                s_slot[line].cb(s_slot[line].pin, s_slot[line].arg);
            }
        }
    }
}

void exti0_isr(void)     { exti_dispatch(0U, 0U); }
void exti1_isr(void)     { exti_dispatch(1U, 1U); }
void exti2_isr(void)     { exti_dispatch(2U, 2U); }
void exti3_isr(void)     { exti_dispatch(3U, 3U); }
void exti4_isr(void)     { exti_dispatch(4U, 4U); }
void exti9_5_isr(void)   { exti_dispatch(5U, 9U); }
void exti15_10_isr(void) { exti_dispatch(10U, 15U); }
