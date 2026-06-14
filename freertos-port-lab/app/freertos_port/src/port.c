#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "systimer.h"

/* Step 17: real port layer with ISR-level yield support. */

/* ISR yield flag: set by portYIELD_FROM_ISR, checked by ISR wrapper. */
volatile BaseType_t xPortYieldFromISR = pdFALSE;

/* ---- 上下文帧偏移量（与 portasm.S 一致） ---- */
#define OFFSET_A0      0x00
#define OFFSET_A2      0x04
#define OFFSET_PS      0x4C
#define PORT_CONTEXT   0x50

/* ---- 初始 PS 值：UM=1 (bit5), INTLEVEL=0 ---- */
#define INITIAL_PS     ( 1UL << 5 )

/* ================================================================
 * pxPortInitialiseStack — 96-byte unified frame for rfi-2 restore.
 *
 * Frame (96 bytes, stack grows down, fill from high to low):
 *   0x50 EPS_2=INITIAL_PS  0x4C EPC_2=pxCode
 *   0x48 LCOUNT=0          0x44 LEND=0
 *   0x40 LBEG=0            0x3C SAR=0
 *   0x38..0x0C a15..a3=0
 *   0x08 a3=0              0x04 a2=pvParameters   0x00 a0=pxCode
 * ================================================================ */
StackType_t * pxPortInitialiseStack( StackType_t * pxTopOfStack,
                                     TaskFunction_t pxCode,
                                     void * pvParameters )
{
    int i;

    pxTopOfStack = ( StackType_t * )( ( ( uint32_t ) pxTopOfStack ) & ~15U );

    /* from high address to low */
    *pxTopOfStack = INITIAL_PS;                /* 0x50 EPS_2 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) pxCode;    /* 0x4C EPC_2 */
    pxTopOfStack--;
    *pxTopOfStack = 0;  pxTopOfStack--;        /* 0x48 LCOUNT */
    *pxTopOfStack = 0;  pxTopOfStack--;        /* 0x44 LEND */
    *pxTopOfStack = 0;  pxTopOfStack--;        /* 0x40 LBEG */
    *pxTopOfStack = 0;  pxTopOfStack--;        /* 0x3C SAR */

    for ( i = 15; i >= 3; i-- ) {
        *pxTopOfStack = 0; pxTopOfStack--;     /* a15..a3 */
    }

    *pxTopOfStack = ( StackType_t ) pvParameters;  /* 0x04 a2 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) pxCode;         /* 0x00 a0 */

    return pxTopOfStack;  /* points to a0 (frame start) */
}

/* ================================================================
 * xPortStartScheduler
 *
 * 由 vTaskStartScheduler 调用。此时：
 *   - idle task 已创建，pxCurrentTCB 指向它
 *   - 如有用户任务，已在就绪列表中
 *
 * 工作：调用 xPortStartFirstTask (portasm.S) 恢复第一个任务的上下文。
 * 这个函数永远不会返回。
 * ================================================================ */
BaseType_t xPortStartScheduler( void )
{
    /* 在第一个任务运行之前启动系统 tick。
     * 不能早于这里：vTaskStartScheduler 此时已完成
     * prvInitialiseTaskLists()（初始化延迟列表等），
     * 否则 tick ISR 会在列表为 NULL 时崩溃。 */
    systimer_irq_init( 1000UL );

    /* 跳入第一个任务；从此不再返回 */
    xPortStartFirstTask();

    /* 不应到达 */
    return pdTRUE;
}

/* ================================================================
 * vPortEndScheduler
 * ================================================================ */
void vPortEndScheduler( void )
{
    for( ;; ) { }
}
