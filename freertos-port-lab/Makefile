PORT ?= /dev/ttyACM0
APP_OFFSET ?= 0x10000
TARGET := esp32s3_freertos
BUILD_DIR := build

CROSS := xtensa-esp32s3-elf-
CC := $(CROSS)gcc
OBJDUMP := $(CROSS)objdump
SIZE := $(CROSS)size

FREERTOS_KERNEL := third_party/FreeRTOS-Kernel

CFLAGS := -Os -mlongcalls -mtext-section-literals -mabi=call0 -fno-builtin -ffunction-sections -fdata-sections -g3
CFLAGS += -Iapp -Iapp/common -Iapp/drivers/watchdog -Iapp/drivers/uart -Iapp/drivers/timer -Iapp/drivers/gpio -Iapp/drivers/pwm
CFLAGS += -Iapp/freertos_port/include
CFLAGS += -I$(FREERTOS_KERNEL)/include
LDFLAGS := -Tapp/linker/app.ld -nostdlib -Wl,-Map=$(BUILD_DIR)/$(TARGET).map -Wl,--entry=_start
LDFLAGS += -Wl,--undefined=crt0_main -Wl,--undefined=main -Wl,--undefined=console_putc -Wl,--undefined=esp_app_desc

ESPTOOL_GLOBAL := --chip esp32s3
FLASH_PARAMS := --flash_mode dio --flash_freq 80m --flash_size 16MB

SRCS := \
    app/startup/start.S \
    app/startup/interrupt_vectors.S \
    app/startup/crt0.c \
    app/common/string.c \
    app/drivers/watchdog/watchdog.c \
    app/drivers/uart/console.c \
    app/drivers/uart/uart1.c \
    app/drivers/gpio/gpio.c \
    app/drivers/pwm/pwm.c \
    app/drivers/timer/systimer.c \
    app/main/app_desc.c \
    app/main/main.c \
    app/freertos_port/src/port.c \
    app/freertos_port/asm/portasm.S \
    $(FREERTOS_KERNEL)/tasks.c \
    $(FREERTOS_KERNEL)/queue.c \
    $(FREERTOS_KERNEL)/list.c \
    $(FREERTOS_KERNEL)/timers.c \
    $(FREERTOS_KERNEL)/event_groups.c \
    $(FREERTOS_KERNEL)/stream_buffer.c \
    $(FREERTOS_KERNEL)/portable/MemMang/heap_4.c

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(filter %.c,$(SRCS)))
OBJS += $(patsubst %.S,$(BUILD_DIR)/%.o,$(filter %.S,$(SRCS)))

.PHONY: all clean image flash-app monitor size objdump

all: $(BUILD_DIR)/$(TARGET).elf

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

image: $(BUILD_DIR)/$(TARGET).elf
	esptool.py $(ESPTOOL_GLOBAL) elf2image $(FLASH_PARAMS) --output $(BUILD_DIR)/$(TARGET).bin $<

flash-app: image
	esptool.py $(ESPTOOL_GLOBAL) --port $(PORT) write_flash $(APP_OFFSET) $(BUILD_DIR)/$(TARGET).bin

monitor:
	picocom -b 115200 $(PORT)

size: $(BUILD_DIR)/$(TARGET).elf
	$(SIZE) $<

objdump: $(BUILD_DIR)/$(TARGET).elf
	$(OBJDUMP) -d $< > $(BUILD_DIR)/$(TARGET).dump

clean:
	rm -rf $(BUILD_DIR)
