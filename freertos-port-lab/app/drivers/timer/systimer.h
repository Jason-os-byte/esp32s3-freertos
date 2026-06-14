#ifndef SYSTIMER_H
#define SYSTIMER_H

#include <stdint.h>

/* Step11: real SYSTIMER interrupt mode.
 *
 * SYSTIMER target0 is routed through the ESP32-S3 interrupt matrix to CPU
 * interrupt line 19 (level 2). The ISR increments a DRAM counter; main only
 * observes that counter and prints outside interrupt context.
 */
void systimer_irq_init(uint32_t period_us);
uint32_t systimer_irq_tick_count(void);
uint32_t systimer_irq_raw_status(void);
void systimer_target0_isr(void);

#endif /* SYSTIMER_H */
