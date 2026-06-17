#include "uart1.h"

#define REG_RD( a )   ( *( volatile uint32_t * )( a ) )
#define REG_WR( a,v ) ( *( volatile uint32_t * )( a ) = ( v ) )

/* ---- GPIO Matrix 寄存器 ----
 * ★ 关键：OUT_SEL 和 IN_SEL 的索引方式不同！
 *
 * GPIO_FUNCn_OUT_SEL_CFG ：按 **GPIO 号** 索引（n = GPIO号）
 *   写入值 = 外设输出信号 ID | OEN_SEL(bit10)
 *   语义：把"外设信号"的输出路由到"GPIO n"
 *   例：GPIO_FUNC17_OUT_SEL = 15 | (1<<10) → U1TXD 信号输出到 GPIO17
 *
 * GPIO_FUNCn_IN_SEL_CFG  ：按 **外设信号 ID** 索引（n = 信号ID）！
 *   写入值 = GPIO 号 | SIG_IN_SEL(bit7)
 *   语义：把"GPIO 号"的物理输入送给"外设信号 n"
 *   例：GPIO_FUNC15_IN_SEL  = 18 | (1<<7) → GPIO18 输入给 U1RXD
 *
 * 来源：ESP-IDF gpio_ll.h → gpio_matrix_in() / gpio_matrix_out()
 */
#define GPIO_BASE                      0x60004000UL
#define GPIO_FUNC_OUT_SEL( gpio_num )  ( GPIO_BASE + 0x554U + (gpio_num) * 4U )
#define GPIO_FUNC_IN_SEL( signal_id )  ( GPIO_BASE + 0x154U + (signal_id) * 4U )

#define U1TXD_OUT_IDX  15
#define U1RXD_IN_IDX   15

/* ---- IO_MUX 寄存器 ---- */
#define IO_MUX_REG( n )  ( 0x60009000UL + 0x04U + (n) * 4U )

/* ---- SYSTEM 总线时钟 ---- */
#define SYSTEM_PERIP_CLK_EN0_REG  0x600C0018UL
#define SYSTEM_UART1_CLK_EN       ( 1UL << 5 )
#define SYSTEM_PERIP_RST_EN0_REG  0x600C0020UL
#define SYSTEM_UART1_RST          ( 1UL << 5 )

/* ---- UART1 寄存器 ---- */
#define UART1_BASE          0x60010000UL
#define UART_FIFO( i )      ( (i) + 0x00U )
#define UART_INT_CLR( i )   ( (i) + 0x10U )
#define UART_CLKDIV( i )    ( (i) + 0x14U )
#define UART_STATUS( i )    ( (i) + 0x1CU )
#define UART_CONF0( i )     ( (i) + 0x20U )

#define UART_RXFIFO_CNT_M   0x000003FFU
#define UART_RXFIFO_CNT_S   0

void uart1_init( uint32_t baud_rate )
{
    uint32_t base = UART1_BASE;

    /* ---- 1. 开 UART1 总线时钟 + 释放复位 ---- */
    REG_WR( SYSTEM_PERIP_CLK_EN0_REG,
            REG_RD( SYSTEM_PERIP_CLK_EN0_REG ) | SYSTEM_UART1_CLK_EN );
    REG_WR( SYSTEM_PERIP_RST_EN0_REG,
            REG_RD( SYSTEM_PERIP_RST_EN0_REG ) | SYSTEM_UART1_RST );
    REG_WR( SYSTEM_PERIP_RST_EN0_REG,
            REG_RD( SYSTEM_PERIP_RST_EN0_REG ) & ~SYSTEM_UART1_RST );

    /* ---- 2. GPIO Matrix + 手动方向控制 ---- */
    REG_WR( GPIO_FUNC_OUT_SEL( 4 ), 15 );               /* U1TXD(15)→GPIO4, OEN_SEL=0 */
    REG_WR( GPIO_FUNC_IN_SEL( 15 ), 5 | (1U << 7) );   /* GPIO5→U1RXD(15), SIG_IN_SEL=1 */
    #define GPIO_ENABLE_REG  0x60004020UL
    REG_WR( GPIO_ENABLE_REG, REG_RD( GPIO_ENABLE_REG ) | ( 1U << 4 ) );     /* GPIO4=输出 */
    REG_WR( GPIO_ENABLE_REG, REG_RD( GPIO_ENABLE_REG ) & ~( 1U << 5 ) );    /* GPIO5=输入 */

    /* ---- 3. IO_MUX ---- */
    REG_WR( IO_MUX_REG( 4 ), ( 1U << 10 ) | ( 3U << 2 ) );                   /* TX */
    REG_WR( IO_MUX_REG( 5 ), ( 1U << 10 ) | ( 1U << 9 ) | ( 3U << 2 ) );     /* RX */

    /* ---- 4. 波特率 ---- */
    uint32_t clkdiv = 80000000UL / ( baud_rate * 16UL );
    uint32_t rem    = 80000000UL % ( baud_rate * 16UL );
    uint32_t frag   = ( rem * 16UL + baud_rate * 8UL ) / ( baud_rate * 16UL );
    if ( frag >= 16UL ) { frag = 15; }
    REG_WR( UART_CLKDIV( base ), ( clkdiv & 0xFFFU ) | ( ( frag & 0xFU ) << 20 ) );

    /* ---- 5. CONF0: 8N1 ---- */
    REG_WR( UART_CONF0( base ), 0x00000003U );

    /* ---- 6. 清中断 ---- */
    REG_WR( UART_INT_CLR( base ), 0xFFFFFFFFU );
}

void uart1_putc( char c )
{
    REG_WR( UART_FIFO( UART1_BASE ), ( uint32_t )( uint8_t )c );
}

void uart1_puts( const char *s )
{
    while ( *s ) {
        if ( *s == '\n' ) uart1_putc( '\r' );
        uart1_putc( *s++ );
    }
}

int uart1_rx_available( void )
{
    uint32_t status = REG_RD( UART_STATUS( UART1_BASE ) );
    return ( status & UART_RXFIFO_CNT_M ) >> UART_RXFIFO_CNT_S;
}

int uart1_getc( void )
{
    if ( uart1_rx_available() == 0 ) return -1;
    return ( int )( REG_RD( UART_FIFO( UART1_BASE ) ) & 0xFFU );
}
