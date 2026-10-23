/**
 * @file    crc.c
 * @brief   CRCs and checksums (see crc.h).
 */
#include "crc.h"

#define CRC8_MAXIM_POLY_REFL   (0x8CU)    /* 0x31 reflected  */
#define CRC8_SMBUS_POLY        (0x07U)
#define CRC16_CCITT_POLY       (0x1021U)
#define CRC16_MODBUS_POLY_REFL (0xA001U)  /* 0x8005 reflected */

uint8_t crc8_maxim(uint8_t crc, const uint8_t *data, size_t len)
{
    while (len-- > 0U) {
        crc ^= *data++;
        for (uint32_t bit = 0U; bit < 8U; bit++) {
            /* 0 - (crc & 1) is 0x00 or 0xFF: branch-free conditional XOR */
            const uint8_t mask = (uint8_t)(0U - (uint32_t)(crc & 1U));
            crc = (uint8_t)((crc >> 1) ^ (CRC8_MAXIM_POLY_REFL & mask));
        }
    }
    return crc;
}

uint8_t crc8_smbus(uint8_t crc, const uint8_t *data, size_t len)
{
    while (len-- > 0U) {
        crc ^= *data++;
        for (uint32_t bit = 0U; bit < 8U; bit++) {
            crc = (uint8_t)(((crc & 0x80U) != 0U) ? (((uint32_t)crc << 1) ^ CRC8_SMBUS_POLY) : ((uint32_t)crc << 1));
        }
    }
    return crc;
}

/* 16-entry table for poly 0x1021, MSB first, one nibble at a time. */
static const uint16_t crc16_1021_nibble[16] = {
    0x0000U, 0x1021U, 0x2042U, 0x3063U, 0x4084U, 0x50A5U, 0x60C6U, 0x70E7U,
    0x8108U, 0x9129U, 0xA14AU, 0xB16BU, 0xC18CU, 0xD1ADU, 0xE1CEU, 0xF1EFU
};

static uint16_t crc16_1021(uint16_t crc, const uint8_t *data, size_t len)
{
    while (len-- > 0U) {
        const uint8_t byte = *data++;
        crc = (uint16_t)((crc << 4) ^ crc16_1021_nibble[(crc >> 12) ^ (byte >> 4)]);
        crc = (uint16_t)((crc << 4) ^ crc16_1021_nibble[(crc >> 12) ^ (byte & 0x0FU)]);
    }
    return crc;
}

uint16_t crc16_ccitt(uint16_t crc, const uint8_t *data, size_t len)
{
    return crc16_1021(crc, data, len);
}

uint16_t crc16_xmodem(uint16_t crc, const uint8_t *data, size_t len)
{
    return crc16_1021(crc, data, len);
}

uint16_t crc16_modbus(uint16_t crc, const uint8_t *data, size_t len)
{
    while (len-- > 0U) {
        crc ^= *data++;
        for (uint32_t bit = 0U; bit < 8U; bit++) {
            crc = (uint16_t)((crc & 1U) ? ((crc >> 1) ^ CRC16_MODBUS_POLY_REFL) : (crc >> 1));
        }
    }
    return crc;
}

/* 16-entry table for reflected poly 0xEDB88320. */
static const uint32_t crc32_nibble[16] = {
    0x00000000UL, 0x1DB71064UL, 0x3B6E20C8UL, 0x26D930ACUL,
    0x76DC4190UL, 0x6B6B51F4UL, 0x4DB26158UL, 0x5005713CUL,
    0xEDB88320UL, 0xF00F9344UL, 0xD6D6A3E8UL, 0xCB61B38CUL,
    0x9B64C2B0UL, 0x86D3D2D4UL, 0xA00AE278UL, 0xBDBDF21CUL
};

uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    while (len-- > 0U) {
        crc ^= *data++;
        crc = (crc >> 4) ^ crc32_nibble[crc & 0x0FU];
        crc = (crc >> 4) ^ crc32_nibble[crc & 0x0FU];
    }
    return crc;
}

uint32_t crc32(const uint8_t *data, size_t len)
{
    return crc32_final(crc32_update(CRC32_INIT, data, len));
}

/* ------------------------- simple checksums ----------------------- */

uint8_t checksum_sum8(const uint8_t *data, size_t len)
{
    uint8_t sum = 0U;
    while (len-- > 0U) {
        sum = (uint8_t)(sum + *data++);
    }
    return sum;
}

uint8_t checksum_xor8(const uint8_t *data, size_t len)
{
    uint8_t x = 0U;
    while (len-- > 0U) {
        x ^= *data++;
    }
    return x;
}

uint8_t checksum_twos8(const uint8_t *data, size_t len)
{
    return (uint8_t)(0U - checksum_sum8(data, len));
}

uint16_t checksum_fletcher16(const uint8_t *data, size_t len)
{
    uint16_t sum1 = 0U;
    uint16_t sum2 = 0U;
    while (len-- > 0U) {
        sum1 = (uint16_t)((sum1 + *data++) % 255U);
        sum2 = (uint16_t)((sum2 + sum1) % 255U);
    }
    return (uint16_t)((sum2 << 8) | sum1);
}

uint32_t checksum_adler32(const uint8_t *data, size_t len)
{
    const uint32_t mod_adler = 65521UL;
    uint32_t a = 1U;
    uint32_t b = 0U;
    while (len-- > 0U) {
        a = (a + *data++) % mod_adler;
        b = (b + a) % mod_adler;
    }
    return (b << 16) | a;
}

uint16_t checksum_inet(const uint8_t *data, size_t len)
{
    uint32_t sum = 0U;
    while (len > 1U) {
        sum  += ((uint32_t)data[0] << 8) | data[1];
        data += 2;
        len  -= 2U;
    }
    if (len == 1U) {
        sum += (uint32_t)data[0] << 8;
    }
    /* fold the carries back in (end-around carry) */
    while ((sum >> 16) != 0U) {
        sum = (sum & 0xFFFFU) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

uint8_t checksum_nmea(const char *sentence)
{
    uint8_t x = 0U;
    if (*sentence == '$') {
        sentence++;
    }
    while ((*sentence != '\0') && (*sentence != '*')) {
        x ^= (uint8_t)*sentence++;
    }
    return x;
}
