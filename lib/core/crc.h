/**
 * @file    crc.h
 * @brief   CRCs and simple checksums, bitwise or 16-entry nibble tables.
 *
 * Bitwise versions cost no flash for tables; nibble-table versions are
 * ~4x faster and need only 16 entries (32-64 bytes) instead of 256.
 * All functions support incremental use: pass the previous result as
 * @p crc to continue over a new chunk (start from the *_INIT value,
 * finish with *_final when the algorithm has a final XOR).
 *
 * Check values ("123456789"):
 *   crc8_maxim   0xA1      crc8_smbus  0xF4
 *   crc16_ccitt  0x29B1    crc16_modbus 0x4B37   crc16_xmodem 0x31C3
 *   crc32        0xCBF43926
 */
#ifndef CRC_H
#define CRC_H

#include <stdint.h>
#include <stddef.h>

#define CRC8_MAXIM_INIT    (0x00U)
#define CRC8_SMBUS_INIT    (0x00U)
#define CRC16_CCITT_INIT   (0xFFFFU)
#define CRC16_MODBUS_INIT  (0xFFFFU)
#define CRC16_XMODEM_INIT  (0x0000U)
#define CRC32_INIT         (0xFFFFFFFFUL)

/** Dallas/Maxim 1-Wire CRC (DS18B20 ROM and scratchpad). */
uint8_t  crc8_maxim(uint8_t crc, const uint8_t *data, size_t len);
/** SMBus PEC / poly 0x07 (also used by many I2C sensors). */
uint8_t  crc8_smbus(uint8_t crc, const uint8_t *data, size_t len);
/** CRC-16/CCITT-FALSE, poly 0x1021, MSB first. */
uint16_t crc16_ccitt(uint16_t crc, const uint8_t *data, size_t len);
/** CRC-16/MODBUS, poly 0x8005 reflected (0xA001). Send low byte first. */
uint16_t crc16_modbus(uint16_t crc, const uint8_t *data, size_t len);
/** CRC-16/XMODEM, poly 0x1021, init 0. */
uint16_t crc16_xmodem(uint16_t crc, const uint8_t *data, size_t len);
/** CRC-32 (Ethernet, zip, PNG), reflected, nibble table. */
uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len);
static inline uint32_t crc32_final(uint32_t crc) { return crc ^ 0xFFFFFFFFUL; }
/** One-shot CRC-32. */
uint32_t crc32(const uint8_t *data, size_t len);

/* ------------------------- simple checksums ----------------------- */
uint8_t  checksum_sum8(const uint8_t *data, size_t len);
uint8_t  checksum_xor8(const uint8_t *data, size_t len);
/** Two's complement: sum of data + checksum == 0 (Intel HEX, NMEA-ish). */
uint8_t  checksum_twos8(const uint8_t *data, size_t len);
uint16_t checksum_fletcher16(const uint8_t *data, size_t len);
uint32_t checksum_adler32(const uint8_t *data, size_t len);
/** Internet checksum (RFC 1071), one's complement sum of 16-bit words. */
uint16_t checksum_inet(const uint8_t *data, size_t len);
/** NMEA sentence checksum: XOR of chars between '$' and '*'. */
uint8_t  checksum_nmea(const char *sentence);

#endif /* CRC_H */
