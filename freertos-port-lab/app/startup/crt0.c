#include <stdint.h>

extern uint32_t _bss_start, _bss_end;
extern int main(void);

void crt0_main(void)
{
    /* bootloader 会把 DRAM load segment (.data) 直接加载到 VMA，
     * 当前阶段不要从 _data_load 再拷贝：早期读取 DROM 仍不稳定，
     * 二次拷贝反而会破坏 .data 中的字符串。 */
    for (uint32_t *p = &_bss_start; p < &_bss_end; p++) {
        *p = 0;
    }

    main();
    while (1) { }
}
