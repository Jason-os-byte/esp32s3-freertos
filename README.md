# ESP32-S3 FreeRTOS Industrial Port

> ESP32-S3 + 官方 FreeRTOS-Kernel 手动移植，单核抢占调度，面向机器人底层控制。

## 项目定位

这不是一个玩具 demo，也不是 ESP-IDF 封装好的 FreeRTOS。这是：

```text
裸机 startup → 手写 linker → 手写 port 层 → 接入官方 FreeRTOS-Kernel
→ 真实 SYSTIMER 中断 → call0 ABI 上下文切换 → 抢占调度
→ queue / software timer / ISR notify → hooks + stack overflow
→ 机器人 BSP (GPIO/UART/PWM/Encoder/ADC)
```

对标行业 AMR/巡检机器人的 MCU 控制层。

## 已完成功能

| 类别 | 功能 | 状态 |
|---|---|---|
| 启动 | 裸机 startup、WDT 关闭、C runtime | ✅ |
| 中断 | SYSTIMER 1ms tick、Xtensa level-2 vector | ✅ |
| FreeRTOS | 官方 Kernel 编译、抢占调度、统一 96B rfi 帧 | ✅ |
| IPC | Queue、Software Timer、Task Notification (ISR→Task) | ✅ |
| 安全 | Stack Overflow Hook、Malloc Failed Hook | ✅ |
| BSP | GPIO、UART (debug console)、SYSTIMER | ✅ |
| 构建 | 纯 Makefile、无 idf.py 依赖 | ✅ |

## 环境依赖

- **工具链**：`xtensa-esp32s3-elf-gcc` (ESP-IDF v5.5.4 自带)
- **ESP-IDF**：v5.5.4（仅用于 bootloader 烧录基线）
- **烧录工具**：`esptool.py` v4.x
- **FreeRTOS-Kernel**：[官方仓库](https://github.com/FreeRTOS/FreeRTOS-Kernel) commit `d877cd539`

## 快速开始

```bash
# 1. 克隆仓库（含 FreeRTOS-Kernel submodule）
git clone --recurse-submodules <this-repo-url>
cd esp32s3_freeRTOS

# 2. 加载 ESP-IDF 环境
source scripts/idf-env.sh

# 3. 编译
cd freertos-port-lab
make clean && make all && make image

# 4. 烧录（板子：ESP32-S3, PORT=/dev/ttyACM0）
make flash-app PORT=/dev/ttyACM0 APP_OFFSET=0x10000

# 5. 查看串口
python -m esp_idf_monitor --port /dev/ttyACM0 --baud 115200 \
    build/esp32s3_freertos.elf
```

## 目录结构

```text
esp32s3_freeRTOS/
├── README.md                          # 本文件
├── freertos-port-lab/                 # 核心工程
│   ├── Makefile                       # 构建脚本
│   ├── app/
│   │   ├── startup/                   # 启动代码 + 中断向量
│   │   ├── main/                      # 应用代码
│   │   ├── freertos_port/             # FreeRTOS 移植层
│   │   ├── drivers/                   # BSP 驱动
│   │   └── linker/                    # 链接脚本
│   ├── docs/                          # 基线记录
│   └── reference/                     # bootloader 基线
├── docs/                              # 项目文档
├── scripts/                           # 环境脚本
├── third_party/                       # FreeRTOS-Kernel (submodule)
└── .gitignore
```

## 移植过程文档

- [实战修正版指南](docs/ESP32-S3-FreeRTOS-移植实战修正版.md) — 从第 1 步到第 18 步
- [调试复盘报告](docs/m2-custom-app-alive-debug-report.md) — 26 个问题逐个解析
- [原始移植指南](docs/ESP32-S3-FreeRTOS-工业级移植指南.md) — 理论参考
- [机器人路线图](docs/esp32s3_freertos_ros2_linux_kernel.md) — 顶层规划

## License

MIT
