/* ESP32-S3 FreeRTOS port: call0 unified-context context switch.
 *
 * ALL saves (vPortYield AND ISR) use the same 96-byte frame:
 *   0x00 a0      0x04 a2     ... 0x38 a15
 *   0x3C SAR     0x40 LBEG   0x44 LEND   0x48 LCOUNT
 *   0x4C EPC_2   0x50 EPS_2
 *
 * vPortYield fills EPC_2=a0(return-addr) and EPS_2=PS(current) so
 * that both yield-initiated and ISR-initiated context switches can
 * restore through the same rfi-2 path.  No frame-format conversion.
 */

.section .iram.text, "ax"
.align 4

.extern pxCurrentTCB
.extern vTaskSwitchContext

#define FRAME   0x60   /* 96 bytes */

/* ================================================================
 * vPortYield — task-level yield
 * ================================================================ */
.global vPortYield
.type   vPortYield, @function
vPortYield:

    /* ---- 1. save full frame on current stack ---- */
    addi    a1, a1, -FRAME

    s32i    a0,  a1, 0x00
    s32i    a2,  a1, 0x04
    s32i    a3,  a1, 0x08
    s32i    a4,  a1, 0x0C
    s32i    a5,  a1, 0x10
    s32i    a6,  a1, 0x14
    s32i    a7,  a1, 0x18
    s32i    a8,  a1, 0x1C
    s32i    a9,  a1, 0x20
    s32i    a10, a1, 0x24
    s32i    a11, a1, 0x28
    s32i    a12, a1, 0x2C
    s32i    a13, a1, 0x30
    s32i    a14, a1, 0x34
    s32i    a15, a1, 0x38

    rsr     a3, SAR;    s32i a3, a1, 0x3C
    rsr     a3, LBEG;   s32i a3, a1, 0x40
    rsr     a3, LEND;   s32i a3, a1, 0x44
    rsr     a3, LCOUNT; s32i a3, a1, 0x48

    /* EPC_2 = a0 (return address for rfi), EPS_2 = PS */
    s32i    a0, a1, 0x4C        /* EPC_2 */
    rsr     a3, PS
    s32i    a3, a1, 0x50        /* EPS_2 */

    /* ---- 2. sp -> current TCB ---- */
    movi    a3, pxCurrentTCB
    l32i    a3, a3, 0
    s32i    a1, a3, 0           /* TCB->pxTopOfStack = sp */

    /* ---- 3. vTaskSwitchContext (critical) ---- */
    rsil    a4, 3
    call0   vTaskSwitchContext
    rsil    a4, 1               /* keep INTLEVEL elevated during restore */

    /* ---- 4. load new task's sp ---- */
    movi    a3, pxCurrentTCB
    l32i    a3, a3, 0
    l32i    a1, a3, 0

    /* ---- 5. restore from new frame (rfi-2 path, same as ISR) ---- */
    /* special regs */
    l32i    a3, a1, 0x50; wsr a3, 194    /* EPS_2 */
    l32i    a3, a1, 0x4C; wsr a3, 178    /* EPC_2 */
    l32i    a3, a1, 0x3C; wsr a3, SAR
    l32i    a3, a1, 0x40; wsr a3, LBEG
    l32i    a3, a1, 0x44; wsr a3, LEND
    l32i    a3, a1, 0x48; wsr a3, LCOUNT

    /* general regs */
    l32i    a15,a1,0x38; l32i a14,a1,0x34; l32i a13,a1,0x30; l32i a12,a1,0x2C
    l32i    a11,a1,0x28; l32i a10,a1,0x24; l32i a9, a1,0x20; l32i a8, a1,0x1C
    l32i    a7, a1,0x18; l32i a6, a1,0x14; l32i a5, a1,0x10; l32i a4, a1,0x0C
    l32i    a3, a1,0x08; l32i a2, a1,0x04; l32i a0, a1,0x00

    addi    a1, a1, FRAME
    rsync
    rfi     2

/* ================================================================
 * xPortStartFirstTask — start first task through rfi-2
 * ================================================================ */
.global xPortStartFirstTask
.type   xPortStartFirstTask, @function
xPortStartFirstTask:
    movi    a3, pxCurrentTCB
    l32i    a3, a3, 0
    l32i    a1, a3, 0

    l32i    a3, a1,0x50; wsr a3, 194
    l32i    a3, a1,0x4C; wsr a3, 178
    l32i    a3, a1,0x3C; wsr a3, SAR
    l32i    a3, a1,0x40; wsr a3, LBEG
    l32i    a3, a1,0x44; wsr a3, LEND
    l32i    a3, a1,0x48; wsr a3, LCOUNT

    l32i    a15,a1,0x38; l32i a14,a1,0x34; l32i a13,a1,0x30; l32i a12,a1,0x2C
    l32i    a11,a1,0x28; l32i a10,a1,0x24; l32i a9, a1,0x20; l32i a8, a1,0x1C
    l32i    a7, a1,0x18; l32i a6, a1,0x14; l32i a5, a1,0x10; l32i a4, a1,0x0C
    l32i    a3, a1,0x08; l32i a2, a1,0x04; l32i a0, a1,0x00

    addi    a1, a1, FRAME
    rsync
    rfi     2
