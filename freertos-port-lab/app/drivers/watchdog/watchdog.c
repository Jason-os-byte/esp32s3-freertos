#include <stdint.h>
#include "watchdog.h"

/* 寄存器地址来自 ESP-IDF v5.5.4
 * components/soc/esp32s3/register/soc/{rtc_cntl_reg.h,timer_group_reg.h}
 * 与 reg_base.h（DR_REG_RTCCNTL_BASE = 0x60008000，
 *                  DR_REG_TIMERGROUP0_BASE = 0x6001F000，
 *                  DR_REG_TIMERGROUP1_BASE = 0x60020000）。
 */
#define REG32(addr) (*(volatile uint32_t *)(addr))

/* RTC RWDT */
#define RTC_CNTL_WDTCONFIG0_REG   0x60008098UL
#define RTC_CNTL_WDTWPROTECT_REG  0x600080B0UL
#define RTC_CNTL_WDT_WKEY         0x50D83AA1UL  /* 默认 unlock key */

/* Super Watchdog (SWD) */
#define RTC_CNTL_SWD_CONF_REG     0x600080B4UL
#define RTC_CNTL_SWD_WPROTECT_REG 0x600080B8UL
#define RTC_CNTL_SWD_WKEY         0x8F1D312AUL
#define RTC_CNTL_SWD_AUTO_FEED_EN (1UL << 31)
#define RTC_CNTL_SWD_DISABLE      (1UL << 30)
#define RTC_CNTL_SWD_FEED         (1UL << 29)

/* Timer Group WDT */
#define TIMG_WDTCONFIG0_REG(i)    (((i) == 0 ? 0x6001F000UL : 0x60020000UL) + 0x48)
#define TIMG_WDTFEED_REG(i)       (((i) == 0 ? 0x6001F000UL : 0x60020000UL) + 0x60)
#define TIMG_WDTWPROTECT_REG(i)   (((i) == 0 ? 0x6001F000UL : 0x60020000UL) + 0x64)
#define TIMG_WDT_WKEY_VALUE       0x50D83AA1UL

static inline void rtc_wdt_disable(void)
{
    REG32(RTC_CNTL_WDTWPROTECT_REG) = RTC_CNTL_WDT_WKEY;
    REG32(RTC_CNTL_WDTCONFIG0_REG) = 0;        /* 清掉 WDT_EN(bit31)，整段 stage 清 0 */
    REG32(RTC_CNTL_WDTWPROTECT_REG) = 0;       /* 上锁 */
}

static inline void rtc_swd_disable(void)
{
    REG32(RTC_CNTL_SWD_WPROTECT_REG) = RTC_CNTL_SWD_WKEY;
    uint32_t v = REG32(RTC_CNTL_SWD_CONF_REG);
    v |= RTC_CNTL_SWD_DISABLE;                 /* 关闭 SWD */
    v |= RTC_CNTL_SWD_FEED;                    /* 顺便先喂一下，避免临界点 */
    v &= ~RTC_CNTL_SWD_AUTO_FEED_EN;           /* 不让中断喂狗 */
    REG32(RTC_CNTL_SWD_CONF_REG) = v;
    REG32(RTC_CNTL_SWD_WPROTECT_REG) = 0;
}

static inline void timg_wdt_disable(int i)
{
    REG32(TIMG_WDTWPROTECT_REG(i)) = TIMG_WDT_WKEY_VALUE;
    REG32(TIMG_WDTCONFIG0_REG(i)) = 0;         /* WDT_EN 在 bit31，整段清 0 即可 */
    REG32(TIMG_WDTFEED_REG(i)) = 1;
    REG32(TIMG_WDTWPROTECT_REG(i)) = 0;
}

void watchdog_disable_all(void)
{
    rtc_wdt_disable();
    rtc_swd_disable();
    timg_wdt_disable(0);
    timg_wdt_disable(1);
}
