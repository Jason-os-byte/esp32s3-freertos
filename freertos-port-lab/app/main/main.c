#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "gpio.h"
#include "adc.h"
#include "console.h"

static char banner[] =
    "\r\n"
    "==================================\r\n"
    " [TEST] ADC1 driver (bare-metal)\r\n"
    "   GPIO4=CH3, 12bit, 12dB atten\r\n"
    "   Full REGI2C init + calibration\r\n"
    "==================================\r\n";
static char crlf[] = "\r\n";

static void print_uint(uint32_t v) {
    char b[10]; int i=0;
    if(v==0){console_putc('0');return;}
    while(v&&i<10){b[i++]=(char)('0'+v%10);v/=10;}while(i>0)console_putc(b[--i]);
}

static void print_hex(uint32_t v) {
    console_putc('0'); console_putc('x');
    for(int s=28;s>=0;s-=4){
        uint8_t n=(v>>s)&0xF;
        console_putc((char)(n<10?'0'+n:'a'+n-10));
    }
}

void vApplicationStackOverflowHook(TaskHandle_t x,char*n){(void)x;for(;;){}}
void vApplicationMallocFailedHook(void){for(;;){}}
void vApplicationIdleHook(void){}
TaskHandle_t g_notify_task_handle;
static void notify_task(void*p){(void)p;for(;;)ulTaskNotifyTake(pdTRUE,portMAX_DELAY);}

static void adc_test_task( void * p ) {
    (void)p;

    /* 诊断：启动前关键寄存器快照 */
    uint32_t sb = 0x60008800UL;
    console_puts( "[adc] SENS register snapshot:\r\n" );
    console_puts( "  SENS_MEAS1_CTRL2  =" ); print_hex(*(volatile uint32_t*)(sb+0x0C)); console_puts(crlf);
    console_puts( "  SENS_SAR_ATTEN1   =" ); print_hex(*(volatile uint32_t*)(sb+0x14)); console_puts(crlf);
    console_puts( "  SENS_SAR_POWER_XPD=" ); print_hex(*(volatile uint32_t*)(sb+0x3C)); console_puts(crlf);
    console_puts( "  SENS_SLAVE_ADDR1  =" ); print_hex(*(volatile uint32_t*)(sb+0x40)); console_puts(crlf);
    console_puts( "  SENS_CLK_GATE_CONF=" ); print_hex(*(volatile uint32_t*)(sb+0x104)); console_puts(crlf);
    console_puts( "  ANA_CONFIG_REG    =" ); print_hex(*(volatile uint32_t*)0x6000E044UL); console_puts(crlf);
    console_puts( "  ANA_CONFIG2_REG   =" ); print_hex(*(volatile uint32_t*)0x6000E048UL); console_puts(crlf);

    /* 初始化 */
    adc1_init();
    console_puts( "[adc] init done\r\n" );

    /* 诊断：初始化后关键寄存器快照 */
    console_puts( "[adc] Post-init snapshot:\r\n" );
    console_puts( "  SENS_MEAS1_CTRL2  =" ); print_hex(*(volatile uint32_t*)(sb+0x0C)); console_puts(crlf);
    console_puts( "  SENS_SAR_POWER_XPD=" ); print_hex(*(volatile uint32_t*)(sb+0x3C)); console_puts(crlf);
    console_puts( "  SENS_SLAVE_ADDR1  =" ); print_hex(*(volatile uint32_t*)(sb+0x40)); console_puts(crlf);
    console_puts( "  SENS_CLK_GATE_CONF=" ); print_hex(*(volatile uint32_t*)(sb+0x104)); console_puts(crlf);
    console_puts( "  SAR_I2C_CTRL  =" ); print_hex(*(volatile uint32_t*)(sb+0x58)); console_puts(crlf);

    /* 预热 */
    for ( int k = 0; k < 5; k++ ) { adc1_read( 0 ); }

    console_puts( "[adc] Scanning CH3~CH7 (GPIO4~GPIO8):\r\n" );
    for ( ;; ) {
        console_puts( "  " );
        for ( int ch = 3; ch <= 7; ch++ ) {
            uint32_t sum = 0;
            for ( int k = 0; k < 8; k++ ) sum += adc1_read( ch );
            uint32_t raw = sum / 8;
            console_puts( "CH" ); print_uint( ch );
            console_puts( "=" ); print_uint( adc1_to_mv( raw ) );
            console_puts( "mV  " );
            vTaskDelay( pdMS_TO_TICKS( 5 ) );
        }
        console_puts( crlf );
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    }
}

/* GPIO48 闪烁证明系统活着 */
static void gpio_blink_task( void * p ) {
    (void)p; gpio_set_output(48);
    for(;;){ gpio_toggle(48); vTaskDelay(pdMS_TO_TICKS(500)); }
}

int main(void){
    volatile uint32_t d=0;for(uint32_t i=0;i<50000000;i++)d=i;(void)d;
    console_puts(banner);

    xTaskCreate(adc_test_task,  "adc_test",   640,NULL,2,NULL);
    xTaskCreate(gpio_blink_task,"gpio_blink", 256,NULL,1,NULL);
    xTaskCreate(notify_task,    "notify",     256,NULL,3,&g_notify_task_handle);

    vTaskStartScheduler();
    for(;;){} return 0;
}
