/* Xtensa level-2 interrupt vector + ISR wrapper, with yield-from-ISR.
 *
 * ALL context frames are 96 bytes (unified with portasm.S):
 *   0x00 a0    0x04 a2    ... 0x38 a15
 *   0x3C SAR   0x40 LBEG  0x44 LEND  0x48 LCOUNT
 *   0x4C EPC_2  0x50 EPS_2
 *
 * Both vPortYield and ISR save this same format, so the ISR
 * yield path does NOT need frame conversion — just save sp,
 * vTaskSwitchContext, load new sp, and fall through to the
 * shared rfi-2 epilogue.
 */

.section .Level2InterruptVector.text, "ax"
.align 4
.global _Level2Vector
_Level2Vector:
    wsr     a0, 210              /* EXCSAVE_2 = 210 */
    call0   systimer_l2_isr_entry

.section .iram.text, "ax"
.align 4

.extern pxCurrentTCB
.extern vTaskSwitchContext
.extern xPortYieldFromISR

#define FRAME  96

.global systimer_l2_isr_entry
.type systimer_l2_isr_entry, @function
systimer_l2_isr_entry:
    addi    a1, a1, -FRAME

    /* save interrupted context */
    rsr     a0, 210;       s32i a0, a1, 0x00
    s32i    a2, a1,0x04;   s32i a3, a1,0x08;   s32i a4, a1,0x0C
    s32i    a5, a1,0x10;   s32i a6, a1,0x14;   s32i a7, a1,0x18
    s32i    a8, a1,0x1C;   s32i a9, a1,0x20;   s32i a10,a1,0x24
    s32i    a11,a1,0x28;   s32i a12,a1,0x2C;   s32i a13,a1,0x30
    s32i    a14,a1,0x34;   s32i a15,a1,0x38

    rsr     a2,SAR;    s32i a2,a1,0x3C
    rsr     a2,LBEG;   s32i a2,a1,0x40
    rsr     a2,LEND;   s32i a2,a1,0x44
    rsr     a2,LCOUNT; s32i a2,a1,0x48
    rsr     a2,178;    s32i a2,a1,0x4C   /* EPC_2 */
    rsr     a2,194;    s32i a2,a1,0x50   /* EPS_2 */

    /* C handler */
    call0   systimer_target0_isr

    /* ---- yield-from-ISR ---- */
    movi    a3, xPortYieldFromISR
    l32i    a3, a3, 0
    beqz    a3, .Lisr_epilogue

    /* clear flag, save sp to current TCB */
    movi    a3, 0
    movi    a4, xPortYieldFromISR
    s32i    a3, a4, 0

    movi    a3, pxCurrentTCB
    l32i    a3, a3, 0
    s32i    a1, a3, 0           /* TCB->pxTopOfStack = sp */

    /* switch */
    rsil    a4, 3
    call0   vTaskSwitchContext
    rsil    a4, 1

    /* load new sp (unified frame, no conversion needed) */
    movi    a3, pxCurrentTCB
    l32i    a3, a3, 0
    l32i    a1, a3, 0

    /* fall through to shared epilogue */

.Lisr_epilogue:
    l32i    a2,a1,0x50; wsr a2,194   /* EPS_2 */
    l32i    a2,a1,0x4C; wsr a2,178   /* EPC_2 */
    l32i    a2,a1,0x3C; wsr a2,SAR
    l32i    a2,a1,0x40; wsr a2,LBEG
    l32i    a2,a1,0x44; wsr a2,LEND
    l32i    a2,a1,0x48; wsr a2,LCOUNT

    l32i    a3, a1,0x08; l32i a4, a1,0x0C; l32i a5, a1,0x10
    l32i    a6, a1,0x14; l32i a7, a1,0x18; l32i a8, a1,0x1C
    l32i    a9, a1,0x20; l32i a10,a1,0x24; l32i a11,a1,0x28
    l32i    a12,a1,0x2C; l32i a13,a1,0x30; l32i a14,a1,0x34; l32i a15,a1,0x38

    l32i    a0, a1,0x00
    l32i    a2, a1,0x04
    addi    a1, a1, FRAME
    rsync
    rfi     2
