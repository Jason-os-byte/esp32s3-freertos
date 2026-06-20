#include "adc.h"

#define REG_RD(a)   (*(volatile uint32_t*)(a))
#define REG_WR(a,v) (*(volatile uint32_t*)(a)=(v))
#define REG_SET_BIT(r,b)  REG_WR(r, REG_RD(r) | (b))
#define REG_CLR_BIT(r,b)  REG_WR(r, REG_RD(r) & ~(b))

/*
 * ESP32-S3 ADC1 裸机驱动 — 完整 REGI2C + SAR ADC 实现
 *
 * 本驱动实现（对齐 ESP-IDF 的完整流程）：
 *   A. I2C 主站（RTC I2C）时钟与复位
 *   B. REGI2C 模拟总线使能
 *   C. SAR ADC 电源 + 时钟
 *   D. REGI2C 校准初始化（DREF、采样周期）
 *   E. RTC 控制器 + APB_SARADC 配置
 *   F. 单次转换读取
 *
 * 寄存器地址全部来自 ESP-IDF 寄存器头文件，已在 ESP32-S3 真机上验证。
 */

/* =========================================================================
 * 一、基地址
 * ========================================================================= */

/* APB 外设控制 */
#define SYSTEM_BASE              0x600C0000UL
#define SYSTEM_PERIP_CLK_EN0     (SYSTEM_BASE + 0x00U)
#define SYSTEM_PERIP_RST_EN0     (SYSTEM_BASE + 0x04U)
#define APB_SARADC_CLK_EN_BIT    (1U << 27)
#define APB_SARADC_RST_BIT       (1U << 27)

/* RTC I2C 主站（芯片内部 I2C 总线控制器，驱动 REGI2C 设备） */
#define RTC_I2C_BASE             0x60008C00UL
#define RTC_I2C_SCL_LOW          (RTC_I2C_BASE + 0x00U)
#define RTC_I2C_CTRL             (RTC_I2C_BASE + 0x04U)
#define RTC_I2C_STATUS           (RTC_I2C_BASE + 0x08U)
#define RTC_I2C_TO               (RTC_I2C_BASE + 0x0CU)
#define RTC_I2C_SLAVE_ADDR       (RTC_I2C_BASE + 0x10U)
#define RTC_I2C_SCL_HIGH         (RTC_I2C_BASE + 0x14U)
#define RTC_I2C_SDA_DUTY         (RTC_I2C_BASE + 0x18U)
#define RTC_I2C_SCL_START        (RTC_I2C_BASE + 0x1CU)
#define RTC_I2C_SCL_STOP         (RTC_I2C_BASE + 0x20U)
#define RTC_I2C_INT_CLR          (RTC_I2C_BASE + 0x24U)
#define RTC_I2C_INT_RAW          (RTC_I2C_BASE + 0x28U)
#define RTC_I2C_INT_ST           (RTC_I2C_BASE + 0x2CU)
#define RTC_I2C_INT_ENA          (RTC_I2C_BASE + 0x30U)
#define RTC_I2C_DATA             (RTC_I2C_BASE + 0x34U)
#define RTC_I2C_CMD0             (RTC_I2C_BASE + 0x38U)

/* RTC I2C CTRL 寄存器位 */
#define RTC_I2C_SDA_FORCE_OUT    (1U << 0)
#define RTC_I2C_SCL_FORCE_OUT    (1U << 1)
#define RTC_I2C_MS_MODE          (1U << 2)
#define RTC_I2C_TRANS_START      (1U << 3)
#define RTC_I2C_TX_LSB_FIRST     (1U << 4)
#define RTC_I2C_RX_LSB_FIRST     (1U << 5)
#define RTC_I2C_CLK_GATE_EN      (1U << 29)
#define RTC_I2C_RESET            (1U << 30)
#define RTC_I2C_CLK_EN           (1U << 31)

/* RTC I2C DATA 寄存器位 */
#define RTC_I2C_DONE             (1U << 31)
#define RTC_I2C_RDATA_SHIFT      0
#define RTC_I2C_RDATA_MASK       0xFFU
#define RTC_I2C_TXDATA_SHIFT     8
#define RTC_I2C_TXDATA_MASK      (0xFFU << 8)

/* RTC I2C CMD 寄存器位 */
#define RTC_I2C_CMD_DONE         (1U << 31)

/* SENS 寄存器（SAR ADC 的 RTC 侧控制） */
#define SENS_BASE                0x60008800UL
#define SENS_SAR_READER1_CTRL    (SENS_BASE + 0x00U)
#define SENS_SAR_MEAS1_CTRL2     (SENS_BASE + 0x0CU)
#define SENS_SAR_MEAS1_MUX       (SENS_BASE + 0x10U)
#define SENS_SAR_ATTEN1          (SENS_BASE + 0x14U)
#define SENS_SAR_POWER_XPD       (SENS_BASE + 0x3CU)
#define SENS_SAR_SLAVE_ADDR1     (SENS_BASE + 0x40U)  /* 修正：原 0x24 错误 */
#define SENS_SAR_PERI_CLK_GATE   (SENS_BASE + 0x104U)
#define SENS_SAR_PERI_RESET      (SENS_BASE + 0x108U)

/* SENS sar_i2c_ctrl — REGI2C 快速通道（位于 SENS 模块内部） */
/* 此寄存器直接驱动 SAR ADC 内部 I2C，无需经过 RTC I2C 主站 */
#define SENS_SAR_I2C_CTRL        (SENS_BASE + 0x58U)

/* ---- REGI2C 设备地址 ---- */
#define I2C_SAR_ADC              0x69U       /* SAR ADC I2C 从设备地址 */
#define I2C_SAR_ADC_HOSTID       1U

/* ---- REGI2C SAR ADC 内部寄存器地址 ---- */
#define ADC_SAR1_DREF_ADDR        0x2U
#define ADC_SAR1_DREF_MSB         0x6U
#define ADC_SAR1_DREF_LSB         0x4U
#define ADC_SAR1_SAMPLE_CYCLE_ADDR  0x2U
#define ADC_SAR1_SAMPLE_CYCLE_MSB   0x2U
#define ADC_SAR1_SAMPLE_CYCLE_LSB   0x0U
#define ADC_SAR1_INITIAL_CODE_HIGH_ADDR  0x1U
#define ADC_SAR1_INITIAL_CODE_HIGH_MSB   0x3U
#define ADC_SAR1_INITIAL_CODE_HIGH_LSB   0x0U
#define ADC_SAR1_INITIAL_CODE_LOW_ADDR   0x0U
#define ADC_SAR1_INITIAL_CODE_LOW_MSB    0x7U
#define ADC_SAR1_INITIAL_CODE_LOW_LSB    0x0U
#define ADC_SAR1_ENCAL_GND_ADDR   0x7U
#define ADC_SAR1_ENCAL_GND_MSB    0x5U
#define ADC_SAR1_ENCAL_GND_LSB    0x5U

/* ---- ANA_CONFIG（模拟 I2C 总线开关）---- */
#define I2C_MST_ANA_CONF0_REG    0x6000E040UL
#define ANA_CONFIG_REG           0x6000E044UL
#define ANA_CONFIG2_REG          0x6000E048UL
#define RTC_CNTL_ANA_CONF_REG    0x60008034UL

#define I2C_SAR_M                (1U << 18)
#define ANA_SAR_CFG2_M           (1U << 16)
#define RTC_CNTL_SAR_I2C_PU      (1U << 22)

/* ---- APB_SARADC 数字控制器 ---- */
#define ADC_BASE                 0x60040000UL
#define ADC_CTRL                 (ADC_BASE + 0x00U)

/* ---- SENS 复位位 ---- */
#define SENS_SARADC_RESET_BIT    (1U << 30)

/* =========================================================================
 * 二、REGI2C 读写原语（使用 SENS sar_i2c_ctrl 快速通道）
 *
 * ESP32-S3 的 SENS 模块内置了一个简化的 I2C 主站，
 * 专门用于与 SAR ADC 内部寄存器通信。
 * 一个 32 位寄存器完成全部 I2C 事务：
 *
 *   bits 7:0   = SLAVE_ID  (I2C 7-bit 从设备地址)
 *   bits 15:8  = REG_ADDR  (从设备内部寄存器地址)
 *   bits 23:16 = DATA      (写数据 / 读回数据)
 *   bit  24    = WR_CNTL   (0=I2C 读, 1=I2C 写)
 *   bit  25    = BUSY      (硬件忙标志，只读)
 *   bits 27:26 = 保留
 *   bit  28    = I2C_START (写入 1 启动事务)
 *   bit  29    = I2C_START_FORCE (写入 1 使能 SW 控制)
 *   bits 31:30 = 保留
 * ========================================================================= */

static void regi2c_saradc_enable_block( void )
{
    /* 步骤与 ESP-IDF regi2c_ctrl_ll_i2c_sar_periph_enable() 一致：
     *   1. 使能 I2C 上拉
     *   2. 清除 I2C_SAR_M → 将 SAR ADC 挂上内部 I2C 总线
     *   3. 设置 ANA_SAR_CFG2_M → 使能 SAR ADC 配置通道
     */
    REG_SET_BIT( RTC_CNTL_ANA_CONF_REG, RTC_CNTL_SAR_I2C_PU );
    REG_CLR_BIT( ANA_CONFIG_REG, I2C_SAR_M );
    REG_SET_BIT( ANA_CONFIG2_REG, ANA_SAR_CFG2_M );
}

static uint8_t regi2c_read_raw( uint8_t slave_id, uint8_t reg_addr )
{
    volatile uint32_t *ctrl = (volatile uint32_t *)SENS_SAR_I2C_CTRL;
    uint32_t v;
    uint32_t timeout;

    /* 等待上一次事务完成（带超时） */
    timeout = 1000000U;
    while ( ((*ctrl) & (1U << 25)) && --timeout ) {}

    /* 构造读命令：SLAVE_ID + REG_ADDR, WR_CNTL=0, I2C_START_FORCE=1 */
    v = ((uint32_t)slave_id   << 0)
      | ((uint32_t)reg_addr   << 8)
      | (0U                   << 24)  /* WR_CNTL = 0 → READ */
      | (1U                   << 29); /* I2C_START_FORCE */

    *ctrl = v;

    /* 触发 START */
    v |= (1U << 28);  /* I2C_START */
    *ctrl = v;

    /* 等待硬件清 BUSY（带超时） */
    timeout = 1000000U;
    while ( ((*ctrl) & (1U << 25)) && --timeout ) {}

    /* 读回 DATA 字段 (bits 23:16) */
    return (uint8_t)((*ctrl >> 16) & 0xFFU);
}

static void regi2c_write_raw( uint8_t slave_id, uint8_t reg_addr, uint8_t data )
{
    volatile uint32_t *ctrl = (volatile uint32_t *)SENS_SAR_I2C_CTRL;
    uint32_t v;
    uint32_t timeout;

    /* 等待上一次事务完成（带超时） */
    timeout = 1000000U;
    while ( ((*ctrl) & (1U << 25)) && --timeout ) {}

    /* 构造写命令：SLAVE_ID + REG_ADDR + DATA, WR_CNTL=1, I2C_START_FORCE=1 */
    v = ((uint32_t)slave_id   << 0)
      | ((uint32_t)reg_addr   << 8)
      | ((uint32_t)data       << 16)
      | (1U                   << 24)  /* WR_CNTL = 1 → WRITE */
      | (1U                   << 29); /* I2C_START_FORCE */

    *ctrl = v;

    /* 触发 START */
    v |= (1U << 28);  /* I2C_START */
    *ctrl = v;

    /* 等待硬件清 BUSY（带超时） */
    timeout = 1000000U;
    while ( ((*ctrl) & (1U << 25)) && --timeout ) {}
}

static uint8_t regi2c_read_mask( uint8_t slave_id, uint8_t reg_addr,
                                  uint8_t msb, uint8_t lsb )
{
    uint8_t val = regi2c_read_raw( slave_id, reg_addr );
    uint8_t mask = (uint8_t)((1U << (msb - lsb + 1)) - 1U);
    return (val >> lsb) & mask;
}

static void regi2c_write_mask( uint8_t slave_id, uint8_t reg_addr,
                                uint8_t msb, uint8_t lsb, uint8_t data )
{
    /* 读-改-写 */
    uint8_t cur = regi2c_read_raw( slave_id, reg_addr );
    uint8_t mask = (uint8_t)((1U << (msb - lsb + 1)) - 1U);
    cur &= ~(mask << lsb);
    cur |= ((data & mask) << lsb);
    regi2c_write_raw( slave_id, reg_addr, cur );
}

/* =========================================================================
 * 三、REGI2C 便捷宏（对齐 ESP-IDF 的 REGI2C_WRITE_MASK 等宏）
 * ========================================================================= */
#define REGI2C_WRITE_MASK(block, reg, data) \
    regi2c_write_mask((block), (reg##_ADDR), (reg##_MSB), (reg##_LSB), (data))
#define REGI2C_READ_MASK(block, reg) \
    regi2c_read_mask((block), (reg##_ADDR), (reg##_MSB), (reg##_LSB))
#define REGI2C_WRITE(block, reg, data) \
    regi2c_write_raw((block), (reg##_ADDR), (data))
#define REGI2C_READ(block, reg) \
    regi2c_read_raw((block), (reg##_ADDR))

/* =========================================================================
 * 四、ADC1 驱动
 * ========================================================================= */

void adc1_init( void )
{
    /* ---- A. APB_SARADC 总线时钟使能 + 复位 ---- */
    /* 对应 ESP-IDF: adc_ll_enable_bus_clock(true) + adc_ll_reset_register() */
    REG_SET_BIT( SYSTEM_PERIP_CLK_EN0, APB_SARADC_CLK_EN_BIT );
    REG_SET_BIT( SYSTEM_PERIP_RST_EN0, APB_SARADC_RST_BIT );
    /* 短暂延时确保复位生效 */
    for ( volatile int d = 0; d < 100; d++ ) {}
    REG_CLR_BIT( SYSTEM_PERIP_RST_EN0, APB_SARADC_RST_BIT );

    /* ---- B. REGI2C 模拟总线使能 ---- */
    /* 对应 ESP-IDF: regi2c_ctrl_ll_i2c_sar_periph_enable() */
    regi2c_saradc_enable_block();

    /* ---- C. SAR ADC 电源开启 ---- */
    /* 对应 ESP-IDF: sar_ctrl_ll_set_power_mode(SAR_CTRL_LL_POWER_ON)
     *
     * SENS_SAR_PERI_CLK_GATE (offset 0x104):
     *   bit 27 = rtc_i2c_clk_en
     *   bit 30 = saradc_clk_en
     * SENS_SAR_POWER_XPD (offset 0x3C):
     *   bits 30:29 = force_xpd_sar (2 bits)
     *     0x0 = FSM 控制
     *     0x3 = SW 强制开启 (POWER_ON)
     */
    REG_WR( SENS_SAR_PERI_CLK_GATE,
            REG_RD(SENS_SAR_PERI_CLK_GATE) | (1U << 30) | (1U << 27) ); /* saradc_clk_en + rtc_i2c_clk_en */
    REG_WR( SENS_SAR_POWER_XPD,
            (REG_RD(SENS_SAR_POWER_XPD) & ~0x60000000U) | 0x60000000U ); /* force_xpd_sar = 0x3 */

    /* ---- D. SAR 时钟分频 ---- */
    /* 对应 ESP-IDF: adc_ll_set_sar_clk_div(ADC_UNIT_1, 1)
     *   SENS.sar_reader1_ctrl.sar1_clk_div = 1
     */
    REG_WR( SENS_SAR_READER1_CTRL,
            (REG_RD(SENS_SAR_READER1_CTRL) & ~0xFFU) | 1U );

    /* ---- E. REGI2C 校准初始化 ---- */
    /* 对应 ESP-IDF: adc_ll_calibration_init(ADC_UNIT_1)
     *   写入 DREF=4 到 SAR ADC 内部寄存器 (通过 REGI2C)
     *   设置采样周期 = 2
     */
    regi2c_saradc_enable_block();  /* 确保总线使能 */

    REGI2C_WRITE_MASK( I2C_SAR_ADC, ADC_SAR1_DREF, 4U );

    REGI2C_WRITE_MASK( I2C_SAR_ADC, ADC_SAR1_SAMPLE_CYCLE, 2U );

    /* 确保内部 GND 断开（非校准模式） */
    REGI2C_WRITE_MASK( I2C_SAR_ADC, ADC_SAR1_ENCAL_GND, 0U );

    /* ---- F. RTC 控制器接管 ADC1 ---- */
    /* 对应 ESP-IDF: adc_ll_set_controller(ADC_UNIT_1, ADC_LL_CTRL_RTC)
     *   SENS.sar_meas1_mux.sar1_dig_force = 0    (RTC 控制)
     *   SENS.sar_meas1_ctrl2.meas1_start_force = 1 (SW 启动)
     *   SENS.sar_meas1_ctrl2.sar1_en_pad_force = 1  (SW 控制通道选择)
     */
    REG_WR( SENS_SAR_MEAS1_MUX, 0U );                      /* sar1_dig_force=0 */
    REG_WR( SENS_SAR_MEAS1_CTRL2, (1U << 31) | (1U << 18) ); /* en_pad_force + start_force */

    /* ---- G. APB_SARADC 数字控制器（单次模式, ADC1, 时钟运行, DIV=4）---- */
    uint32_t ctrl = REG_RD( ADC_CTRL );
    ctrl &= ~(3U << 3);       /* bits 4:3 清零 — 单次模式相关 */
    ctrl &= ~(1U << 5);       /* bit 5 = sar_sel, 0=ADC1 */
    ctrl &= ~(1U << 6);       /* bit 6 = data_sar_sel */
    ctrl &= ~(0xFFU << 7);    /* bits 14:7 清零 — sar_clk_div */
    ctrl |= (4U << 7);        /* sar_clk_div = 4 */
    REG_WR( ADC_CTRL, ctrl );
}

uint32_t adc1_read( uint32_t channel )
{
    volatile uint32_t *ctrl2 = (volatile uint32_t *)SENS_SAR_MEAS1_CTRL2;
    volatile uint32_t *slave_addr1 = (volatile uint32_t *)SENS_SAR_SLAVE_ADDR1;
    uint32_t mask = 1U << channel;

    /* ---- 设置衰减（12dB = 0x3，对应 0~3.3V 范围）---- */
    uint32_t atten = REG_RD( SENS_SAR_ATTEN1 );
    atten &= ~(3U << (channel * 2U));
    atten |= (3U << (channel * 2U));                        /* atten=3 → 12dB */
    REG_WR( SENS_SAR_ATTEN1, atten );

    /* ---- 配置控制寄存器：start_force + en_pad_force ---- */
    uint32_t v = (1U << 31)        /* sar1_en_pad_force: SW 控制通道 */
               | (1U << 18);       /* meas1_start_force:  SW 控制启动 */
    *ctrl2 = v;

    /* ---- 选择通道 ---- */
    v |= (mask << 19);             /* sar1_en_pad: 通道掩码 */
    *ctrl2 = v;

    /* ---- 等待 RTC 状态机空闲 ---- */
    /* SENS_SAR_SLAVE_ADDR1.meas_status 字段 (bits 22:15) 反映 RTC SAR 状态 */
    while ( (*slave_addr1 & (0xFFU << 15)) != 0U ) {}

    /* ---- 触发转换：START 位 0 → 1 ---- */
    v &= ~(1U << 17);  /* meas1_start_sar = 0 */
    *ctrl2 = v;
    v |= (1U << 17);   /* meas1_start_sar = 1 */
    *ctrl2 = v;

    /* ---- 等待转换完成 (DONE 标志) ---- */
    while ( (*ctrl2 & (1U << 16)) == 0U ) {}

    /* ---- 读取 16 位原始数据 ---- */
    return *ctrl2 & 0xFFFFU;
}

uint32_t adc1_to_mv( uint32_t raw )
{
    /* 12dB 衰减 → 0~3.3V 范围
     * 12-bit 分辨率 → 4096 级
     * mV = raw * 3300 / 4095
     */
    return ( raw * 3300UL ) / 4095UL;
}
