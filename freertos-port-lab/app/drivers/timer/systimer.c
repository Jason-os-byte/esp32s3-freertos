#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "systimer.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))
#define BIT(n)      (1UL << (n))

/* ESP32-S3 SYSTIMER register addresses from ESP-IDF v5.5
 * components/soc/esp32s3/register/soc/systimer_reg.h
 */
#define SYSTEM_PERIP_CLK_EN0_REG      0x600C0018UL
#define SYSTEM_PERIP_RST_EN0_REG      0x600C0020UL
#define SYSTEM_SYSTIMER_CLK_EN        BIT(29)
#define SYSTEM_SYSTIMER_RST           BIT(29)

#define SYSTIMER_CONF_REG             0x60023000UL
#define SYSTIMER_UNIT0_LOAD_HI_REG    0x6002300CUL
#define SYSTIMER_UNIT0_LOAD_LO_REG    0x60023010UL
#define SYSTIMER_TARGET0_HI_REG       0x6002301CUL
#define SYSTIMER_TARGET0_LO_REG       0x60023020UL
#define SYSTIMER_TARGET0_CONF_REG     0x60023034UL
#define SYSTIMER_COMP0_LOAD_REG       0x60023050UL
#define SYSTIMER_UNIT0_LOAD_REG       0x6002305CUL
#define SYSTIMER_INT_ENA_REG          0x60023064UL
#define SYSTIMER_INT_RAW_REG          0x60023068UL
#define SYSTIMER_INT_CLR_REG          0x6002306CUL

#define SYSTIMER_CLK_EN               BIT(31)
#define SYSTIMER_TIMER_UNIT0_WORK_EN  BIT(30)
#define SYSTIMER_TARGET0_WORK_EN      BIT(24)
#define SYSTIMER_TARGET0_PERIOD_MODE  BIT(30)
#define SYSTIMER_TARGET0_INT          BIT(0)

/* ESP32-S3 interrupt matrix: SYSTIMER target0 source 57 -> CPU int 19. */
#define INTERRUPT_CORE0_SYSTIMER_TARGET0_INT_MAP_REG 0x600C20E4UL
#define INTERRUPT_CORE0_CLOCK_GATE_REG               0x600C219CUL
#define SYSTIMER_CPU_INTR_NUM                        19UL
#define SYSTIMER_CPU_INTR_MASK                       BIT(SYSTIMER_CPU_INTR_NUM)

/* ESP32-S3 SYSTIMER: XTAL 40MHz / 2.5 = 16MHz, 即 16 ticks/us。 */
#define SYSTIMER_TICKS_PER_US         16UL
#define SYSTIMER_TARGET_PERIOD_MASK   0x03FFFFFFUL

static volatile uint32_t g_systimer_irq_ticks;

static inline void cpu_intr_enable_mask(uint32_t mask)
{
    uint32_t v;
    __asm__ volatile ("rsr %0, intenable" : "=a"(v));
    v |= mask;
    __asm__ volatile ("wsr %0, intenable\n\trsync" :: "a"(v) : "memory");
}

static inline void cpu_intr_global_enable(void)
{
    uint32_t old_ps;
    __asm__ volatile ("rsil %0, 0" : "=a"(old_ps) :: "memory");
}

void systimer_irq_init(uint32_t period_us)
{
    uint32_t period_ticks = period_us * SYSTIMER_TICKS_PER_US;

    if (period_ticks == 0) {
        period_ticks = 1;
    }
    if (period_ticks > SYSTIMER_TARGET_PERIOD_MASK) {
        period_ticks = SYSTIMER_TARGET_PERIOD_MASK;
    }

    g_systimer_irq_ticks = 0;

    /* 1) Enable SYSTIMER peripheral bus clock and release reset.
     *    Although default is often already enabled, bootloader may leave
     *    SYSTEM_SYSTIMER_RST or gate in an unknown state. */
    REG32(SYSTEM_PERIP_CLK_EN0_REG) |= SYSTEM_SYSTIMER_CLK_EN;
    REG32(SYSTEM_PERIP_RST_EN0_REG) |= SYSTEM_SYSTIMER_RST;
    REG32(SYSTEM_PERIP_RST_EN0_REG) &= ~SYSTEM_SYSTIMER_RST;

    /* 2) Disable target0 alarm while configuring. */
    REG32(SYSTIMER_INT_ENA_REG) &= ~SYSTIMER_TARGET0_INT;
    REG32(SYSTIMER_INT_CLR_REG) = SYSTIMER_TARGET0_INT;

    /* 3) Enable SYSTIMER clock and timer unit0; keep target0 off for now. */
    uint32_t conf = REG32(SYSTIMER_CONF_REG);
    conf |= (SYSTIMER_CLK_EN | SYSTIMER_TIMER_UNIT0_WORK_EN);
    conf &= ~SYSTIMER_TARGET0_WORK_EN;
    REG32(SYSTIMER_CONF_REG) = conf;

    /* 4) Counter0 starts from zero. */
    REG32(SYSTIMER_UNIT0_LOAD_HI_REG) = 0;
    REG32(SYSTIMER_UNIT0_LOAD_LO_REG) = 0;
    REG32(SYSTIMER_UNIT0_LOAD_REG) = 1;

    /* 5) Target0 period mode, unit0.
     *    Do NOT write TARGET0_LO/HI for periodic mode (HAL only sets
     *    target_conf.[period,period_mode,timer_unit_sel] and comp_load). */
    REG32(SYSTIMER_TARGET0_HI_REG) = 0;
    REG32(SYSTIMER_TARGET0_LO_REG) = 0;
    REG32(SYSTIMER_TARGET0_CONF_REG) =
        (period_ticks & SYSTIMER_TARGET_PERIOD_MASK) |
        SYSTIMER_TARGET0_PERIOD_MODE;
    REG32(SYSTIMER_COMP0_LOAD_REG) = 1;

    /* 6) Now enable target0 alarm in CONF. */
    conf = REG32(SYSTIMER_CONF_REG);
    conf |= SYSTIMER_TARGET0_WORK_EN;
    REG32(SYSTIMER_CONF_REG) = conf;

    /* 7) Clear stale and enable peripheral IRQ. */
    REG32(SYSTIMER_INT_CLR_REG) = SYSTIMER_TARGET0_INT;
    REG32(SYSTIMER_INT_ENA_REG) |= SYSTIMER_TARGET0_INT;

    /* 8) Route to CPU and enable int line */
    REG32(INTERRUPT_CORE0_CLOCK_GATE_REG) |= 1UL;
    REG32(INTERRUPT_CORE0_SYSTIMER_TARGET0_INT_MAP_REG) =
        SYSTIMER_CPU_INTR_NUM;
    cpu_intr_enable_mask(SYSTIMER_CPU_INTR_MASK);
    cpu_intr_global_enable();
}

uint32_t systimer_irq_tick_count(void)
{
    return g_systimer_irq_ticks;
}

uint32_t systimer_irq_raw_status(void)
{
    return REG32(SYSTIMER_INT_RAW_REG) & SYSTIMER_TARGET0_INT;
}

void systimer_target0_isr(void)
{
    if (REG32(SYSTIMER_INT_RAW_REG) & SYSTIMER_TARGET0_INT) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        REG32(SYSTIMER_INT_CLR_REG) = SYSTIMER_TARGET0_INT;
        g_systimer_irq_ticks++;

        /* 通知 FreeRTOS：一个 tick 已过去，唤醒延迟任务 */
        if ( xTaskIncrementTick() != pdFALSE ) {
            xHigherPriorityTaskWoken = pdTRUE;
        }

        /* ISR notify: 每 1000 ticks (1 秒) 通知 notify_task */
        {
            static uint32_t notify_counter;
            extern TaskHandle_t g_notify_task_handle;

            if ( g_notify_task_handle != NULL ) {
                notify_counter++;
                if ( notify_counter >= 1000UL ) {
                    notify_counter = 0;
                    vTaskNotifyGiveFromISR( g_notify_task_handle,
                                            &xHigherPriorityTaskWoken );
                }
            }
        }

        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
}
