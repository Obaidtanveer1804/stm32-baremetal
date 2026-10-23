/**
 * @file    hal_irq.h
 * @brief   Global interrupt masking / critical sections.
 *
 *   uint32_t s = hal_irq_save();   // disable, remember previous state
 *   shared_counter++;              // no ISR can interrupt this
 *   hal_irq_restore(s);            // re-enable only if it was enabled
 *
 * Nesting-safe, unlike a plain disable/enable pair.
 */
#ifndef HAL_IRQ_H
#define HAL_IRQ_H

#include "hal_types.h"

uint32_t hal_irq_save(void);
void     hal_irq_restore(uint32_t state);
void     hal_irq_enable_all(void);
void     hal_irq_disable_all(void);

/** Statement-block critical section:
 *  HAL_CRITICAL({ a++; b++; }); */
#define HAL_CRITICAL(block)                  \
    do {                                     \
        const uint32_t irq_state_ = hal_irq_save(); \
        block                                \
        hal_irq_restore(irq_state_);         \
    } while (0)

#endif /* HAL_IRQ_H */
