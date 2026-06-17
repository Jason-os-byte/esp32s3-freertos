#include "pwm.h"

#define REG_RD(a)  (*(volatile uint32_t*)(a))
#define REG_WR(a,v) (*(volatile uint32_t*)(a)=(v))

/* ---- LEDC 寄存器 ---- */
#define LEDC_BASE               0x60019000UL
#define LEDC_TIMER0_CONF        (LEDC_BASE + 0xA0U)
#define LEDC_CH0_CONF0          (LEDC_BASE + 0x00U)
#define LEDC_CH0_DUTY           (LEDC_BASE + 0x08U)

/* ---- GPIO Matrix：把 LEDC 信号路由到物理引脚 ---- */
#define GPIO_BASE               0x60004000UL
#define GPIO_FUNC_OUT_SEL(n)    (GPIO_BASE + 0x554U + (n)*4U)
#define LEDC_CH0_SIG            73U      /* LEDC_LS_SIG_OUT0_IDX */

/* ---- IO_MUX ---- */
#define IO_MUX_REG(n)           (0x60009000UL + 0x04U + (n)*4U)

/* ---- SYSTEM 总线时钟 ---- */
#define SYSTEM_PERIP_CLK_EN0_REG  0x600C0018UL
#define SYSTEM_LEDC_CLK_EN       (1UL << 11)  /* bit11 = LEDC (ESP-IDF system_reg.h) */
#define SYSTEM_PERIP_RST_EN0_REG  0x600C0020UL
#define SYSTEM_LEDC_RST          (1UL << 11)

/* ================================================================
 * 教学：PWM 频率计算
 *
 * LEDC 用一个 n 位计数器不断从 0 加到 (2^n - 1)，再归零。
 * 当计数器 < duty 值时输出高，>= duty 值输出低。
 *
 * 频率 = APB_CLK / (clk_div × 2^resolution)
 *   APB_CLK = 80,000,000 Hz
 *   resolution = 10 bits → 2^10 = 1024
 *   freq = 1000 Hz
 *   → clk_div = 80,000,000 / (1000 × 1024) ≈ 78
 *
 *   验证：1 kHz PWM 周期 = 1ms，每个 tick = 78/80000000 = 0.975μs
 *         1024 ticks × 0.975μs ≈ 1ms ✓
 * ================================================================ */

void pwm_init( uint32_t gpio, uint32_t freq_hz, uint32_t resolution_bits )
{
    /* ---- 1. 开 LEDC 总线时钟 + 释放复位 ---- */
    REG_WR( SYSTEM_PERIP_CLK_EN0_REG,
            REG_RD( SYSTEM_PERIP_CLK_EN0_REG ) | SYSTEM_LEDC_CLK_EN );
    REG_WR( SYSTEM_PERIP_RST_EN0_REG,
            REG_RD( SYSTEM_PERIP_RST_EN0_REG ) | SYSTEM_LEDC_RST );
    REG_WR( SYSTEM_PERIP_RST_EN0_REG,
            REG_RD( SYSTEM_PERIP_RST_EN0_REG ) & ~SYSTEM_LEDC_RST );

    /* ---- 2. 配置 LEDC Timer0 ----
     *
     * 字段（TIMER0_CONF_REG）：
     *   DUTY_RES[3:0]: 分辨率减1。10-bit → 写 9
     *   CLK_DIV[21:4]: 时钟分频系数
     *   TICK_SEL[24]: 0=APB_CLK(80MHz), 1=REF_TICK(1MHz)
     *   RST[23]: 1=复位保持, 0=运行
     *   PAUSE[22]: 0=运行, 1=暂停
     */

    if ( resolution_bits < 2  ) resolution_bits = 2;
    if ( resolution_bits > 14 ) resolution_bits = 14;

    uint32_t max_duty = ( 1UL << resolution_bits ) - 1;
    uint32_t clk_div  = 80000000UL / ( freq_hz * ( 1UL << resolution_bits ) );
    if ( clk_div == 0 ) clk_div = 1;

    /* 先释放 RST，写入基础配置 */
    uint32_t tconf = ( ( resolution_bits - 1 ) & 0xFU )   /* DUTY_RES[3:0] */
                   | ( ( clk_div & 0x3FFFFU ) << 4 );      /* CLK_DIV[21:4] */
    REG_WR( LEDC_TIMER0_CONF, tconf );                     /* 先写配置值 */
    REG_WR( LEDC_TIMER0_CONF, tconf | ( 1U << 25 ) );      /* 再写一次 + PARA_UP 锁存 */

    /* ---- 3. GPIO Matrix：把 LEDC CH0 信号路由到目标 GPIO ----
     * 寄存器 = GPIO_FUNC_OUT_SEL(gpio)
     * 写入值 = 信号ID | OEN_SEL(bit10)  ← OEN=1 让外设控制输出
     */
    REG_WR( GPIO_FUNC_OUT_SEL( gpio ), LEDC_CH0_SIG | ( 1U << 10 ) );

    /* IO_MUX：GPIO 模式 */
    REG_WR( IO_MUX_REG( gpio ), ( 1U << 10 ) | ( 3U << 2 ) );

    /* ---- 4. LEDC Channel0 配置 ----
     * TIMER_SEL=0, SIG_OUT_EN=1, 先写基础值再锁存 */
    uint32_t ch0conf = ( 0U << 0 ) | ( 1U << 2 );         /* TIMER_SEL=0, SIG_OUT_EN=1 */
    REG_WR( LEDC_CH0_CONF0, ch0conf );
    REG_WR( LEDC_CH0_CONF0, ch0conf | ( 1U << 29 ) );     /* +PARA_UP 锁存 */

    /* 初始占空比 0 */
    uint32_t duty_val = 0U;
    REG_WR( LEDC_CH0_DUTY, duty_val );
    REG_WR( LEDC_CH0_DUTY, duty_val | ( 1U << 25 ) );     /* +PARA_UP 锁存 */
}

void pwm_set_duty( uint32_t gpio, uint32_t duty )
{
    (void)gpio;
    uint32_t v = ( duty & 0x1FFFFFFU );
    REG_WR( LEDC_CH0_DUTY, v );
    REG_WR( LEDC_CH0_DUTY, v | ( 1U << 25 ) | ( 1U << 31 ) );  /* PARA_UP + DUTY_START */
}
