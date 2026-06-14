#ifndef GPIO_H
#define GPIO_H
#include <stdint.h>

/* ESP32-S3 GPIO BSP 驱动
 *
 * 基地址 0x60004000。关键寄存器：
 *   0x20 ENABLE    输出使能（读-改-写）
 *   0x08 W1TS      置高（只写：写 1 的 bit 置位，写 0 的不变）
 *   0x0C W1TC      置低（同上）
 *   0x04 OUT        输出状态（可读）
 *   0x3C IN         输入状态（只读）
 *
 * W1TS/W1TC 天然支持多任务并发——不需要锁。
 */

void gpio_set_output( uint32_t gpio_num );
void gpio_set_high( uint32_t gpio_num );
void gpio_set_low( uint32_t gpio_num );
void gpio_toggle( uint32_t gpio_num );
uint32_t gpio_read( uint32_t gpio_num );

#endif
