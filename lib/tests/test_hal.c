/**
 * @file    test_hal.c
 * @brief   Tests of the HAL helpers, the PC port and the device
 *          drivers, running against the simulated chips.
 */
#include "unit.h"

#include "hal.h"
#include "sim.h"
#include "io.h"
#include "expr.h"
#include "datetime.h"
#include "crc.h"
#include "eeprom24.h"
#include "w25q.h"
#include "sensors_i2c.h"
#include "config.h"
#include "encoder.h"
#include "onewire.h"
#include "melody.h"
#include "led.h"
#include "shiftreg.h"
#include "cli.h"
#include "cli_cmds.h"

#include <stdio.h>

int unit_checks;
int unit_failures;

static char   s_out[8192];
static size_t s_len;

static void cap_putc(char c)
{
    if (s_len < (sizeof(s_out) - 1U)) {
        s_out[s_len++] = c;
        s_out[s_len]   = '\0';
    }
}

static void cap_reset(void)
{
    s_len    = 0U;
    s_out[0] = '\0';
}

/* ------------------------------------------------------------------ */

TEST(hal_helpers)
{
    uint16_t  psc;
    uint16_t  arr;
    hal_pin_t pin;
    char      name[6];

    CHECK(hal_timer_calc(24000000U, 1000U, &psc, &arr));
    CHECK_EQ_INT(psc, 0);
    CHECK_EQ_INT(arr, 23999);
    CHECK(hal_timer_calc(72000000U, 1U, &psc, &arr));
    CHECK_EQ_INT(((uint32_t)psc + 1U) * ((uint32_t)arr + 1U), 72000000U);
    CHECK(hal_timer_calc(24000000U, 50U, &psc, &arr));           /* servo */
    CHECK_EQ_INT(24000000U / (((uint32_t)psc + 1U) * ((uint32_t)arr + 1U)), 50);
    CHECK(!hal_timer_calc(24000000U, 0U, &psc, &arr));
    CHECK(!hal_timer_calc(72000000U, 0U, &psc, &arr));
    CHECK_EQ_INT(hal_spi_calc_div_log2(24000000U, 1000000U), 5);  /* /32 = 750 kHz */
    CHECK_EQ_INT(hal_spi_calc_div_log2(24000000U, 12000000U), 1);
    CHECK_EQ_INT(hal_spi_calc_div_log2(72000000U, 1000U), 0);
    CHECK_EQ_INT(hal_adc_raw_to_mv(4095U, 3300U), 3300);
    CHECK_EQ_INT(hal_adc_raw_to_mv(2048U, 3300U), 1650);
    CHECK_EQ_INT(hal_adc_mv_to_temp_c10(1430U), 250);
    CHECK_EQ_INT(hal_adc_mv_to_temp_c10(1387U), 350);
    CHECK(hal_gpio_parse("PC13", &pin) && (pin == PC(13)));
    CHECK(hal_gpio_parse("a5", &pin) && (pin == PA(5)));
    CHECK(!hal_gpio_parse("PZ1", &pin));
    CHECK(!hal_gpio_parse("PA16", &pin));
    CHECK_EQ_STR(hal_gpio_name(PB(12), name), "PB12");
    CHECK_EQ_STR(hal_gpio_name(PA(0), name), "PA0");
}

TEST(gpio_sim)
{
    uint32_t v = 0U;
    hal_gpio_mode(PC(8), HAL_GPIO_OUTPUT);
    hal_gpio_set(PC(8));
    CHECK(hal_gpio_read(PC(8)));
    CHECK(hal_sys_mem_read32(0x4001100CU, &v));        /* GPIOC_ODR */
    CHECK((v & (1U << 8)) != 0U);
    CHECK(hal_sys_mem_write32(0x40011010U, 1U << 24));  /* BSRR reset bit 8 */
    CHECK(!hal_gpio_read_output(PC(8)));
    CHECK(hal_sys_mem_read32(0x40011004U, &v));        /* CRH: pin 8 = 0x2 */
    CHECK_EQ_INT(v & 0xFU, 0x2);
    hal_gpio_port_write(HAL_PORT_C, 0x0300U, 0x0100U);  /* set wins */
    CHECK_EQ_INT(hal_gpio_port_read_output(HAL_PORT_C) & 0x0300U, 0x0300);

    hal_gpio_mode(PA(1), HAL_GPIO_INPUT_PULLUP);
    CHECK(hal_gpio_read(PA(1)));
    sim_gpio_input(PA(1), false);
    CHECK(!hal_gpio_read(PA(1)));
    sim_gpio_release(PA(1));
    CHECK(hal_gpio_read(PA(1)));
    hal_gpio_mode(PA(1), HAL_GPIO_INPUT_PULLDOWN);
    CHECK(!hal_gpio_read(PA(1)));
}

static volatile uint32_t s_exti_hits;
static void on_edge(hal_pin_t pin, void *arg) { (void)pin; (void)arg; s_exti_hits++; }

TEST(exti_sim)
{
    hal_gpio_mode(PA(2), HAL_GPIO_INPUT_PULLDOWN);
    CHECK_EQ_INT(hal_exti_attach(PA(2), HAL_EXTI_FALLING, on_edge, NULL), HAL_OK);
    CHECK_EQ_INT(hal_exti_attach(PB(2), HAL_EXTI_FALLING, on_edge, NULL), HAL_EBUSY);
    sim_gpio_input(PA(2), true);
    CHECK_EQ_INT(s_exti_hits, 0);
    sim_gpio_input(PA(2), false);
    CHECK_EQ_INT(s_exti_hits, 1);
    hal_exti_enable(PA(2), false);
    sim_gpio_input(PA(2), true);
    sim_gpio_input(PA(2), false);
    CHECK_EQ_INT(s_exti_hits, 1);
    CHECK_EQ_INT(hal_exti_count(PA(2)), 1);
    hal_exti_detach(PA(2));
}

static volatile uint32_t s_timer_hits;
static void on_tick(hal_timer_t t, void *arg) { (void)t; (void)arg; s_timer_hits++; }

TEST(timer_sim)
{
    s_timer_hits = 0U;
    CHECK_EQ_INT(hal_timer_start_periodic(HAL_TIMER4, 2000U, on_tick, NULL), HAL_OK);
    hal_delay_ms(100U);
    hal_timer_stop(HAL_TIMER4);
    CHECK((s_timer_hits >= 190U) && (s_timer_hits <= 210U));
    const uint32_t t0 = hal_time_ms();
    hal_delay_ms(20U);
    CHECK((hal_time_ms() - t0) >= 20U);
    CHECK((hal_time_us() - (t0 * 1000U)) >= 20000U);
}

TEST(adc_sim)
{
    (void)hal_adc_init();
    sim_adc_set(3U, 1000U);
    CHECK_EQ_INT(hal_adc_read(3U), 1000);
    CHECK_EQ_INT(hal_gpio_get_mode(PA(3)), HAL_GPIO_ANALOG);
    CHECK_NEAR_INT(hal_adc_vdda_mv(), 3300, 5);
    sim_set_temperature_c10(400);
    CHECK_NEAR_INT(hal_adc_temp_c10(), 400, 10);
    sim_set_temperature_c10(250);
    CHECK_EQ_INT(hal_dac_init(HAL_DAC1), HAL_OK);
    CHECK_EQ_INT(hal_dac_write_mv(HAL_DAC1, 1650U), HAL_OK);
    CHECK_NEAR_INT(hal_adc_read(4U), 2048, 2);           /* DAC1 PA4 -> ADC ch4 */
    sim_adc_wave(5U, SIM_WAVE_SQUARE, 2000U, 500U, 1000U);
    const uint16_t a = hal_adc_read(5U);
    CHECK((a == 1500U) || (a == 2500U));
}

TEST(pwm_sim)
{
    CHECK_EQ_INT(hal_pwm_init(HAL_PWM_T4C1, 1000U), HAL_OK);
    hal_pwm_set_duty(HAL_PWM_T4C1, 250U);
    CHECK_EQ_INT(hal_pwm_get_duty(HAL_PWM_T4C1), 250);
    CHECK_EQ_INT(hal_pwm_get_frequency(HAL_PWM_T4C1), 1000);
    CHECK_EQ_INT(hal_pwm_pin(HAL_PWM_T4C1), PB(6));
    CHECK_EQ_INT(hal_gpio_get_mode(PB(6)), HAL_GPIO_AF);
    hal_pwm_stop(HAL_PWM_T4C1);
}

TEST(eeprom)
{
    eeprom24_t ee;
    uint8_t    buf[64];
    uint8_t    back[64];

    (void)hal_i2c_init(HAL_I2C1, 100000U);
    CHECK(!eeprom24_init(&ee, HAL_I2C1, 0x50U, 3U));
    CHECK(eeprom24_init(&ee, HAL_I2C1, 0x50U, 2U));
    CHECK_EQ_INT(ee.size, 256);
    CHECK_EQ_INT(ee.page_size, 8);
    /* write across 4 page borders from an odd address */
    for (uint32_t i = 0U; i < sizeof(buf); i++) {
        buf[i] = (uint8_t)(i * 7U + 3U);
    }
    CHECK_EQ_INT(eeprom24_write(&ee, 5U, buf, 40U), HAL_OK);
    CHECK_EQ_INT(eeprom24_read(&ee, 5U, back, 40U), HAL_OK);
    CHECK(memcmp(buf, back, 40U) == 0);
    /* sequential read wrapping the whole chip */
    CHECK_EQ_INT(eeprom24_read(&ee, 250U, back, 10U), HAL_EINVAL);
    CHECK_EQ_INT(eeprom24_fill(&ee, 200U, 0xA5U, 56U), HAL_OK);
    CHECK_EQ_INT(eeprom24_read(&ee, 200U, back, 56U), HAL_OK);
    CHECK((back[0] == 0xA5U) && (back[55] == 0xA5U));
    /* nobody at 0x51 */
    eeprom24_t ghost;
    (void)eeprom24_init(&ghost, HAL_I2C1, 0x51U, 2U);
    CHECK_EQ_INT(eeprom24_read(&ghost, 0U, back, 1U), HAL_ETIMEOUT);
}

TEST(spi_flash)
{
    w25q_t f;
    uint8_t buf[600];
    uint8_t back[600];
    const hal_spi_config_t cfg = { 8000000U, HAL_SPI_MODE0, false };

    (void)hal_spi_init(HAL_SPI1, &cfg);
    CHECK_EQ_INT(w25q_init(&f, HAL_SPI1, PA(4)), HAL_OK);
    CHECK_EQ_INT(f.size, 2U * 1024U * 1024U);
    CHECK_EQ_INT(f.jedec[0], 0xEF);
    CHECK_EQ_INT(w25q_erase_sector(&f, 0x1000U), HAL_OK);
    for (uint32_t i = 0U; i < sizeof(buf); i++) {
        buf[i] = (uint8_t)(i ^ (i >> 8));
    }
    /* starts mid-page, spans 3 pages */
    CHECK_EQ_INT(w25q_write(&f, 0x1080U, buf, sizeof(buf)), HAL_OK);
    CHECK_EQ_INT(w25q_read(&f, 0x1080U, back, sizeof(back)), HAL_OK);
    CHECK(memcmp(buf, back, sizeof(buf)) == 0);
    /* programming only clears bits */
    const uint8_t zeros[2] = { 0x0FU, 0xF0U };
    CHECK_EQ_INT(w25q_write(&f, 0x1080U, zeros, 2U), HAL_OK);
    CHECK_EQ_INT(w25q_read(&f, 0x1080U, back, 2U), HAL_OK);
    CHECK_EQ_INT(back[0], buf[0] & 0x0FU);
    CHECK_EQ_INT(w25q_erase_sector(&f, 0x1FFFU), HAL_OK);
    CHECK_EQ_INT(w25q_read(&f, 0x1080U, back, 4U), HAL_OK);
    CHECK_EQ_INT(back[0], 0xFF);
    CHECK_EQ_INT(w25q_read(&f, f.size - 1U, back, 2U), HAL_EINVAL);
    w25q_t none;
    CHECK(w25q_init(&none, HAL_SPI1, PB(1)) != HAL_OK);       /* no chip on that CS */
}

TEST(i2c_sensors)
{
    int32_t    c10;
    datetime_t dt = { 2030U, 2U, 28U, 23U, 59U, 50U, 0U };
    datetime_t rd;
    uint8_t    ram[4] = { 1U, 2U, 3U, 4U };
    uint8_t    back[4];

    CHECK_EQ_INT(lm75_raw_to_c10(0x1900U), 250);
    CHECK_EQ_INT(lm75_raw_to_c10(0xFF80U), -5);
    CHECK_EQ_INT(lm75_raw_to_c10(0xE700U), -250);
    CHECK_EQ_INT(lm75_c10_to_raw(-250), 0xE700);
    sim_set_temperature_c10(-127);
    CHECK_EQ_INT(lm75_read_c10(HAL_I2C1, 0x48U, &c10), HAL_OK);
    CHECK_EQ_INT(c10, -125);                      /* 0.5 C resolution */
    sim_set_temperature_c10(250);
    CHECK_EQ_INT(lm75_set_limits(HAL_I2C1, 0x48U, 700, 800), HAL_OK);

    CHECK_EQ_INT(ds1307_set(HAL_I2C1, &dt), HAL_OK);
    CHECK_EQ_INT(ds1307_get(HAL_I2C1, &rd), HAL_OK);
    CHECK_EQ_INT(rd.year, 2030);
    CHECK_EQ_INT(rd.month, 2);
    CHECK_EQ_INT(rd.day, 28);
    CHECK_EQ_INT(rd.hour, 23);
    CHECK_EQ_INT(rd.weekday, 4);                  /* 2030-02-28 is a Thursday */
    CHECK_EQ_INT(ds1307_ram_write(HAL_I2C1, 10U, ram, 4U), HAL_OK);
    CHECK_EQ_INT(ds1307_ram_read(HAL_I2C1, 10U, back, 4U), HAL_OK);
    CHECK(memcmp(ram, back, 4U) == 0);
    CHECK_EQ_INT(ds1307_ram_write(HAL_I2C1, 54U, ram, 4U), HAL_EINVAL);
}

TEST(config_store)
{
    char     s[16];
    uint32_t big = 0U;

    CHECK_EQ_INT(config_format(), HAL_OK);
    CHECK_EQ_INT(config_get_u32(1U, 77U), 77);             /* default */
    CHECK_EQ_INT(config_set_u32(1U, 115200U), HAL_OK);
    CHECK_EQ_INT(config_get_u32(1U, 0U), 115200);
    CHECK_EQ_INT(config_set_str(2U, "elise"), HAL_OK);
    CHECK(config_get_str(2U, s, sizeof(s)));
    CHECK_EQ_STR(s, "elise");
    const uint32_t used = config_used();
    CHECK_EQ_INT(config_set_u32(1U, 115200U), HAL_OK);     /* same value: no write */
    CHECK_EQ_INT(config_used(), used);
    CHECK_EQ_INT(config_delete(2U), HAL_OK);
    CHECK(!config_get_str(2U, s, sizeof(s)));
    CHECK_EQ_INT(config_set(0U, &big, 4U), HAL_EINVAL);

    /* many updates: forces several compactions, values survive */
    for (uint32_t i = 0U; i < 500U; i++) {
        CHECK_EQ_INT(config_set_u32(3U, i), HAL_OK);
    }
    CHECK_EQ_INT(config_get_u32(3U, 0U), 499);
    CHECK_EQ_INT(config_get_u32(1U, 0U), 115200);
    CHECK(config_generation() >= 3U);             /* compacted at least twice */
    /* "reboot": re-read from flash */
    CHECK_EQ_INT(config_init(), HAL_OK);
    CHECK_EQ_INT(config_get_u32(3U, 0U), 499);
    CHECK_EQ_INT(config_get_u32(1U, 0U), 115200);
}

TEST(datetime_and_expr)
{
    datetime_t dt;
    uint32_t   t;
    char       buf[24];
    int32_t    v;

    dt_from_unix(0U, &dt);
    CHECK((dt.year == 1970U) && (dt.month == 1U) && (dt.day == 1U) && (dt.weekday == 4U));
    dt_from_unix(951782400U, &dt);                  /* 2000-02-29 */
    CHECK((dt.year == 2000U) && (dt.month == 2U) && (dt.day == 29U));
    CHECK(dt_parse("2026-10-16 08:30:05", &dt));
    CHECK(dt_to_unix(&dt, &t));
    CHECK_EQ_INT(t, 1792139405U);
    CHECK_EQ_INT(dt.weekday, 5);
    (void)dt_format(&dt, buf, sizeof(buf));
    CHECK_EQ_STR(buf, "2026-10-16 08:30:05");
    CHECK(!dt_parse("2025-02-29", &dt));
    CHECK(dt_parse("12:34", &dt) && (dt.hour == 12U) && (dt.second == 0U));
    for (uint32_t x = 0U; x < 4000000000U; x += 99991U * 37U) {
        dt_from_unix(x, &dt);
        CHECK(dt_to_unix(&dt, &t) && (t == x));
    }
    (void)dt_format_duration(90061U, buf, sizeof(buf));
    CHECK_EQ_STR(buf, "1d 01:01:01");

    CHECK(expr_eval("(1<<8)|(1<<9)", &v, NULL) == EXPR_OK && v == 768);
    CHECK(expr_eval("2+3*4-10/2", &v, NULL) == EXPR_OK && v == 9);
    CHECK(expr_eval("-(3 - 5) * ~0", &v, NULL) == EXPR_OK && v == -2);
    CHECK(expr_eval("0xFF & ~0x0F ^ 0x01", &v, NULL) == EXPR_OK && v == 0xF1);
    CHECK(expr_eval("1 < 2 && 3 >= 3 || 0", &v, NULL) == EXPR_OK && v == 1);
    CHECK(expr_eval("0x80000000 >> 31", &v, NULL) == EXPR_OK && v == 1);
    CHECK(expr_eval("'A' + 1", &v, NULL) == EXPR_OK && v == 66);
    CHECK(expr_eval("7 % 3 == 1", &v, NULL) == EXPR_OK && v == 1);
    CHECK(expr_eval("1 / 0", &v, NULL) == EXPR_ERR_DIV_ZERO);
    CHECK(expr_eval("(1 + 2", &v, NULL) == EXPR_ERR_PAREN);
    CHECK(expr_eval("1 + ", &v, NULL) == EXPR_ERR_SYNTAX);
    CHECK(expr_eval("foo", &v, NULL) == EXPR_ERR_SYNTAX);
    CHECK(expr_eval("((((((((((((((((((((((((((((((((((1))))))))))))))))))))))))))))))))))", &v, NULL)
          == EXPR_ERR_DEPTH);
}

TEST(driver_math)
{
    CHECK_EQ_INT(encoder_decode(0U, 1U), 1);
    CHECK_EQ_INT(encoder_decode(1U, 3U), 1);
    CHECK_EQ_INT(encoder_decode(3U, 2U), 1);
    CHECK_EQ_INT(encoder_decode(2U, 0U), 1);
    CHECK_EQ_INT(encoder_decode(1U, 0U), -1);
    CHECK_EQ_INT(encoder_decode(0U, 3U), 0);       /* invalid jump */
    CHECK_EQ_INT(ds18b20_raw_to_c100(0x0550), 8500);
    CHECK_EQ_INT(ds18b20_raw_to_c100(0x0191), 2506);
    CHECK_EQ_INT(ds18b20_raw_to_c100((int16_t)0xFF5E), -1013);
    CHECK_EQ_INT(ds18b20_conv_ms(DS18B20_RES_12BIT), 752);
    const uint8_t rom_ok[8] = { 0x28U, 0xFFU, 0x4CU, 0x6EU, 0x91U, 0x16U, 0x04U, 0x00U };
    uint8_t rom[8];
    memcpy(rom, rom_ok, 8U);
    rom[7] = crc8_maxim(0U, rom, 7U);
    CHECK(ow_rom_valid(rom));
    rom[3] ^= 1U;
    CHECK(!ow_rom_valid(rom));
    CHECK_EQ_INT(melody_note_hz("a4"), 440);
    CHECK_EQ_INT(melody_note_hz("a5"), 880);
    CHECK_EQ_INT(melody_note_hz("c4"), 262);
    CHECK_EQ_INT(melody_note_hz("c#5"), 554);
    CHECK_EQ_INT(melody_note_hz("b#4"), 523);
    CHECK_EQ_INT(melody_note_hz("x4"), 0);
}

TEST(drivers_on_gpio)
{
    led_t      led;
    shiftreg_t sr;

    led_init(&led, PB(3), true);
    led_blink(&led, 10U, 30U);
    uint32_t on_count = 0U;
    for (uint32_t t = hal_time_ms(), end = t + 400U; t < end; t++) {
        led_update(&led, t);
        on_count += hal_gpio_read_output(PB(3)) ? 1U : 0U;
    }
    CHECK((on_count >= 80U) && (on_count <= 120U));   /* 25 % duty */

    shiftreg595_init(&sr, PB(13), PB(14), PB(15), 2U);
    shiftreg595_write(&sr, 0xA55AU);
    CHECK_EQ_INT(shiftreg595_get(&sr), 0xA55A);
    shiftreg595_write_bit(&sr, 15U, false);
    CHECK_EQ_INT(shiftreg595_get(&sr), 0x255A);
}

TEST(cli_engine)
{
    io_init(cap_putc, NULL);
    cli_init("> ");
    cli_register_all();

    cap_reset();
    CHECK_EQ_INT(cli_execute("calc 6*7"), CLI_OK);
    CHECK(strstr(s_out, "42  0x0000002A") != NULL);
    cap_reset();
    CHECK_EQ_INT(cli_execute("nope"), CLI_ERR);
    CHECK(strstr(s_out, "unknown command") != NULL);
    cap_reset();
    CHECK_EQ_INT(cli_execute("ec \"a  b\" c"), CLI_OK);          /* prefix + quotes */
    CHECK_EQ_STR(s_out, "a  b c\r\n");
    cap_reset();
    (void)cli_execute("echo one ; echo two");
    CHECK_EQ_STR(s_out, "one\r\ntwo\r\n");
    cap_reset();
    CHECK_EQ_INT(cli_execute("gpio wr PC10 1"), CLI_OK);
    CHECK(hal_gpio_read_output(PC(10)));
    cap_reset();
    CHECK(cli_execute("t") == CLI_ERR);                          /* ambiguous: task temp time timer tone */
    CHECK(strstr(s_out, "ambiguous") != NULL);
    cap_reset();
    CHECK(cli_execute("delay x") != CLI_OK);
    CHECK(strstr(s_out, "not a number") != NULL);

    /* line editor: type, move left, insert, backspace, Enter */
    cap_reset();
    const char keys[] = "ecXo hi\x1b[D\x1b[D\x1b[D\x1b[D\x08h\r";
    for (const char *k = keys; *k != '\0'; k++) {
        cli_input_char(*k);
    }
    CHECK(strstr(s_out, "hi\r\n") != NULL);                      /* ran "echo hi" */
    /* history recall with the up arrow, then Enter */
    cap_reset();
    cli_input_char('\x1b');
    cli_input_char('[');
    cli_input_char('A');
    cli_input_char('\r');
    CHECK(strstr(s_out, "hi\r\n") != NULL);
    /* TAB completion of a unique prefix */
    cap_reset();
    const char tab[] = "uptim\t";
    for (const char *k = tab; *k != '\0'; k++) {
        cli_input_char(*k);
    }
    CHECK(strstr(s_out, "e ") != NULL);
    cli_input_char('\x03');                                       /* Ctrl-C clears */
    io_init(hal_console_putc, hal_console_getc);
}

int main(void)
{
    hal_sys_init();
    hal_time_init();
    sim_devices_install_default();
    printf("hal / driver / cli tests (PC simulation)\n");
    RUN(hal_helpers);
    RUN(gpio_sim);
    RUN(exti_sim);
    RUN(timer_sim);
    RUN(adc_sim);
    RUN(pwm_sim);
    RUN(eeprom);
    RUN(spi_flash);
    RUN(i2c_sensors);
    RUN(config_store);
    RUN(datetime_and_expr);
    RUN(driver_math);
    RUN(drivers_on_gpio);
    RUN(cli_engine);
    return unit_summary();
}
