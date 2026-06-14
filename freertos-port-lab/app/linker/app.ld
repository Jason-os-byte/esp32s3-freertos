/* ESP32-S3 custom-app 链接脚本：与 ESP-IDF v5.5 bootloader 兼容
 *
 * 关键点（被踩过的坑）：
 *  1) 镜像 segment #0 必须是 DROM（0x3C000000 段），起始处放 esp_app_desc_t
 *     （magic=0xABCD5432），否则 bootloader 在加载阶段 assert -> abort。
 *  2) IROM/DROM 段的 vaddr 必须落在 64 KB 页基址 + 0x20（段头预留）。
 *  3) IRAM 段 (.iram.text) 由 bootloader 拷贝执行，AT 区不要占 IROM 页。
 */

MEMORY
{
  /* segment #0：DROM (rodata + app_desc)，放在最前 */
  drom_seg (R)  : ORIGIN = 0x3C000020, LENGTH = 0x100000

  /* segment #1：IRAM，启动指令常驻（.text.startup） */
  iram_seg (RX) : ORIGIN = 0x40370000, LENGTH = 0x10000

  /* segment #2：IROM，普通 .text/.literal 直接 XIP */
  irom_seg (RX) : ORIGIN = 0x42000020, LENGTH = 0x300000

  /* DRAM：.data / .bss / 栈
   * 注意：ESP32-S3 internal SRAM (DRAM 视角) 实际可用范围
   *   0x3FC88000 .. 0x3FCF0000  (416 KB = 0x68000)
   * 0x3FCF0000 之后是 MAC dump / RTC 等保留区，写入会触发
   * LoadStoreError → KernelException。所以 LENGTH 必须 <= 0x68000。 */
  dram_seg (RW) : ORIGIN = 0x3FC88000, LENGTH = 0x68000
}

ENTRY(_start)

SECTIONS
{
  /* ===== Segment #0: DROM ===== */
  .flash.appdesc : ALIGN(0x10)
  {
    _rodata_start = ABSOLUTE(.);
    KEEP(*(.rodata_desc .rodata_desc.*))   /* esp_app_desc：必须留在 segment #0 DROM */
    /* 注意：*(.rodata*) 已移到 .data 段 (DRAM)，避免早期 DROM 读取问题 */
  } > drom_seg

  /* ===== Segment #1: Xtensa vectors in IRAM =====
   * VECBASE 必须 0x400 对齐。CPU interrupt 19 是 level-2 interrupt，
   * 入口地址是 VECBASE + 0x180。先保留完整 0x400 vector window，
   * 普通代码从 0x40370400 之后开始，避免 vector 与 .text 重叠。 */
  .iram.vectors ORIGIN(iram_seg) : ALIGN(0x400)
  {
    _vector_base = ABSOLUTE(.);
    . = 0x180;
    KEEP(*(.Level2InterruptVector.text))
    . = 0x400;
  } > iram_seg AT> drom_seg

  ASSERT((_vector_base & 0x3ff) == 0, "VECBASE must be 0x400 aligned")
  ASSERT(_Level2Vector == (_vector_base + 0x180), "Level2 vector must be at VECBASE+0x180")

  /* ===== Segment #2: IRAM normal code ===== */
  .iram.text : ALIGN(4)
  {
    *(.text.startup)
    *(.iram.text*)         /* IRAM_ATTR 函数 */
    *(.text*)              /* 调试期：把所有 .text 都搬进 IRAM，避免 IROM 跨段问题 */
    *(.literal*)
    . = ALIGN(4);
  } > iram_seg AT> drom_seg

  /* ===== Segment #3: IROM =====
   * 调试期所有代码都搬进了 IRAM，但 IROM 段不能为空（esptool 会丢空段，
   * bootloader segment 数对不上 → abort）。放一个 4 字节 dummy 占位。 */
  .flash.text : ALIGN(4)
  {
    _stext = ABSOLUTE(.);
    LONG(0xDEADBEEF)       /* 占位字节，确保 IROM 段非空 */
    *(.flash.text*)
    . = ALIGN(4);
  } > irom_seg

  /* DATA：Flash 里存一份（跟 DROM 同 LMA），运行时 crt0 拷到 DRAM */
  _data_load = LOADADDR(.data);
  .data : ALIGN(4)
  {
    _data_start = .;
    *(.data*)
    *(.rodata*)            /* 字符串字面量等放 DRAM，避免早期 DROM 读取 0xff */
    . = ALIGN(4);
    _data_end = .;
  } > dram_seg AT> drom_seg

  .bss (NOLOAD) : ALIGN(4)
  {
    _bss_start = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _bss_end = .;
  } > dram_seg

  _stack_top = ORIGIN(dram_seg) + LENGTH(dram_seg);
}
