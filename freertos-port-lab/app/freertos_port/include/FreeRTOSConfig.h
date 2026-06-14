#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Step 18: FreeRTOSConfig for ESP32-S3 single-core, call0 ABI.
 * 抢占调度 + hooks（stack overflow / malloc failed / idle hook）。
 */

/* ---- Scheduling ---- */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      240000000UL
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    8
#define configMINIMAL_STACK_SIZE                256
#define configMAX_TASK_NAME_LEN                 16

/* Tick type: 32-bit */
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS

#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       0
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           0
#define configQUEUE_REGISTRY_SIZE               0

/* ---- Memory ---- */
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0
#define configTOTAL_HEAP_SIZE                   32768

/* ---- Stack overflow ---- */
#define configCHECK_FOR_STACK_OVERFLOW          2   /* 2=最严格：每次上下文切换检查 */
#define configSTACK_DEPTH_TYPE                  uint16_t

/* ---- Hooks ---- */
#define configUSE_IDLE_HOOK                     1   /* 空闲任务 hook，用于 heap 监控 */
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            1   /* pvPortMalloc 失败时触发 */

/* ---- Runtime stats ---- */
#define configGENERATE_RUN_TIME_STATS           0

/* ---- Software timers ---- */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               4
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            512

/* ---- API functions ---- */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_uxTaskPriorityGet               0
#define INCLUDE_vTaskSuspend                    0
#define INCLUDE_uxTaskGetStackHighWaterMark      1   /* 栈使用率监控 */
#define INCLUDE_xTaskGetIdleTaskHandle            1

/* ---- Assert ---- */
#define configASSERT( x )                       do { if( ( x ) == 0 ) { for( ;; ); } } while( 0 )

/* ---- Optional includes (none for now) ---- */

#endif /* FREERTOS_CONFIG_H */
