/* 最小 USB-Serial/JTAG 控制台。
 *
 * ESP32-S3 USB-Serial/JTAG 寄存器：
 *   DR_REG_USB_SERIAL_JTAG_BASE = 0x60038000
 *   EP1_REG       0x00   写一字节进 IN FIFO
 *   EP1_CONF_REG  0x04   bit0 WR_DONE：写 1 后把 IN packet 推给 host
 *                        bit1 SERIAL_IN_EP_DATA_FREE：IN FIFO 可写
 *
 * 当前阶段约束：
 *   - monitor 已经打开，host 端在读 /dev/ttyACM0
 *   - 为了不丢字节，每次写之前等待 DATA_FREE
 *   - 如果 host 不在读，等待超时后丢弃本字节，避免 main 永久卡死
 */

#include <stdint.h>
#include "console.h"

#define REG32(addr)               (*(volatile uint32_t *)(addr))

#define USJ_EP1_REG               REG32(0x60038000UL)
#define USJ_EP1_CONF_REG          REG32(0x60038004UL)
#define USJ_WR_DONE               (1UL << 0)
#define USJ_IN_EP_DATA_FREE       (1UL << 1)

#define USJ_TX_TIMEOUT_LOOPS      5000000UL

static int console_wait_tx_free(void)
{
    uint32_t guard = USJ_TX_TIMEOUT_LOOPS;
    while (!(USJ_EP1_CONF_REG & USJ_IN_EP_DATA_FREE)) {
        if (--guard == 0) {
            return 0;
        }
    }
    return 1;
}

static void console_raw_putc(uint8_t c)
{
    if (!console_wait_tx_free()) {
        return;
    }

    USJ_EP1_REG = c;
    USJ_EP1_CONF_REG = USJ_WR_DONE;
}

void console_putc(char c)
{
    if (c == '\n') {
        console_raw_putc('\r');
    }
    console_raw_putc((uint8_t)c);
}

void console_puts(const char *s)
{
    while (*s) {
        console_putc(*s++);
    }
}

/* ---------- 极简 printf ---------- */

static void console_print_uint(uint32_t v, uint32_t base, int upper)
{
    char buf[12];
    int  i = 0;
    if (v == 0) {
        console_putc('0');
        return;
    }
    while (v && i < (int)sizeof(buf)) {
        uint32_t d = v % base;
        buf[i++] = (char)(d < 10 ? '0' + d : (upper ? 'A' : 'a') + d - 10);
        v /= base;
    }
    while (i--) {
        console_putc(buf[i]);
    }
}

static void console_print_int(int32_t v)
{
    if (v < 0) {
        console_putc('-');
        v = -v;
    }
    console_print_uint((uint32_t)v, 10, 0);
}

void console_vprintf(const char *fmt, va_list ap)
{
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            console_putc(*fmt);
            continue;
        }
        fmt++;
        switch (*fmt) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            console_puts(s ? s : "(null)");
            break;
        }
        case 'c':
            console_putc((char)va_arg(ap, int));
            break;
        case 'd':
        case 'i':
            console_print_int(va_arg(ap, int32_t));
            break;
        case 'u':
            console_print_uint(va_arg(ap, uint32_t), 10, 0);
            break;
        case 'x':
            console_print_uint(va_arg(ap, uint32_t), 16, 0);
            break;
        case 'X':
            console_print_uint(va_arg(ap, uint32_t), 16, 1);
            break;
        case 'p':
            console_puts("0x");
            console_print_uint((uint32_t)(uintptr_t)va_arg(ap, void *), 16, 0);
            break;
        case '%':
            console_putc('%');
            break;
        case '\0':
            return;
        default:
            console_putc('%');
            console_putc(*fmt);
            break;
        }
    }
}

void console_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    console_vprintf(fmt, ap);
    va_end(ap);
}
