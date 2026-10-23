/**
 * @file    datalogger.c
 * @brief   Data logger: samples sensors periodically and appends
 *          CRC-protected records to a W25Q SPI flash, survives resets
 *          (it finds the end of the log at boot), dumps CSV on demand.
 *
 *   W25Qxx on SPI1 (PA5 SCK, PA6 MISO, PA7 MOSI, PA4 CS)
 *   LM75 on I2C1 (PB6/PB7), analog input on PA1 (ADC ch1)
 *   console keys: d = dump CSV, s = statistics, e = erase log, q = quit (PC)
 *
 * Record (16 bytes): magic, sequence number, uptime s, LM75 0.1 C,
 * ADC mV, MCU 0.1 C, CRC-16. A record whose CRC fails (power cut during
 * programming) marks the end of the valid log.
 */
#include "hal.h"
#include "io.h"
#include "log.h"
#include "crc.h"
#include "bits.h"
#include "stats.h"
#include "filter.h"
#include "w25q.h"
#include "sensors_i2c.h"
#ifdef BOARD_PC
#include "sim.h"
#endif

#define LOG_PERIOD_MS     (1000U)
#define LOG_AREA_START    (0x000000U)
#define LOG_AREA_SIZE     (64U * 1024U)      /* 4096 records */
#define REC_MAGIC         (0xA55AU)
#define REC_SIZE          ((uint32_t)sizeof(log_record_t))
#define ADC_CHANNEL       (1U)
#define ADC_AVG_LOG2      (4U)

typedef struct {
    uint16_t magic;
    uint16_t crc;
    uint32_t seq;
    uint32_t uptime_s;
    int16_t  temp_c10;      /* LM75, INT16_MIN if absent */
    uint16_t adc_mv;
} log_record_t;

/* 16 bytes: a 256-byte flash page holds exactly 16 records */
STATIC_ASSERT(sizeof(log_record_t) == 16U, record_size_16);

static w25q_t   s_flash;
static uint32_t s_next_addr;
static uint32_t s_next_seq;

static uint16_t record_crc(const log_record_t *r)
{
    /* CRC over everything after the crc field */
    return crc16_ccitt(CRC16_CCITT_INIT, (const uint8_t *)&r->seq, sizeof(*r) - 4U);
}

static bool record_valid(const log_record_t *r)
{
    return (r->magic == REC_MAGIC) && (r->crc == record_crc(r));
}

/* Binary search for the first free slot: the log is written in order,
 * so valid records are followed by erased (0xFF) ones. O(log n) reads. */
static void find_log_end(void)
{
    uint32_t lo = 0U;
    uint32_t hi = LOG_AREA_SIZE / sizeof(log_record_t);
    log_record_t r;
    while (lo < hi) {
        const uint32_t mid = lo + ((hi - lo) / 2U);
        (void)w25q_read(&s_flash, LOG_AREA_START + (mid * REC_SIZE), &r, sizeof(r));
        if (record_valid(&r)) {
            lo = mid + 1U;
        } else {
            hi = mid;
        }
    }
    s_next_addr = LOG_AREA_START + (lo * REC_SIZE);
    s_next_seq  = 0U;
    if (lo > 0U) {
        (void)w25q_read(&s_flash, s_next_addr - REC_SIZE, &r, sizeof(r));
        s_next_seq = r.seq + 1U;
    }
}

static void erase_log(void)
{
    for (uint32_t a = 0U; a < LOG_AREA_SIZE; a += W25Q_SECTOR_SIZE) {
        (void)w25q_erase_sector(&s_flash, LOG_AREA_START + a);
    }
    s_next_addr = LOG_AREA_START;
    s_next_seq  = 0U;
    LOG_I("log", "log erased");
}

static void take_sample(void)
{
    log_record_t r;
    int32_t      c10 = 0;

    if (s_next_addr >= (LOG_AREA_START + LOG_AREA_SIZE)) {
        LOG_W("log", "log full");
        return;
    }
    r.magic    = REC_MAGIC;
    r.seq      = s_next_seq;
    r.uptime_s = hal_time_ms() / 1000U;
    r.temp_c10 = (lm75_read_c10(HAL_I2C1, LM75_ADDR_DEFAULT, &c10) == HAL_OK) ? (int16_t)c10 : INT16_MIN;
    r.adc_mv   = (uint16_t)hal_adc_raw_to_mv(hal_adc_read_avg(ADC_CHANNEL, ADC_AVG_LOG2), hal_adc_vdda_mv());
    r.crc      = record_crc(&r);
    if (w25q_write(&s_flash, s_next_addr, &r, sizeof(r)) != HAL_OK) {
        LOG_E("log", "flash write failed");
        return;
    }
    s_next_addr += REC_SIZE;
    s_next_seq++;
    LOG_D("log", "#%lu T=%d.%d C  A=%u mV", (unsigned long)r.seq, r.temp_c10 / 10, r.temp_c10 % 10, r.adc_mv);
}

static void dump_csv(void)
{
    log_record_t r;
    io_println("seq,uptime_s,temp_c,adc_mv");
    for (uint32_t a = LOG_AREA_START; a < s_next_addr; a += REC_SIZE) {
        (void)w25q_read(&s_flash, a, &r, sizeof(r));
        if (!record_valid(&r)) {
            break;
        }
        io_printf("%lu,%lu,", (unsigned long)r.seq, (unsigned long)r.uptime_s);
        if (r.temp_c10 == INT16_MIN) {
            io_puts(",");
        } else {
            io_print_fixed(r.temp_c10, 10U, 1U);
            io_puts(",");
        }
        io_printf("%u" IO_NEWLINE, (unsigned)r.adc_mv);
    }
}

static void print_stats(void)
{
    stats_t      t;
    stats_t      a;
    log_record_t r;
    stats_reset(&t);
    stats_reset(&a);
    for (uint32_t addr = LOG_AREA_START; addr < s_next_addr; addr += REC_SIZE) {
        (void)w25q_read(&s_flash, addr, &r, sizeof(r));
        if (record_valid(&r)) {
            if (r.temp_c10 != INT16_MIN) {
                stats_add(&t, r.temp_c10);
            }
            stats_add(&a, r.adc_mv);
        }
    }
    io_printf("%lu records, %lu bytes used of %lu" IO_NEWLINE, (unsigned long)s_next_seq,
              (unsigned long)(s_next_addr - LOG_AREA_START), (unsigned long)LOG_AREA_SIZE);
    stats_print(&t, "temp 0.1C");
    stats_print(&a, "adc mV  ");
}

int main(void)
{
    const hal_spi_config_t spi_cfg = { 8000000U, HAL_SPI_MODE0, false };

    hal_sys_init();
    hal_time_init();
    hal_console_init(BOARD_CONSOLE_BAUD);
    io_init(hal_console_putc, hal_console_getc);
    log_init(hal_time_ms);
#ifdef BOARD_PC
    sim_devices_install_default();
    sim_adc_wave(ADC_CHANNEL, SIM_WAVE_SINE, 2048U, 600U, 20000U);
#endif
    (void)hal_adc_init();
    (void)hal_i2c_init(HAL_I2C1, 100000U);
    (void)hal_spi_init(HAL_SPI1, &spi_cfg);
    if (w25q_init(&s_flash, HAL_SPI1, PA(4)) != HAL_OK) {
        LOG_E("log", "no SPI flash found");
        for (;;) {
            hal_sys_idle();
        }
    }
    LOG_I("log", "flash %s, JEDEC %02X %02X %02X", w25q_name(&s_flash),
          s_flash.jedec[0], s_flash.jedec[1], s_flash.jedec[2]);
    find_log_end();
    LOG_I("log", "%lu records found, logging every %lu ms (d=dump s=stats e=erase)",
          (unsigned long)s_next_seq, (unsigned long)LOG_PERIOD_MS);

    uint32_t next = hal_time_ms();
    for (;;) {
        if ((int32_t)(hal_time_ms() - next) >= 0) {
            next += LOG_PERIOD_MS;
            take_sample();
        }
        const int c = io_getc();
        if (c == 'd') {
            dump_csv();
        } else if (c == 's') {
            print_stats();
        } else if (c == 'e') {
            erase_log();
        } else if ((c == 'q') || (c == IO_CLOSED)) {
            break;
        } else {
            /* keep logging */
        }
        hal_sys_idle();
    }
    print_stats();
#ifdef BOARD_PC
    sim_shutdown(0);
#endif
    return 0;
}
