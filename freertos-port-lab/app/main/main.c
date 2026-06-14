#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "gpio.h"
#include "console.h"

/* ================================================================
 * BSP Layer Step 1: GPIO + FreeRTOS 任务
 *
 * 硬件抽象层（HAL/BSP）的职责：
 *   对上层任务暴露简单 API（gpio_set_high / gpio_toggle / ...），
 *   封装 MMIO 寄存器地址和操作时序。
 *   上层任务不需要知道 GPIO_BASE=0x60004000。
 *
 * 本步验证：
 *   1. gpio driver 编译通过、正确读写寄存器
 *   2. FreeRTOS 任务能调用 BSP 函数控制硬件
 *   3. 栈溢出 hook + heap 监控仍然工作
 * ================================================================ */

/* ---- 配置：你的板子 LED 引脚 ----
 * ESP32-S3-DevKitC: 通常 GPIO 48 (或 38/47)。
 * 如果板上没 LED，改这个宏为任意空闲 GPIO，用万用表或逻辑分析仪验证。 */
#define LED_GPIO  48

/* ---- 字符串 ---- */
static char banner[] =
    "\r\n"
    "==================================\r\n"
    " [BSP] GPIO driver + FreeRTOS task\r\n"
    "   ESP32-S3, LED on GPIO48, 500ms\r\n"
    "==================================\r\n";
static char crlf[] = "\r\n";
static char sched_label[] = "[scheduler] starting...\r\n";
static char gpio_label[] = "[gpio_blink] LED toggle\r\n";

/* ---- Step10 ---- */
static uint32_t g_bss_value;
static uint32_t g_data_value = 0x12345678UL;
static void print_hex32(uint32_t v) {
    for(int s=28;s>=0;s-=4){uint32_t d=(v>>s)&0xFUL;console_putc((char)(d<10?'0'+d:'a'+d-10));}
}
static void runtime_check(void) {
    console_puts("\r\n[Step10] bss=0x");print_hex32(g_bss_value);
    console_puts(" data=0x");print_hex32(g_data_value);
    console_puts((g_bss_value==0&&g_data_value==0x12345678UL)?" [PASS]\r\n":" [FAIL]\r\n");
}

/* ---- GPIO blink 任务 ---- */
static void gpio_blink_task( void * p )
{
    (void)p;
    gpio_set_output( LED_GPIO );
    for ( ;; ) {
        gpio_toggle( LED_GPIO );
        console_puts( gpio_label );
        vTaskDelay( pdMS_TO_TICKS( 500 ) );
    }
}

/* ---- hooks（从 Step 18 保留）---- */
void vApplicationStackOverflowHook( TaskHandle_t xTask, char * pcTaskName )
{
    (void)xTask;
    console_puts("\r\n[HOOK] *** STACK OVERFLOW *** Task: ");
    console_puts(pcTaskName); console_puts(crlf);
    for(;;){}
}
void vApplicationMallocFailedHook( void ) {
    console_puts("\r\n[HOOK] *** MALLOC FAILED ***\r\n"); for(;;){}
}
static uint32_t g_idle_count;
void vApplicationIdleHook( void ) {
    g_idle_count++;
    if((g_idle_count&0x3FF)==0){console_puts("[idle] heap=");
    {uint32_t h=xPortGetFreeHeapSize();char b[10];int i=0;if(h==0)console_putc('0');
    while(h&&i<10){b[i++]=(char)('0'+h%10);h/=10;}while(i>0)console_putc(b[--i]);}
    console_puts(crlf);}
}

/* ---- ISR notify（保留，但简化）---- */
TaskHandle_t g_notify_task_handle;
static void notify_task( void * p ) {
    (void)p; for(;;){ulTaskNotifyTake(pdTRUE,portMAX_DELAY);}
}

/* ---- main ---- */
int main( void )
{
    volatile uint32_t d=0; for(uint32_t i=0;i<50000000;i++)d=i; (void)d;
    console_puts( banner );
    runtime_check();

    xTaskCreate( gpio_blink_task, "gpio_blink", 256, NULL, 1, NULL );
    xTaskCreate( notify_task,     "notify",     256, NULL, 2, &g_notify_task_handle );

    console_puts( sched_label );
    vTaskStartScheduler();
    for(;;){}
    return 0;
}
