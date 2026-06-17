#ifndef PWM_H
#define PWM_H
#include <stdint.h>

/* ESP32-S3 LEDC PWM 驱动（BSP 层：电机速度控制）
 *
 * 教学：PWM（脉宽调制）用"占空比"模拟不同电压。
 * 周期固定、高电平时间可变 → 平均电压 = VCC × (duty/max)。
 * 例：周期 1000，duty=500 → 50% 占空比 → 电机以半速旋转。
 *
 * LEDC 外设：
 *   基地址：0x60019000
 *   8 个定时器（Timer0-7）：控制频率和分辨率
 *   8 个通道（Channel0-7）：控制输出 GPIO 和占空比
 *
 *   每个通道绑定到一个定时器。同一频率的多个通道可共享一个定时器。
 *
 * 配置公式：
 *   频率 = APB_CLK / (ledc_clk_div × 2^duty_resolution)
 *   其中 APB_CLK = 80 MHz
 *
 * 例：10-bit 分辨率 (0-1023)，1 kHz PWM
 *   clk_div = 80,000,000 / (1000 × 1024) ≈ 78
 */

void pwm_init( uint32_t gpio, uint32_t freq_hz, uint32_t resolution_bits );
void pwm_set_duty( uint32_t gpio, uint32_t duty );

#endif
