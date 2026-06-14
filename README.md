# ESP32-S3 FreeRTOS Industrial Port

> ESP32-S3 + Official FreeRTOS-Kernel, bare-metal port, single-core preemptive scheduling, targeting robot MCU control.

## Features

| Category | Feature | Status |
|---|---|---|
| Boot | Bare-metal startup, watchdog disable, C runtime | ✅ |
| Interrupt | SYSTIMER 1ms tick, Xtensa level-2 vector | ✅ |
| FreeRTOS | Official Kernel, preemptive scheduling, unified 96-byte rfi frame | ✅ |
| IPC | Queue, Software Timer, Task Notification (ISR→Task) | ✅ |
| Safety | Stack Overflow Hook, Malloc Failed Hook | ✅ |
| BSP | GPIO, USB-Serial/JTAG console, SYSTIMER | ✅ |
| Build | Pure Makefile, no idf.py dependency | ✅ |

## Prerequisites

### Hardware

- ESP32-S3 development board (16MB Flash / 8MB PSRAM)
- USB cable connected to the on-board USB-Serial/JTAG port
- The port will appear as `/dev/ttyACM0` on Linux

### Software

| Component | Version | Notes |
|---|---|---|
| ESP-IDF | v5.5.4 | Only needed for the toolchain and esptool.py. The app itself does NOT use idf.py or any ESP-IDF runtime. |
| Toolchain | xtensa-esp32s3-elf-gcc 14.2.0 | Comes with ESP-IDF |
| esptool.py | v4.x | Comes with ESP-IDF |
| FreeRTOS-Kernel | commit `d877cd539` | Included as a git submodule |

## Getting Started (from a fresh clone)

### Step 1: Clone the repository

```bash
git clone --recurse-submodules https://github.com/<your-username>/esp32s3-freertos-port.git
cd esp32s3-freertos-port
```

The `--recurse-submodules` flag pulls the FreeRTOS-Kernel source automatically.

### Step 2: Install ESP-IDF v5.5.4

Download ESP-IDF v5.5.4 from:

```text
https://github.com/espressif/esp-idf/releases/tag/v5.5.4
```

Extract it **next to** this repository (not inside it):

```bash
# Example:
cd ..
git clone -b v5.5.4 --depth 1 https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
cd ../esp32s3-freertos-port
```

### Step 3: Load the toolchain

```bash
source ../esp-idf/export.sh
```

Verify:

```bash
xtensa-esp32s3-elf-gcc --version
# Expected: xtensa-esp32s3-elf-gcc (crosstool-NG ...) 14.2.0

esptool.py version
# Expected: esptool.py v4.x
```

### Step 4: Build

```bash
cd freertos-port-lab
make clean
make all
make image
```

After a successful build:

```text
build/esp32s3_freertos.elf   # ELF executable
build/esp32s3_freertos.bin   # Flash image
```

### Step 5: Flash the bootloader and partition table (first time only)

The bootloader and partition table come from the ESP-IDF hello_world baseline.
They are stored in `reference/idf_sanity/` and only need to be flashed **once**.

```bash
esptool.py --chip esp32s3 --port /dev/ttyACM0 write_flash \
    0x0      reference/idf_sanity/bootloader.bin \
    0x8000   reference/idf_sanity/partition-table.bin
```

> If you ever re-flash a different ESP-IDF project, these will be overwritten.
> Simply re-run the command above to restore them before flashing this app.

### Step 6: Flash the app

```bash
make flash-app PORT=/dev/ttyACM0 APP_OFFSET=0x10000
```

If esptool reports "port is busy or doesn't exist":

```bash
# Hold BOOT, press RESET, release BOOT to enter ROM download mode.
# Then retry.
```

After flashing, **unplug and re-plug the USB cable** to exit download mode.

### Step 7: Monitor

```bash
python -m esp_idf_monitor --port /dev/ttyACM0 --baud 115200 \
    build/esp32s3_freertos.elf
```

Exit monitor: `Ctrl + ]`

### Expected Output

```text
==================================
 [BSP] GPIO driver + FreeRTOS task
   ESP32-S3, LED on GPIO48, 500ms
==================================

[Step10] bss=0x00000000 data=0x12345678 [PASS]
[scheduler] starting...
[gpio_blink] LED toggle
[idle] heap=26960
[gpio_blink] LED toggle
[idle] heap=26960
...
```

If your board has an LED on GPIO 48, it will blink at 500ms intervals.

## Rebuilding After Code Changes

```bash
cd freertos-port-lab
make clean && make all && make image
make flash-app PORT=/dev/ttyACM0 APP_OFFSET=0x10000
```

## Flash Layout

```text
Offset   | Content
---------|--------
0x0      | Bootloader (ESP-IDF v5.5.4)
0x8000   | Partition table
0x10000  | Factory app (this project)
```

## Directory Structure

```text
esp32s3-freertos-port/
├── README.md
├── .gitignore
├── .gitmodules
├── idf-env.sh
├── robot-env.sh
├── freertos-port-lab/
│   ├── Makefile
│   ├── README.md
│   ├── app/
│   │   ├── startup/                   # startup.S, crt0.c, interrupt_vectors.S
│   │   ├── main/                      # main.c, app_desc.c
│   │   ├── freertos_port/
│   │   │   ├── include/               # FreeRTOSConfig.h, portmacro.h
│   │   │   ├── src/                   # port.c
│   │   │   └── asm/                   # portasm.S
│   │   ├── drivers/                   # BSP: gpio, uart, timer, watchdog
│   │   ├── common/                    # string.c (memcpy/memset)
│   │   └── linker/                    # app.ld
│   ├── reference/idf_sanity/          # bootloader & partition table baseline
│   └── third_party/FreeRTOS-Kernel/   # git submodule
```

## Key Technical Details

### ABI: call0 (not windowed)

All C code is compiled with `-mabi=call0`. The startup assembly (`start.S`) uses `call0` to enter C. The context switch frame is 96 bytes, saved and restored through the `rfi 2` instruction path.

### Interrupt: SYSTIMER target0, CPU line 19, level 2

```
SYSTIMER target0 alarm
  → interrupt matrix (source 57 → CPU int 19)
  → Xtensa level-2 vector (VECBASE + 0x180)
  → assembly wrapper (96-byte frame save)
  → C ISR (clear + xTaskIncrementTick)
  → yield-from-ISR check
  → rfi 2
```

### Linker: .rodata in DRAM

To avoid early-stage DROM cache issues, `.rodata` is placed in `.data` (DRAM) instead of `.flash.appdesc` (DROM). The bootloader loads it from flash at boot time.

### Makefile: no ESP-IDF build system

The app is built entirely with a hand-written Makefile. No `idf.py`, no CMake, no `components/`. This keeps the port self-contained and portable.

## Troubleshooting

| Symptom | Check |
|---|---|
| `abort() at PC 0x403cdd3d` | Bootloader / partition table not flashed. Re-do Step 5. |
| `boot:0x3 (DOWNLOAD)` | Board stuck in ROM download mode. Unplug USB, wait 2 seconds, replug. |
| USB disconnect loop ("tick-tick" sound) | Flash the app again. The watchdog disable is in `_start`. |
| `make: xtensa-esp32s3-elf-gcc: command not found` | Toolchain not loaded. Re-do Step 3. |
| `Could not open /dev/ttyACM0` | Port busy or board not connected. Check `ls /dev/ttyACM0`. |
| `Saved PC: 0x403703c0` (Double Exception) | Hardware fault. Check serial output for the last working step. |

## License

MIT
