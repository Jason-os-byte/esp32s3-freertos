#ifndef PORTMACRO_H
#define PORTMACRO_H

/* portmacro.h for ESP32-S3, call0 ABI. Step 13: real port layer. */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ---- Types ---- */
#define portCHAR        char
#define portSHORT       short
#define portLONG        long

typedef uint32_t        StackType_t;
typedef int32_t         BaseType_t;
typedef uint32_t        UBaseType_t;

#if ( configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_32_BITS )
    typedef uint32_t    TickType_t;
    #define portMAX_DELAY              ( TickType_t ) 0xffffffffUL
#else
    typedef uint16_t    TickType_t;
    #define portMAX_DELAY              ( TickType_t ) 0xffffU
#endif

/* ---- Architecture ---- */
#define portSTACK_GROWTH                ( -1 )
#define portBYTE_ALIGNMENT              16
#define portTICK_PERIOD_MS              ( ( TickType_t ) 1000 / configTICK_RATE_HZ )
#define portNOP()                       __asm__ volatile ( "nop" )
#define portHAS_STACK_OVERFLOW_CHECKING 0
#define portUSING_MPU_WRAPPERS          0

/* ---- Context frame (call0 ABI, 96 bytes = 0x60, unified ISR format) ----
 * All context saves (vPortYield AND ISR) use the same 96-byte frame.
 * vPortYield saves EPC_2=a0 (return addr) and EPS_2=PS (current PS)
 * at offsets 0x4C/0x50 so rfi 2 works for both paths. No conversion.
 * ---- */
#define portCONTEXT_SIZE                0x60

/* ---- Critical sections (simple, no TCB nesting for step 13) ---- */
#define portCRITICAL_NESTING_IN_TCB     0

#define portENTER_CRITICAL() \
    do { \
        uint32_t __ps; \
        __asm__ volatile ( "rsil %0, 3" : "=a"(__ps) :: "memory" ); \
        ( void ) __ps; \
    } while( 0 )

#define portEXIT_CRITICAL() \
    do { \
        uint32_t __ps; \
        __asm__ volatile ( "rsil %0, 0" : "=a"(__ps) :: "memory" ); \
        ( void ) __ps; \
    } while( 0 )

#define portDISABLE_INTERRUPTS()  portENTER_CRITICAL()
#define portENABLE_INTERRUPTS()   portEXIT_CRITICAL()

/* ---- Yield ---- */
extern void vPortYield( void );
#define portYIELD()                     vPortYield()
extern volatile BaseType_t xPortYieldFromISR;
#define portYIELD_FROM_ISR( x )         do { if( ( x ) != pdFALSE ) xPortYieldFromISR = pdTRUE; } while( 0 )

/* ---- Scheduler functions (in portasm.S and port.c) ---- */
#define portTASK_FUNCTION_PROTO( vFunction, pvParameters )  void vFunction( void * pvParameters )
#define portTASK_FUNCTION( vFunction, pvParameters )        void vFunction( void * pvParameters )

extern void xPortStartFirstTask( void );

/* ---- Single core ---- */
#define portGET_CORE_ID()               0

/* ---- TCB hooks (stub) ---- */
#define portSETUP_TCB( x )              ( void )( x )
#define portCLEAN_UP_TCB( x )           ( void )( x )
#define portPRE_TASK_DELETE_HOOK( pxTCB, pxYieldPending )   ( void )( pxTCB )

/* ---- Priority helpers (stub for step 13) ---- */
#define portRECORD_READY_PRIORITY( prio, uxTopPriority )  ( void )( prio )
#define portRESET_READY_PRIORITY( prio )                  ( void )( prio )
#define portGET_HIGHEST_PRIORITY( prio, uxTopPriority )   ( void )( prio )

/* ---- Assert / validate in ISR ---- */
#define portASSERT_IF_IN_ISR()          ( void )0
#define portASSERT_IF_INTERRUPT_PRIORITY_INVALID()  ( void )0

/* ---- ISR lock (stub for single core) ---- */
#define portGET_ISR_LOCK()              0
#define portRELEASE_ISR_LOCK()          ( void )0

/* ---- Interrupt mask from ISR (stub) ---- */
#define portSET_INTERRUPT_MASK_FROM_ISR()       0
#define portCLEAR_INTERRUPT_MASK_FROM_ISR( x )  ( void )( x )
#define portMEMORY_BARRIER()            __asm__ volatile ( "" ::: "memory" )

#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */
