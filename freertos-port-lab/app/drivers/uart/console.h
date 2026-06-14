#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* USB-Serial/JTAG 控制台。bootloader 已经把外设初始化好，
 * 这里只做最小 putc / puts / printf，目标是看到 "custom app alive"。
 *
 * 支持的 printf 转义：
 *   %s  字符串
 *   %c  单字符
 *   %d  有符号 int
 *   %u  无符号 int
 *   %x  无符号 int 十六进制（小写）
 *   %p  指针 (0x...)
 *   %%  百分号
 *
 * 不支持宽度/精度——裸机阶段足够。 */

void console_putc(char c);
void console_puts(const char *s);
void console_printf(const char *fmt, ...);
void console_vprintf(const char *fmt, va_list ap);

#endif /* CONSOLE_H */
