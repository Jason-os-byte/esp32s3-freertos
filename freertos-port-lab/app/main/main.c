#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "gpio.h"
#include "uart1.h"
#include "console.h"

static char banner[] =
    "\r\n"
    "==================================\r\n"
    " [TEST] UART1 loopback v4\r\n"
    "   GPIO4=TX, GPIO5=RX, 115200\r\n"
    "==================================\r\n";
static char crlf[] = "\r\n";

static void print_hex(uint32_t v) {
    for(int s=28;s>=0;s-=4){uint32_t d=(v>>s)&0xFUL;console_putc((char)(d<10?'0'+d:'a'+d-10));}
}

void vApplicationStackOverflowHook(TaskHandle_t x,char*n){(void)x;for(;;){}}
void vApplicationMallocFailedHook(void){for(;;){}}
void vApplicationIdleHook(void){}
TaskHandle_t g_notify_task_handle;
static void notify_task(void*p){(void)p;for(;;)ulTaskNotifyTake(pdTRUE,portMAX_DELAY);}

int main(void){
    volatile uint32_t d=0;for(uint32_t i=0;i<50000000;i++)d=i;(void)d;
    console_puts(banner);
    gpio_set_output(48);

    uart1_init( 115200 );

    uint32_t b = 0x60010000UL;
    console_puts("[uart1] diag:\r\n");
    console_puts("  OUT(4)="); print_hex(*(volatile uint32_t*)(0x60004000UL+0x554+4*4)); console_puts(crlf);
    console_puts("  IN(15)="); print_hex(*(volatile uint32_t*)(0x60004000UL+0x154+15*4)); console_puts(crlf);
    console_puts("  MUX4  ="); print_hex(*(volatile uint32_t*)0x60009014UL); console_puts(crlf);
    console_puts("  MUX5  ="); print_hex(*(volatile uint32_t*)0x60009018UL); console_puts(crlf);
    console_puts("  EN    ="); print_hex(*(volatile uint32_t*)0x60004020UL); console_puts(crlf);
    console_puts("  CLK   ="); print_hex(*(volatile uint32_t*)(b+0x14)); console_puts(crlf);

    /* 清缓冲 */
    while((*(volatile uint32_t*)(b+0x1CU)&0x3FF)!=0) (void)(*(volatile uint32_t*)(b+0x00U));

    /* 发 0x55, 0xAA, 'H' */
    uart1_putc(0x55); uart1_putc(0xAA); uart1_putc('H');
    for(volatile int i=0;i<50000000;i++){}

    console_puts("  STATUS="); print_hex(*(volatile uint32_t*)(b+0x1C)); console_puts(crlf);
    console_puts("  rx: ");
    int got=0;
    while((*(volatile uint32_t*)(b+0x1CU)&0x3FF)!=0){
        char c2=(char)(*(volatile uint32_t*)(b+0x00U)&0xFF);
        if(c2>=0x20&&c2<=0x7E) console_putc(c2);
        else { console_putc('<');
            console_putc("0123456789ABCDEF"[(c2>>4)&0xF]);
            console_putc("0123456789ABCDEF"[c2&0xF]);
            console_putc('>');
        }
        got++;
    }
    console_puts(" ("); {char bb[4];bb[0]='0'+got/10;bb[1]='0'+got%10;bb[2]=0;console_puts(bb);}
    console_puts(" bytes)\r\n");

    gpio_set_output(48);
    xTaskCreate(notify_task,"notify",256,NULL,2,&g_notify_task_handle);
    vTaskStartScheduler();
    for(;;){} return 0;
}
