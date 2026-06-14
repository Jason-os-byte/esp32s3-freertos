#ifndef WATCHDOG_H
#define WATCHDOG_H

/* 关闭 ESP32-S3 上 bootloader 离开时仍处于使能状态的全部看门狗：
 *   - RTC RWDT
 *   - Super Watchdog (SWD)
 *   - Timer Group 0 / 1 主 WDT
 * 调用时机：start.S 跳到 C 入口（crt0_main）后立即调用，main() 之前。
 * 不调用 → 芯片几秒一次硬复位，串口/USB-JTAG 反复枚举（电脑滴答声）。
 */
void watchdog_disable_all(void);

#endif /* WATCHDOG_H */
