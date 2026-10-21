/**
 * @file    cmd_dev.c
 * @brief   Commands for the external chips (drivers/):
 *
 *   eeprom read 0 32 | eeprom write 0x10 "hello" | eeprom fill 0xFF | eeprom test
 *   flash id | flash read 0 64 | flash write 0 "data" | flash erase 0
 *   temp                    LM75 on I2C1 (and DS18B20 on PB5 if present)
 *   rtcx [YYYY-MM-DD HH:MM:SS]   external DS1307
 *   config list | config set 3 115200 | config str 4 "name" | config del 3 | config format
 *   ow scan [pin]           1-Wire ROM search
 *   play <song|notes...>    melody on a PWM buzzer
 *   oled text "hi" | oled demo | oled clear | oled contrast 50
 *   nmea "$GPRMC,...*6A"    parse a sentence and show the fix
 *   geo 47.2378 6.0241 47.2466 6.0219    distance and bearing
 */
#include "cli_cmds.h"
#include "hal.h"
#include "io.h"
#include "hexdump.h"
#include "strutil.h"
#include "fixmath.h"
#include "eeprom24.h"
#include "w25q.h"
#include "sensors_i2c.h"
#include "config.h"
#include "onewire.h"
#include "melody.h"
#include "ssd1306.h"
#include "nmea.h"
#include "geo.h"
#include "fmt.h"

#define DEV_I2C          HAL_I2C1
#define DEV_EEPROM_ADDR  (0x50U)
#define DEV_EEPROM_KBIT  (2U)
#define DEV_SPI          HAL_SPI1
#define DEV_FLASH_CS     PA(4)
#define DEV_OW_PIN       PB(5)
#define DEV_BUZZER       HAL_PWM_T4C3     /* PB8 */
#define DEV_CHUNK        (16U)
#define DEV_MAX_LEN      (512U)

static int report(hal_status_t st)
{
    return (st == HAL_OK) ? CLI_OK : cli_error("%s", hal_status_str(st));
}

/* bytes from numbers or a string, like the crc command */
static uint32_t collect(int argc, char *argv[], uint8_t *buf, uint32_t max)
{
    uint32_t n = 0U;
    for (int i = 0; (i < argc) && (n < max); i++) {
        uint32_t v;
        if (str_to_u32(argv[i], &v) && (v <= 0xFFU)) {
            buf[n++] = (uint8_t)v;
        } else {
            for (const char *p = argv[i]; (*p != '\0') && (n < max); p++) {
                buf[n++] = (uint8_t)*p;
            }
        }
    }
    return n;
}

/* ------------------------------ eeprom ---------------------------- */

static eeprom24_t s_ee;

static bool ee_ready(void)
{
    static bool done;
    if (!done) {
        (void)hal_i2c_init(DEV_I2C, 100000U);
        (void)eeprom24_init(&s_ee, DEV_I2C, DEV_EEPROM_ADDR, DEV_EEPROM_KBIT);
        done = true;
    }
    if (!hal_i2c_probe(DEV_I2C, DEV_EEPROM_ADDR)) {
        (void)cli_error("no EEPROM at 0x%02X on I2C%u", DEV_EEPROM_ADDR, (unsigned)DEV_I2C + 1U);
        return false;
    }
    return true;
}

static int sub_ee_read(int argc, char *argv[])
{
    uint32_t addr = 0U;
    uint32_t len  = 64U;
    uint8_t  buf[DEV_CHUNK];
    if (((argc >= 2) && !cli_arg_u32(argv[1], &addr)) || ((argc >= 3) && !cli_arg_u32_range(argv[2], 1U, DEV_MAX_LEN, &len))) {
        return CLI_ERR_USAGE;
    }
    if (!ee_ready()) {
        return CLI_ERR;
    }
    for (uint32_t off = 0U; off < len; off += DEV_CHUNK) {
        const uint32_t n = ((len - off) < DEV_CHUNK) ? (len - off) : DEV_CHUNK;
        const hal_status_t st = eeprom24_read(&s_ee, addr + off, buf, n);
        if (st != HAL_OK) {
            return report(st);
        }
        hexdump(buf, n, addr + off);
    }
    return CLI_OK;
}

static int sub_ee_write(int argc, char *argv[])
{
    uint32_t addr;
    uint8_t  buf[64];
    if ((argc < 3) || !cli_arg_u32(argv[1], &addr)) {
        return CLI_ERR_USAGE;
    }
    if (!ee_ready()) {
        return CLI_ERR;
    }
    const uint32_t n = collect(argc - 2, &argv[2], buf, sizeof(buf));
    const hal_status_t st = eeprom24_write(&s_ee, addr, buf, n);
    if (st == HAL_OK) {
        io_printf("%lu bytes written at 0x%03lX" IO_NEWLINE, (unsigned long)n, (unsigned long)addr);
    }
    return report(st);
}

static int sub_ee_fill(int argc, char *argv[])
{
    uint32_t v = 0xFFU;
    if ((argc >= 2) && !cli_arg_u32_range(argv[1], 0U, 0xFFU, &v)) {
        return CLI_ERR_USAGE;
    }
    if (!ee_ready()) {
        return CLI_ERR;
    }
    return report(eeprom24_fill(&s_ee, 0U, (uint8_t)v, s_ee.size));
}

static int sub_ee_test(int argc, char *argv[])
{
    uint8_t pattern[DEV_CHUNK];
    uint8_t back[DEV_CHUNK];
    (void)argc;
    (void)argv;
    if (!ee_ready()) {
        return CLI_ERR;
    }
    /* address-in-data pattern catches wrap-around and stuck bits */
    for (uint32_t base = 0U; base < s_ee.size; base += DEV_CHUNK) {
        for (uint32_t i = 0U; i < DEV_CHUNK; i++) {
            pattern[i] = (uint8_t)((base + i) ^ 0x5AU);
        }
        hal_status_t st = eeprom24_write(&s_ee, base, pattern, DEV_CHUNK);
        if (st == HAL_OK) {
            st = eeprom24_read(&s_ee, base, back, DEV_CHUNK);
        }
        if (st != HAL_OK) {
            return report(st);
        }
        for (uint32_t i = 0U; i < DEV_CHUNK; i++) {
            if (back[i] != pattern[i]) {
                return cli_error("mismatch at 0x%03lX: wrote %02X read %02X", (unsigned long)(base + i),
                                 pattern[i], back[i]);
            }
        }
    }
    io_printf("EEPROM test passed (%lu bytes)" IO_NEWLINE, (unsigned long)s_ee.size);
    return CLI_OK;
}

static const cli_subcmd_t ee_subs[] = {
    { "read",  sub_ee_read,  "[addr] [len]",         "hexdump" },
    { "write", sub_ee_write, "<addr> <bytes|text>",  "write (page splitting automatic)" },
    { "fill",  sub_ee_fill,  "[value]",              "fill the whole chip" },
    { "test",  sub_ee_test,  "",                     "write/verify every byte" },
    { NULL, NULL, NULL, NULL }
};

/* --------------------------- SPI flash ---------------------------- */

static w25q_t s_flash;

static bool flash_ready(void)
{
    const hal_spi_config_t cfg = { 8000000U, HAL_SPI_MODE0, false };
    (void)hal_spi_init(DEV_SPI, &cfg);
    const hal_status_t st = w25q_init(&s_flash, DEV_SPI, DEV_FLASH_CS);
    if (st != HAL_OK) {
        (void)cli_error("no SPI flash on SPI%u CS PA4 (%s)", (unsigned)DEV_SPI + 1U, hal_status_str(st));
        return false;
    }
    return true;
}

static int sub_fl_id(int argc, char *argv[])
{
    uint8_t uid[8];
    (void)argc;
    (void)argv;
    if (!flash_ready()) {
        return CLI_ERR;
    }
    w25q_unique_id(&s_flash, uid);
    io_printf("JEDEC %02X %02X %02X  %s" IO_NEWLINE, s_flash.jedec[0], s_flash.jedec[1], s_flash.jedec[2],
              w25q_name(&s_flash));
    io_puts("unique id ");
    for (uint32_t i = 0U; i < 8U; i++) {
        io_printf("%02X", uid[i]);
    }
    io_puts(IO_NEWLINE);
    return CLI_OK;
}

static int sub_fl_read(int argc, char *argv[])
{
    uint32_t addr = 0U;
    uint32_t len  = 64U;
    uint8_t  buf[DEV_CHUNK];
    if (((argc >= 2) && !cli_arg_u32(argv[1], &addr)) || ((argc >= 3) && !cli_arg_u32_range(argv[2], 1U, DEV_MAX_LEN, &len))) {
        return CLI_ERR_USAGE;
    }
    if (!flash_ready()) {
        return CLI_ERR;
    }
    for (uint32_t off = 0U; off < len; off += DEV_CHUNK) {
        const uint32_t n = ((len - off) < DEV_CHUNK) ? (len - off) : DEV_CHUNK;
        const hal_status_t st = w25q_read(&s_flash, addr + off, buf, n);
        if (st != HAL_OK) {
            return report(st);
        }
        hexdump(buf, n, addr + off);
    }
    return CLI_OK;
}

static int sub_fl_write(int argc, char *argv[])
{
    uint32_t addr;
    uint8_t  buf[64];
    if ((argc < 3) || !cli_arg_u32(argv[1], &addr)) {
        return CLI_ERR_USAGE;
    }
    if (!flash_ready()) {
        return CLI_ERR;
    }
    const uint32_t n = collect(argc - 2, &argv[2], buf, sizeof(buf));
    return report(w25q_write(&s_flash, addr, buf, n));
}

static int sub_fl_erase(int argc, char *argv[])
{
    uint32_t addr;
    if (argc != 2) {
        return CLI_ERR_USAGE;
    }
    if (!flash_ready()) {
        return CLI_ERR;
    }
    if (str_eq(argv[1], "chip")) {
        io_println("chip erase (can take a while)...");
        return report(w25q_erase_chip(&s_flash));
    }
    if (!cli_arg_u32(argv[1], &addr)) {
        return CLI_ERR;
    }
    const hal_status_t st = w25q_erase_sector(&s_flash, addr);
    if (st == HAL_OK) {
        io_printf("sector 0x%06lX erased" IO_NEWLINE, (unsigned long)(addr & ~(W25Q_SECTOR_SIZE - 1U)));
    }
    return report(st);
}

static const cli_subcmd_t fl_subs[] = {
    { "id",    sub_fl_id,    "",                     "JEDEC + unique ID" },
    { "read",  sub_fl_read,  "[addr] [len]",         "hexdump" },
    { "write", sub_fl_write, "<addr> <bytes|text>",  "program (area must be erased)" },
    { "erase", sub_fl_erase, "<addr|chip>",          "erase a 4 KiB sector or all" },
    { NULL, NULL, NULL, NULL }
};

/* ---------------------------- sensors ----------------------------- */

static int cmd_temp(int argc, char *argv[])
{
    int32_t c10;
    int32_t c100;
    bool    any = false;
    (void)argc;
    (void)argv;

    (void)hal_i2c_init(DEV_I2C, 100000U);
    if (lm75_read_c10(DEV_I2C, LM75_ADDR_DEFAULT, &c10) == HAL_OK) {
        io_printf("LM75    %ld.%ld C" IO_NEWLINE, (long)(c10 / 10), (long)fx_abs(c10 % 10));
        any = true;
    }
    onewire_t ow;
    ow_init(&ow, DEV_OW_PIN);
    if (ow_reset(&ow) && (ds18b20_measure(&ow, &c100) == HAL_OK)) {
        io_printf("DS18B20 %ld.%02ld C" IO_NEWLINE, (long)(c100 / 100), (long)fx_abs(c100 % 100));
        any = true;
    }
    (void)hal_adc_init();
    c10 = hal_adc_temp_c10();
    io_printf("MCU die %ld.%ld C" IO_NEWLINE, (long)(c10 / 10), (long)fx_abs(c10 % 10));
    if (!any) {
        io_println("(no LM75 at 0x48 on I2C1, no DS18B20 on PB5)");
    }
    return CLI_OK;
}

static int cmd_rtcx(int argc, char *argv[])
{
    datetime_t dt = { 2026U, 1U, 1U, 0U, 0U, 0U, 0U };
    char       buf[24];
    (void)hal_i2c_init(DEV_I2C, 100000U);
    if (argc >= 2) {
        char text[24] = "";
        for (int i = 1; (i < argc) && (i < 3); i++) {
            (void)str_cat(text, argv[i], sizeof(text));
            (void)str_cat(text, (i < (argc - 1)) ? " " : "", sizeof(text));
        }
        if (!dt_parse(text, &dt)) {
            return cli_error("format: YYYY-MM-DD HH:MM:SS");
        }
        const hal_status_t st = ds1307_set(DEV_I2C, &dt);
        if (st != HAL_OK) {
            return report(st);
        }
    }
    const hal_status_t st = ds1307_get(DEV_I2C, &dt);
    if (st != HAL_OK) {
        return cli_error("DS1307: %s", hal_status_str(st));
    }
    (void)dt_format(&dt, buf, sizeof(buf));
    io_printf("DS1307 %s %s" IO_NEWLINE, buf, dt_weekday_name(dt.weekday));
    return CLI_OK;
}

/* ----------------------------- config ----------------------------- */

static void print_entry(uint8_t key, const uint8_t *value, uint8_t len, void *arg)
{
    bool printable = true;
    (void)arg;
    for (uint32_t i = 0U; i < len; i++) {
        printable = printable && chr_is_print((char)value[i]);
    }
    io_printf("  %2u  len %2u  ", (unsigned)key, (unsigned)len);
    if (printable) {
        io_printf("\"%.*s\"", (int)len, (const char *)value);
    } else if (len == 4U) {
        const uint32_t v = (uint32_t)value[0] | ((uint32_t)value[1] << 8) |
                           ((uint32_t)value[2] << 16) | ((uint32_t)value[3] << 24);
        io_printf("%lu (0x%lX)", (unsigned long)v, (unsigned long)v);
    } else {
        for (uint32_t i = 0U; i < len; i++) {
            io_printf("%02X ", value[i]);
        }
    }
    io_puts(IO_NEWLINE);
}

static bool parse_key(const char *s, uint8_t *key)
{
    uint32_t k;
    if (!cli_arg_u32_range(s, 1U, CONFIG_KEYS_MAX - 1U, &k)) {
        return false;
    }
    *key = (uint8_t)k;
    return true;
}

static int cmd_config(int argc, char *argv[])
{
    uint8_t  key;
    uint32_t v;
    if (config_init() != HAL_OK) {
        return cli_error("flash error");
    }
    if ((argc == 1) || str_eq(argv[1], "list")) {
        config_foreach(print_entry, NULL);
        io_printf("used %lu / %lu bytes, generation %lu" IO_NEWLINE, (unsigned long)config_used(),
                  (unsigned long)config_capacity(), (unsigned long)config_generation());
        return CLI_OK;
    }
    if (str_eq(argv[1], "format")) {
        return report(config_format());
    }
    if ((argc >= 3) && str_eq(argv[1], "del") && parse_key(argv[2], &key)) {
        return report(config_delete(key));
    }
    if ((argc == 4) && str_eq(argv[1], "set") && parse_key(argv[2], &key) && cli_arg_u32(argv[3], &v)) {
        return report(config_set_u32(key, v));
    }
    if ((argc == 4) && str_eq(argv[1], "str") && parse_key(argv[2], &key)) {
        return report(config_set_str(key, argv[3]));
    }
    return CLI_ERR_USAGE;
}

/* ---------------------------- 1-Wire ------------------------------ */

static int cmd_ow(int argc, char *argv[])
{
    hal_pin_t pin = DEV_OW_PIN;
    onewire_t ow;
    uint8_t   rom[OW_ROM_SIZE];
    uint32_t  n = 0U;
    if ((argc < 2) || !str_eq(argv[1], "scan")) {
        return CLI_ERR_USAGE;
    }
    if ((argc >= 3) && !hal_gpio_parse(argv[2], &pin)) {
        return cli_error("bad pin");
    }
    ow_init(&ow, pin);
    if (!ow_reset(&ow)) {
        return cli_error("no presence pulse (4.7k pull-up fitted?)");
    }
    for (bool ok = ow_search_first(&ow, rom); ok; ok = ow_search_next(&ow, rom)) {
        io_printf("  %lu: ", (unsigned long)n++);
        for (uint32_t i = 0U; i < OW_ROM_SIZE; i++) {
            io_printf("%02X", rom[i]);
        }
        io_printf("%s" IO_NEWLINE, (rom[0] == DS18B20_FAMILY) ? "  DS18B20" : "");
    }
    io_printf("%lu device(s)" IO_NEWLINE, (unsigned long)n);
    return CLI_OK;
}

/* ----------------------------- melody ----------------------------- */

static int cmd_play(int argc, char *argv[])
{
    char     notes[CLI_LINE_MAX] = "";
    melody_t m;
    if (argc < 2) {
        io_puts("songs:");
        for (uint32_t i = 0U; melody_song_names[i] != NULL; i++) {
            io_printf(" %s", melody_song_names[i]);
        }
        io_puts(IO_NEWLINE);
        return CLI_ERR_USAGE;
    }
    const char *song = NULL;
    for (uint32_t i = 0U; melody_song_names[i] != NULL; i++) {
        if (str_eq(argv[1], melody_song_names[i])) {
            song = melody_songs[i];
        }
    }
    if (song == NULL) {
        for (int i = 1; i < argc; i++) {
            (void)str_cat(notes, argv[i], sizeof(notes));
            (void)str_cat(notes, " ", sizeof(notes));
        }
        song = notes;
    }
    melody_init(&m, DEV_BUZZER);
    melody_play(&m, song, 140U);
    while (melody_is_playing(&m) && !cli_abort_requested()) {
        melody_update(&m, hal_time_ms());
    }
    melody_stop(&m);
    return CLI_OK;
}


#if CLI_WITH_DISPLAY
/* ------------------------------ OLED ------------------------------ */

static ssd1306_t s_oled;
static bool      s_oled_ready;

static bool oled_ready(void)
{
    if (!s_oled_ready) {
        (void)hal_i2c_init(DEV_I2C, 400000U);
        if (ssd1306_init(&s_oled, DEV_I2C, SSD1306_ADDR, 64) != HAL_OK) {
            (void)cli_error("no SSD1306 at 0x3C on I2C1");
            return false;
        }
        s_oled_ready = true;
    }
    return true;
}

static void oled_demo(gfx_t *g)
{
    gfx_clear(g);
    gfx_rect(g, 0, 0, 128, 64, GFX_SET);
    gfx_set_text_scale(g, 2U);
    gfx_set_cursor(g, 16, 4);
    gfx_puts(g, "ELISE");
    gfx_set_text_scale(g, 1U);
    gfx_set_cursor(g, 16, 24);
    gfx_puts(g, "STM32 bare metal");
    gfx_circle(g, 100, 14, 9, GFX_SET);
    gfx_line(g, 91, 14, 109, 14, GFX_SET);
    gfx_line(g, 100, 5, 100, 23, GFX_SET);
    gfx_progress(g, 8, 40, 112, 10, 650U);
    gfx_set_cursor(g, 8, 54);
    gfx_printf(g, "uptime %lu s", (unsigned long)(hal_time_ms() / 1000U));
}

static int cmd_oled(int argc, char *argv[])
{
    uint32_t v;
    if (argc < 2) {
        return CLI_ERR_USAGE;
    }
    if (!oled_ready()) {
        return CLI_ERR;
    }
    gfx_t *g = &s_oled.gfx;
    if (str_eq(argv[1], "clear")) {
        gfx_clear(g);
    } else if (str_eq(argv[1], "demo")) {
        oled_demo(g);
    } else if (str_eq(argv[1], "text") && (argc >= 3)) {
        for (int i = 2; i < argc; i++) {
            gfx_puts(g, argv[i]);
            gfx_putc(g, (i < (argc - 1)) ? ' ' : '\n');
        }
    } else if (str_eq(argv[1], "contrast") && (argc == 3) && cli_arg_u32_range(argv[2], 0U, 255U, &v)) {
        return report(ssd1306_contrast(&s_oled, (uint8_t)v));
    } else if (str_eq(argv[1], "invert") && (argc == 3)) {
        return report(ssd1306_invert(&s_oled, str_eq(argv[2], "on")));
    } else {
        return CLI_ERR_USAGE;
    }
    return report(ssd1306_flush(&s_oled));
}

#endif /* CLI_WITH_DISPLAY */

#if CLI_WITH_GPS
/* ------------------------------ GPS ------------------------------- */

static void print_udeg(int32_t udeg)
{
    const uint32_t mag = (udeg < 0) ? (0U - (uint32_t)udeg) : (uint32_t)udeg;
    io_printf("%s%lu.%06lu", (udeg < 0) ? "-" : "", (unsigned long)(mag / 1000000U),
              (unsigned long)(mag % 1000000U));
}

static int cmd_nmea(int argc, char *argv[])
{
    static nmea_t n;
    static bool   init;
    if (argc != 2) {
        return CLI_ERR_USAGE;
    }
    if (!init) {
        nmea_init(&n);
        init = true;
    }
    const nmea_sentence_t t = nmea_parse(&n, argv[1]);
    if (t == NMEA_SENTENCE_BAD_CRC) {
        return cli_error("bad checksum or missing '*hh'");
    }
    const nmea_fix_t *f = &n.fix;
    io_printf("fix %s  q%u  sats %u  hdop %u.%02u" IO_NEWLINE, f->valid ? "valid" : "none",
              (unsigned)f->quality, (unsigned)f->satellites, (unsigned)(f->hdop_x100 / 100U),
              (unsigned)(f->hdop_x100 % 100U));
    io_puts("lat ");
    print_udeg(f->lat_udeg);
    io_puts("  lon ");
    print_udeg(f->lon_udeg);
    io_printf("  alt %ld.%ld m" IO_NEWLINE, (long)(f->alt_dm / 10), (long)fx_abs(f->alt_dm % 10));
    io_printf("speed %lu.%02lu km/h  course %lu.%02lu deg  %02u:%02u:%02u UTC %04u-%02u-%02u" IO_NEWLINE,
              (unsigned long)((f->speed_mmps * 36U) / 10000U), (unsigned long)(((f->speed_mmps * 36U) / 100U) % 100U),
              (unsigned long)(f->course_cdeg / 100U), (unsigned long)(f->course_cdeg % 100U),
              (unsigned)f->hour, (unsigned)f->minute, (unsigned)f->second,
              (unsigned)f->year, (unsigned)f->month, (unsigned)f->day);
    return CLI_OK;
}

static bool parse_deg(const char *s, int32_t *udeg)
{
    if (!str_to_fixed(s, 1000000U, udeg)) {
        (void)cli_error("'%s' is not decimal degrees", s);
        return false;
    }
    return true;
}

static int cmd_geo(int argc, char *argv[])
{
    geo_point_t a;
    geo_point_t b;
    if ((argc != 5) || !parse_deg(argv[1], &a.lat_udeg) || !parse_deg(argv[2], &a.lon_udeg) ||
        !parse_deg(argv[3], &b.lat_udeg) || !parse_deg(argv[4], &b.lon_udeg)) {
        return CLI_ERR_USAGE;
    }
    const uint32_t d  = geo_distance_m(&a, &b);
    const uint32_t br = geo_bearing_cdeg(&a, &b);
    io_printf("distance %lu m, bearing %lu.%02lu deg (%s)" IO_NEWLINE, (unsigned long)d,
              (unsigned long)(br / 100U), (unsigned long)(br % 100U), geo_compass(br));
    return CLI_OK;
}

#endif /* CLI_WITH_GPS */

static const cli_command_t dev_cmds[] = {
    { "eeprom", NULL,       "<sub> ...",       "24C02 EEPROM on I2C1 @0x50",       ee_subs },
    { "flash",  NULL,       "<sub> ...",       "W25Qxx SPI flash on SPI1, CS PA4", fl_subs },
    { "temp",   cmd_temp,   "",                "LM75 / DS18B20 / MCU temperature", NULL },
    { "rtcx",   cmd_rtcx,   "[date time]",     "external DS1307 RTC",              NULL },
    { "config", cmd_config, "[list|set k v|str k s|del k|format]", "settings in internal flash", NULL },
    { "ow",     cmd_ow,     "scan [pin]",      "1-Wire ROM search",                NULL },
    { "play",   cmd_play,   "<song|notes...>", "melody on buzzer (T4C3 = PB8)",    NULL },
#if CLI_WITH_DISPLAY
    { "oled",   cmd_oled,   "<clear|demo|text ...|contrast n|invert on|off>", "SSD1306 OLED on I2C1", NULL },
#endif
#if CLI_WITH_GPS
    { "nmea",   cmd_nmea,   "<\"$GP...*hh\">", "parse an NMEA sentence",            NULL },
    { "geo",    cmd_geo,    "<lat1> <lon1> <lat2> <lon2>", "distance and bearing",     NULL },
#endif
};

static const cli_group_t dev_group = { "devices", dev_cmds, sizeof(dev_cmds) / sizeof(dev_cmds[0]) };

const cli_group_t *cmd_dev_group(void)
{
    return &dev_group;
}
