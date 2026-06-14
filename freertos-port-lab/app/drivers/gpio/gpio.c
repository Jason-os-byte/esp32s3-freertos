#include "gpio.h"

#define GPIO_BASE           0x60004000U
#define GPIO_ENABLE_REG     ( GPIO_BASE + 0x20U )
#define GPIO_OUT_REG        ( GPIO_BASE + 0x04U )
#define GPIO_OUT_W1TS_REG   ( GPIO_BASE + 0x08U )
#define GPIO_OUT_W1TC_REG   ( GPIO_BASE + 0x0CU )
#define GPIO_IN_REG         ( GPIO_BASE + 0x3CU )

#define REG_RD( a )  ( *( volatile uint32_t * )( a ) )
#define REG_WR( a,v ) ( *( volatile uint32_t * )( a ) = ( v ) )

void gpio_set_output( uint32_t gpio_num )
{
    uint32_t m = 1U << gpio_num;
    REG_WR( GPIO_ENABLE_REG, REG_RD( GPIO_ENABLE_REG ) | m );
}

void gpio_set_high( uint32_t gpio_num )
{
    REG_WR( GPIO_OUT_W1TS_REG, 1U << gpio_num );
}

void gpio_set_low( uint32_t gpio_num )
{
    REG_WR( GPIO_OUT_W1TC_REG, 1U << gpio_num );
}

void gpio_toggle( uint32_t gpio_num )
{
    uint32_t m = 1U << gpio_num;
    if ( REG_RD( GPIO_OUT_REG ) & m )
        REG_WR( GPIO_OUT_W1TC_REG, m );
    else
        REG_WR( GPIO_OUT_W1TS_REG, m );
}

uint32_t gpio_read( uint32_t gpio_num )
{
    return ( REG_RD( GPIO_IN_REG ) >> gpio_num ) & 1U;
}
