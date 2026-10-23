/**
 * @file    config.c
 * @brief   Log-structured key/value store in flash (see config.h).
 *
 * Page layout (1 KiB, half-word writes only):
 *   [magic 0xC0F1][generation]  header, 4 bytes
 *   [key][len][value... padded to even][crc16]   records, back to back
 *   0xFF...                                       free space
 * len == 0 is a tombstone (key deleted).
 */
#include "config.h"
#include "hal_flash.h"
#include "crc.h"

#define CFG_MAGIC         (0xC0F1U)
#define CFG_HDR_SIZE      (4U)
#define CFG_REC_HDR       (2U)
#define CFG_CRC_SIZE      (2U)
#define CFG_KEY_FREE      (0xFFU)
#define CFG_PAGE_COUNT    (2U)

typedef struct {
    uint32_t base;        /* active page address        */
    uint32_t write_off;   /* first free byte in it      */
    uint16_t generation;
    bool     ready;
} cfg_state_t;

static cfg_state_t s_cfg;

static uint32_t page_addr(uint32_t index)
{
    return hal_flash_user_base() + (index * HAL_FLASH_PAGE_SIZE);
}

static uint32_t padded(uint32_t len)
{
    return (len + 1U) & ~1U;
}

static uint32_t record_size(uint8_t len)
{
    return CFG_REC_HDR + padded(len) + CFG_CRC_SIZE;
}

static uint16_t record_crc(uint8_t key, uint8_t len, const uint8_t *value)
{
    const uint8_t hdr[2] = { key, len };
    uint16_t crc = crc16_ccitt(CRC16_CCITT_INIT, hdr, sizeof(hdr));
    return crc16_ccitt(crc, value, len);
}

/* Read the record at @p off; returns false at the end of the log. */
static bool read_record(uint32_t base, uint32_t off, uint8_t *key, uint8_t *len,
                        uint8_t value[CONFIG_VALUE_MAX], bool *crc_ok)
{
    uint8_t hdr[2];
    uint8_t crc_bytes[2];

    if ((off + CFG_REC_HDR) > HAL_FLASH_PAGE_SIZE) {
        return false;
    }
    hal_flash_read(base + off, hdr, sizeof(hdr));
    if ((hdr[0] == CFG_KEY_FREE) || (hdr[1] > CONFIG_VALUE_MAX) ||
        ((off + record_size(hdr[1])) > HAL_FLASH_PAGE_SIZE)) {
        return false;
    }
    *key = hdr[0];
    *len = hdr[1];
    hal_flash_read(base + off + CFG_REC_HDR, value, *len);
    hal_flash_read(base + off + CFG_REC_HDR + padded(*len), crc_bytes, sizeof(crc_bytes));
    const uint16_t stored = (uint16_t)(crc_bytes[0] | ((uint16_t)crc_bytes[1] << 8));
    *crc_ok = (stored == record_crc(*key, *len, value));
    return true;
}

static bool page_header(uint32_t base, uint16_t *generation)
{
    uint8_t h[CFG_HDR_SIZE];
    hal_flash_read(base, h, sizeof(h));
    const uint16_t magic = (uint16_t)(h[0] | ((uint16_t)h[1] << 8));
    *generation = (uint16_t)(h[2] | ((uint16_t)h[3] << 8));
    return magic == CFG_MAGIC;
}

static hal_status_t write_header(uint32_t base, uint16_t generation)
{
    const uint8_t h[CFG_HDR_SIZE] = {
        (uint8_t)CFG_MAGIC, (uint8_t)(CFG_MAGIC >> 8), (uint8_t)generation, (uint8_t)(generation >> 8)
    };
    return hal_flash_write(base, h, sizeof(h));
}

static uint32_t find_end(uint32_t base)
{
    uint32_t off = CFG_HDR_SIZE;
    uint8_t  key;
    uint8_t  len;
    uint8_t  value[CONFIG_VALUE_MAX];
    bool     ok;
    while (read_record(base, off, &key, &len, value, &ok)) {
        off += record_size(len);
    }
    return off;
}

hal_status_t config_format(void)
{
    for (uint32_t i = 0U; i < CFG_PAGE_COUNT; i++) {
        const hal_status_t st = hal_flash_erase_page(page_addr(i));
        if (st != HAL_OK) {
            return st;
        }
    }
    s_cfg.base       = page_addr(0U);
    s_cfg.generation = 1U;
    s_cfg.write_off  = CFG_HDR_SIZE;
    s_cfg.ready      = true;
    return write_header(s_cfg.base, s_cfg.generation);
}

hal_status_t config_init(void)
{
    uint16_t g0;
    uint16_t g1;
    const bool v0 = page_header(page_addr(0U), &g0);
    const bool v1 = page_header(page_addr(1U), &g1);

    if (!v0 && !v1) {
        return config_format();          /* blank flash: first boot */
    }
    /* newest generation wins; (int16_t) difference handles the wrap */
    const bool use1 = v1 && (!v0 || ((int16_t)(g1 - g0) > 0));
    s_cfg.base       = page_addr(use1 ? 1U : 0U);
    s_cfg.generation = use1 ? g1 : g0;
    s_cfg.write_off  = find_end(s_cfg.base);
    s_cfg.ready      = true;
    return HAL_OK;
}

/* Latest valid value of a key in a page; -1 if absent or deleted. */
static int32_t lookup(uint32_t base, uint8_t key, uint8_t *value, uint8_t max_len)
{
    uint32_t off   = CFG_HDR_SIZE;
    int32_t  found = -1;
    uint8_t  k;
    uint8_t  len;
    uint8_t  buf[CONFIG_VALUE_MAX];
    bool     ok;

    while (read_record(base, off, &k, &len, buf, &ok)) {
        if (ok && (k == key)) {
            found = (len == 0U) ? -1 : (int32_t)len;
            for (uint32_t i = 0U; (i < len) && (i < max_len); i++) {
                value[i] = buf[i];
            }
        }
        off += record_size(len);
    }
    return found;
}

static hal_status_t append(uint8_t key, const uint8_t *value, uint8_t len)
{
    uint8_t        rec[CFG_REC_HDR + CONFIG_VALUE_MAX + 1U + CFG_CRC_SIZE];
    const uint32_t size = record_size(len);
    const uint16_t crc  = record_crc(key, len, value);

    rec[0] = key;
    rec[1] = len;
    for (uint32_t i = 0U; i < padded(len); i++) {
        rec[CFG_REC_HDR + i] = (i < len) ? value[i] : 0xFFU;
    }
    rec[CFG_REC_HDR + padded(len)]      = (uint8_t)crc;
    rec[CFG_REC_HDR + padded(len) + 1U] = (uint8_t)(crc >> 8);

    const hal_status_t st = hal_flash_write(s_cfg.base + s_cfg.write_off, rec, size);
    if (st == HAL_OK) {
        s_cfg.write_off += size;
    }
    return st;
}

/* Copy the live keys to the other page and switch to it. */
static hal_status_t compact(void)
{
    const uint32_t old_base = s_cfg.base;
    const uint32_t new_base = (old_base == page_addr(0U)) ? page_addr(1U) : page_addr(0U);
    uint8_t        value[CONFIG_VALUE_MAX];

    hal_status_t st = hal_flash_erase_page(new_base);
    if (st != HAL_OK) {
        return st;
    }
    s_cfg.base      = new_base;
    s_cfg.write_off = CFG_HDR_SIZE;
    for (uint32_t key = 1U; key < CONFIG_KEYS_MAX; key++) {
        const int32_t len = lookup(old_base, (uint8_t)key, value, CONFIG_VALUE_MAX);
        if (len > 0) {
            st = append((uint8_t)key, value, (uint8_t)len);
            if (st != HAL_OK) {
                return st;
            }
        }
    }
    /* header written last: until then the old page stays the valid one */
    s_cfg.generation++;
    return write_header(new_base, s_cfg.generation);
}

hal_status_t config_set(uint8_t key, const void *value, uint8_t len)
{
    uint8_t current[CONFIG_VALUE_MAX];

    if ((key == 0U) || (key >= CONFIG_KEYS_MAX) || (len > CONFIG_VALUE_MAX)) {
        return HAL_EINVAL;
    }
    if (!s_cfg.ready) {
        const hal_status_t st = config_init();
        if (st != HAL_OK) {
            return st;
        }
    }
    /* unchanged value: no flash write at all (saves wear) */
    const int32_t cur = lookup(s_cfg.base, key, current, CONFIG_VALUE_MAX);
    if ((len > 0U) && (cur == (int32_t)len)) {
        bool same = true;
        for (uint32_t i = 0U; i < len; i++) {
            same = same && (current[i] == ((const uint8_t *)value)[i]);
        }
        if (same) {
            return HAL_OK;
        }
    }
    if ((s_cfg.write_off + record_size(len)) > HAL_FLASH_PAGE_SIZE) {
        const hal_status_t st = compact();
        if (st != HAL_OK) {
            return st;
        }
        if ((s_cfg.write_off + record_size(len)) > HAL_FLASH_PAGE_SIZE) {
            return HAL_EBUSY;          /* store genuinely full */
        }
    }
    return append(key, (const uint8_t *)value, len);
}

int32_t config_get(uint8_t key, void *value, uint8_t max_len)
{
    if (!s_cfg.ready && (config_init() != HAL_OK)) {
        return -1;
    }
    return lookup(s_cfg.base, key, (uint8_t *)value, max_len);
}

hal_status_t config_delete(uint8_t key)
{
    uint8_t dummy[CONFIG_VALUE_MAX];
    if (config_get(key, dummy, sizeof(dummy)) < 0) {
        return HAL_OK;
    }
    return config_set(key, dummy, 0U);   /* tombstone */
}

hal_status_t config_set_u32(uint8_t key, uint32_t value)
{
    const uint8_t b[4] = { (uint8_t)value, (uint8_t)(value >> 8), (uint8_t)(value >> 16), (uint8_t)(value >> 24) };
    return config_set(key, b, sizeof(b));
}

uint32_t config_get_u32(uint8_t key, uint32_t default_value)
{
    uint8_t b[4];
    if (config_get(key, b, sizeof(b)) != 4) {
        return default_value;
    }
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

hal_status_t config_set_str(uint8_t key, const char *s)
{
    uint8_t len = 0U;
    while ((s[len] != '\0') && (len < CONFIG_VALUE_MAX)) {
        len++;
    }
    return config_set(key, s, len);
}

bool config_get_str(uint8_t key, char *buf, uint8_t size)
{
    if (size == 0U) {
        return false;
    }
    const int32_t n = config_get(key, buf, (uint8_t)(size - 1U));
    if (n < 0) {
        buf[0] = '\0';
        return false;
    }
    buf[((uint32_t)n < (uint32_t)(size - 1U)) ? (uint32_t)n : (uint32_t)(size - 1U)] = '\0';
    return true;
}

uint32_t config_used(void)
{
    return s_cfg.write_off;
}

uint32_t config_capacity(void)
{
    return HAL_FLASH_PAGE_SIZE;
}

uint32_t config_generation(void)
{
    return s_cfg.generation;
}

void config_foreach(void (*fn)(uint8_t key, const uint8_t *value, uint8_t len, void *arg), void *arg)
{
    uint8_t value[CONFIG_VALUE_MAX];
    if (!s_cfg.ready && (config_init() != HAL_OK)) {
        return;
    }
    for (uint32_t key = 1U; key < CONFIG_KEYS_MAX; key++) {
        const int32_t len = lookup(s_cfg.base, (uint8_t)key, value, CONFIG_VALUE_MAX);
        if (len > 0) {
            fn((uint8_t)key, value, (uint8_t)len, arg);
        }
    }
}
