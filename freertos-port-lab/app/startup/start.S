/* ESP32-S3 startup for freertos-port-lab M2
 *
 * bootloader 跳到 _start 后：
 *   1. 关中断
 *   2. 设置栈
 *   3. 关闭 bootloader/ROM 留下的 watchdog
 *   4. 跳到 crt0_main
 *
 * 注意：现阶段所有 .text 暂时放 IRAM，避免早期 IROM/cache 细节干扰。
 */

.section .text.startup, "ax"
.global _start
_start:
    rsil    a0, 15
    movi    a1, _stack_top
    movi    a0, 0
    wsr     a0, ps
    wsr     a0, intenable
    wsr     a0, interrupt
    rsync

    /* 设置 Xtensa relocatable vector base。必须在打开任何 CPU interrupt 前完成。 */
    movi    a0, _vector_base
    wsr     a0, vecbase
    rsync

    /* ---- 关闭 RTC RWDT ---- */
    movi    a2, 0x50D83AA1
    movi    a3, 0x600080B0       /* RTC_CNTL_WDTWPROTECT_REG */
    s32i    a2, a3, 0
    memw
    movi    a2, 0
    movi    a3, 0x60008098       /* RTC_CNTL_WDTCONFIG0_REG */
    s32i    a2, a3, 0
    memw
    movi    a3, 0x600080B0
    s32i    a2, a3, 0
    memw

    /* ---- 关闭 Super-WDT ---- */
    movi    a2, 0x8F1D312A
    movi    a3, 0x600080B8       /* RTC_CNTL_SWD_WPROTECT_REG */
    s32i    a2, a3, 0
    memw
    movi    a3, 0x600080B4       /* RTC_CNTL_SWD_CONF_REG */
    l32i    a4, a3, 0
    movi    a5, 0x60000000       /* SWD_DISABLE | SWD_FEED */
    or      a4, a4, a5
    movi    a5, 0x7FFFFFFF       /* clear AUTO_FEED_EN */
    and     a4, a4, a5
    s32i    a4, a3, 0
    memw
    movi    a2, 0
    movi    a3, 0x600080B8
    s32i    a2, a3, 0
    memw

    /* ---- 关闭 TIMG0 WDT ---- */
    movi    a2, 0x50D83AA1
    movi    a3, 0x6001F064       /* TIMG_WDTWPROTECT_REG(0) */
    s32i    a2, a3, 0
    memw
    movi    a2, 0
    movi    a3, 0x6001F048       /* TIMG_WDTCONFIG0_REG(0) */
    s32i    a2, a3, 0
    memw
    movi    a3, 0x6001F064
    s32i    a2, a3, 0
    memw

    /* ---- 关闭 TIMG1 WDT ---- */
    movi    a2, 0x50D83AA1
    movi    a3, 0x60020064       /* TIMG_WDTWPROTECT_REG(1) */
    s32i    a2, a3, 0
    memw
    movi    a2, 0
    movi    a3, 0x60020048       /* TIMG_WDTCONFIG0_REG(1) */
    s32i    a2, a3, 0
    memw
    movi    a3, 0x60020064
    s32i    a2, a3, 0
    memw

    call0   crt0_main

.Lhang:
    j       .Lhang
