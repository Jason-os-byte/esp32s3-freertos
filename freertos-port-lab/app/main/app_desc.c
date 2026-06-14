/* esp_app_desc：放在镜像 segment #0 (DROM) 的最前面。
 *
 * ESP-IDF v5.5 bootloader 在 esp_image_format.c::process_appended_hash_and_sig
 * 之前，会读 segment #0 的前 8 字节做：
 *     assert(data_buffer[0] == ESP_APP_DESC_MAGIC_WORD);   // 0xABCD5432
 * 没有这个结构 → bootloader 自己 abort()，根本不进 app。
 *
 * 结构定义来自 components/esp_app_format/include/esp_app_desc.h
 * （这里我们只关心字段顺序与大小，所以独立复刻一份，避免拉 IDF 头）。
 * 大小必须等于 256 字节。
 */
#include <stdint.h>

#define ESP_APP_DESC_MAGIC_WORD 0xABCD5432UL

typedef struct {
    uint32_t magic_word;
    uint32_t secure_version;
    uint32_t reserv1[2];
    char     version[32];
    char     project_name[32];
    char     time[16];
    char     date[16];
    char     idf_ver[32];
    uint8_t  app_elf_sha256[32];
    uint16_t min_efuse_blk_rev_full;
    uint16_t max_efuse_blk_rev_full;
    uint8_t  mmu_page_size;        /* log2(page) - ESP32-S3 默认 64KB → 16 */
    uint8_t  reserv3[3];
    uint32_t reserv2[18];
} esp_app_desc_t;

_Static_assert(sizeof(esp_app_desc_t) == 256, "esp_app_desc_t must be 256 bytes");

__attribute__((section(".rodata_desc"), used))
const esp_app_desc_t esp_app_desc = {
    .magic_word            = ESP_APP_DESC_MAGIC_WORD,
    .secure_version        = 0,
    .version               = "freertos-port-lab",
    .project_name          = "esp32s3_freertos",
    .time                  = __TIME__,
    .date                  = __DATE__,
    .idf_ver               = "custom",
    .min_efuse_blk_rev_full = 0,
    .max_efuse_blk_rev_full = 0xFFFF,
    .mmu_page_size         = 16,   /* 1 << 16 = 64 KB */
};
