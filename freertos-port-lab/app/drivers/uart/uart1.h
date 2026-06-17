#ifndef UART1_H
#define UART1_H
#include <stdint.h>

/* ESP32-S3 UART1 驱动（BSP 层：Linux 通信通道）
 *
 * 硬件：
 *   GPIO17 = TX, GPIO18 = RX
 *   波特率 = 115200, 8N1
 *
 * 与 console.c（USB-Serial/JTAG 调试口）的区别：
 *   console.c 走内置 USB-JTAG → /dev/ttyACM0（调试用）
 *   uart1.c   走物理 UART1 引脚 → /dev/ttyS0 或 USB-TTL（数据用）
 *
 * UART1 基地址：0x60010000
 * APB 时钟：80MHz
 * 波特率计算公式：CLKDIV = APB_CLK / (16 × baud)
 *   115200: 80000000 / (16 × 115200) ≈ 43.4
 *   → 整数部分 43, 小数部分 (0.4 × 16) ≈ 6
 */

void uart1_init( uint32_t baud_rate );
void uart1_putc( char c );
void uart1_puts( const char *s );
int  uart1_getc( void );          /* 阻塞读取一个字节，无数据时返回 -1 */
int  uart1_rx_available( void );  /* RX FIFO 中可读的字节数 */

#endif
