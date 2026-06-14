# ESP32-S3 FreeRTOS 裸机移植

> 不使用 ESP-IDF 运行时，从零手写 startup / linker / port 层，接入官方 FreeRTOS-Kernel，实现单核抢占调度。面向机器人底层 MCU 控制。

## 项目架构

```text
┌──────────────────────────────────────────┐
│ Application Tasks                        │
│  gpio_blink / producer / consumer / ...  │
├──────────────────────────────────────────┤
│ FreeRTOS-Kernel (官方源码，未修改)         │
│  tasks.c  queue.c  timers.c  heap_4.c   │
├──────────────────────────────────────────┤
│ Port Layer (手写)                        │
│  portmacro.h  port.c  portasm.S          │
│  FreeRTOSConfig.h                        │
├──────────────────────────────────────────┤
│ BSP / Drivers                            │
│  systimer (1ms tick)  gpio  console     │
├──────────────────────────────────────────┤
│ Startup & Linker                         │
│  start.S  crt0.c  interrupt_vectors.S   │
│  app.ld                                  │
├──────────────────────────────────────────┤
│ ESP32-S3 Hardware                        │
│  Xtensa LX7  SYSTIMER  GPIO  USB-JTAG   │
└──────────────────────────────────────────┘
```

## 已实现功能

| 类别 | 功能 |
|---|---|
| 启动 | 裸机 startup、WDT 关闭、C runtime |
| 中断 | SYSTIMER 1ms tick、Xtensa level-2 vector |
| FreeRTOS | 抢占调度、统一 96 字节 rfi 帧 |
| IPC | Queue、Software Timer、Task Notification (ISR→Task) |
| 安全 | Stack Overflow Hook、Malloc Failed Hook |
| BSP | GPIO、USB-Serial/JTAG 控制台、SYSTIMER |

## 环境要求

- **硬件：** ESP32-S3 开发板（16MB Flash / 8MB PSRAM）
- **ESP-IDF：** v5.5.4（仅用于提供工具链和 esptool.py）
- **工具链：** xtensa-esp32s3-elf-gcc 14.2.0（ESP-IDF 自带）
- **FreeRTOS-Kernel：** commit `d877cd539`（git submodule）

## 编译与运行

### 1. 克隆仓库

```bash
git clone --recurse-submodules https://github.com/<用户名>/esp32s3-freertos-port.git
cd esp32s3-freertos-port
```

### 2. 安装并加载 ESP-IDF 工具链

```bash
# 在仓库同级目录安装 ESP-IDF v5.5.4
cd ..
git clone -b v5.5.4 --depth 1 https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
cd ../esp32s3-freertos-port

# 加载工具链
source ../esp-idf/export.sh
```

验证：

```bash
xtensa-esp32s3-elf-gcc --version
esptool.py version
```

### 3. 编译

```bash
cd freertos-port-lab
make clean && make all && make image
```

### 4. 烧录 bootloader 和分区表（只需一次）

```bash
esptool.py --chip esp32s3 --port /dev/ttyACM0 write_flash \
    0x0      reference/idf_sanity/bootloader.bin \
    0x8000   reference/idf_sanity/partition-table.bin
```

### 5. 烧录 app

```bash
make flash-app PORT=/dev/ttyACM0 APP_OFFSET=0x10000
```

> 烧录前如端口不通：按住 BOOT → 点按 RESET → 松开 BOOT（进入 ROM 下载模式）。烧录完成后拔插 USB 退出下载模式。

### 6. 查看输出

```bash
python -m esp_idf_monitor --port /dev/ttyACM0 --baud 115200 \
    build/esp32s3_freertos.elf
```

退出：`Ctrl + ]`

### 期望输出

```text
==================================
 [BSP] GPIO driver + FreeRTOS task
   ESP32-S3, LED on GPIO48, 500ms
==================================

[Step10] bss=0x00000000 data=0x12345678 [PASS]
[scheduler] starting...
[gpio_blink] LED toggle
[idle] heap=26960
...
```

### Flash 布局

```text
0x0       Bootloader
0x8000    分区表
0x10000   出厂 app（本项目）
```

## 目录结构

```text
freertos-port-lab/
├── Makefile
├── app/
│   ├── startup/                   # start.S, crt0.c, interrupt_vectors.S
│   ├── main/                      # main.c, app_desc.c
│   ├── freertos_port/
│   │   ├── include/               # FreeRTOSConfig.h, portmacro.h
│   │   ├── src/                   # port.c
│   │   └── asm/                   # portasm.S
│   ├── drivers/                   # gpio, uart(console), timer(systimer), watchdog
│   ├── common/                    # string.c (memcpy/memset)
│   └── linker/                    # app.ld
├── reference/idf_sanity/          # bootloader 与分区表基线
└── third_party/FreeRTOS-Kernel/   # git submodule（官方源码，未修改）
```

## License

MIT
