#ifndef ADC_H
#define ADC_H
#include <stdint.h>

/* ESP32-S3 ADC1 裸机驱动（带完整 REGI2C 初始化和校准）
 *
 * ESP32-S3 ADC1 特性：
 *   - 12 位分辨率（0~4095）
 *   - 8 个输入通道
 *   - SAR（逐次逼近）架构
 *   - 内部 REGI2C 总线用于模拟前端配置
 *
 * 初始化流程（对齐 ESP-IDF）：
 *   1. APB_SARADC 总线时钟使能 + 复位
 *   2. REGI2C 主站（RTC I2C）时钟 + 时序配置
 *   3. RTC I2C 模拟总线使能（SAR_I2C_PU + I2C_SAR_M + ANA_SAR_CFG2_M）
 *   4. SAR ADC 电源开启（FSM → POWER_ON）
 *   5. SAR 时钟分频（sar1_clk_div=1）
 *   6. RTC 控制器接管（start_force + en_pad_force）
 *   7. REGI2C 校准初始化（DREF=4, 采样周期=2）
 *   8. APB_SARADC 数字控制器配置
 *
 * 使用流程：
 *   1. adc1_init()                          初始化 ADC1
 *   2. adc1_read(channel)                   读取指定通道的原始值
 *   3. adc1_to_mv(raw)                      转为毫伏（近似）
 */

void adc1_init( void );
uint32_t adc1_read( uint32_t channel );
uint32_t adc1_to_mv( uint32_t raw );

#endif
