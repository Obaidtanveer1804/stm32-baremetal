/**
 * @file    scpi_instrument.c
 * @brief   The board as a SCPI instrument on its USB-serial port:
 *          voltmeter, frequency meter, DAC source / function generator,
 *          digital I/O. Drive it from Python with PyVISA exactly like a
 *          bench multimeter (see tools/scpi_client.py).
 *
 *   *IDN?                              ELISE,STM32-INSTR,<uid>,1.0
 *   *RST / *TST?
 *   MEASure:VOLTage[:DC]? [ch]         volts, 16x averaged (ch 0..15)
 *   MEASure:VOLTage:AC? [ch]           AC RMS volts (capture 256 @ 10 kHz)
 *   MEASure:FREQuency? [ch]            Hz (zero crossings)
 *   MEASure:TEMPerature?               die temperature, C
 *   SOURce#:VOLTage[:LEVel] <v>|MIN|MAX   DAC1/DAC2 output, volts
 *   SOURce#:VOLTage[:LEVel]?
 *   SOURce:FUNCtion[:SHAPe] SIN|SQU|TRI|DC   on DAC1
 *   SOURce:FREQuency <hz>              generator frequency
 *   SOURce:VOLTage:AMPLitude <vpp>
 *   OUTPut[:STATe] ON|OFF              generator on/off
 *   DIGital:OUTPut <pin>,<0|1>         e.g. DIG:OUTP PC8,1
 *   DIGital:INPut? <pin>
 *   SYSTem:ERRor?  SYSTem:VERSion?
 *
 * PC: run build/pc/scpi_instrument.pc and type commands, or
 *     python3 tools/scpi_client.py --sim
 */
#include "hal.h"
#include "io.h"
#include "scpi.h"
#include "dds.h"
#include "fmt.h"
#include "strutil.h"
#include "adc_capture.h"
#ifdef BOARD_PC
#include "sim.h"
#endif

#define GEN_TIMER          HAL_TIMER3
#define GEN_SAMPLE_HZ      (20000U)
#define VOLT_DECIMALS      (3U)          /* volts handled as mV        */
#define AC_SAMPLES         (256U)
#define AC_RATE_HZ         (10000U)
#define AVG_LOG2           (4U)

static scpi_t   s_scpi;
static char     s_idn[48];
static dds_t    s_gen;
static bool     s_gen_on;
static uint32_t s_gen_freq_c = 100000U;      /* 1 kHz */
static uint32_t s_gen_vpp_mv = 2000U;
static int16_t  s_buf[AC_SAMPLES];

/* ------------------------------- I/O ------------------------------ */

static void scpi_write(const char *text, size_t len, void *ctx)
{
    (void)ctx;
    io_write(text, len);
}

/* --------------------------- generator ---------------------------- */

static void gen_isr(hal_timer_t t, void *arg)
{
    (void)t;
    (void)arg;
    (void)hal_dac_write(HAL_DAC1, dds_next(&s_gen));
}

static void gen_apply(void)
{
    /* amplitude in DAC LSB: Vpp/2 * 4095 / 3300 */
    const uint32_t amp = (s_gen_vpp_mv * HAL_DAC_MAX) / (2U * HAL_ADC_VREF_MV);
    s_gen.amplitude = (uint16_t)amp;
    s_gen.offset    = (uint16_t)(HAL_DAC_MAX / 2U);
    dds_set_freq_c(&s_gen, s_gen_freq_c);
    if (s_gen_on) {
        (void)hal_dac_init(HAL_DAC1);
        (void)hal_timer_start_periodic(GEN_TIMER, GEN_SAMPLE_HZ, gen_isr, NULL);
    } else {
        hal_timer_stop(GEN_TIMER);
    }
}

/* ---------------------------- handlers ---------------------------- */

static bool channel_arg(scpi_args_t *a, int32_t *ch)
{
    *ch = 0;
    return scpi_args_empty(a) || scpi_param_fixed(a, 0U, 0, 17, 0, ch);
}

static int32_t h_rst(scpi_t *s, scpi_args_t *a)
{
    (void)s;
    (void)a;
    s_gen_on     = false;
    s_gen_freq_c = 100000U;
    s_gen_vpp_mv = 2000U;
    dds_init(&s_gen, GEN_SAMPLE_HZ, DDS_SINE, 0U, 0U);
    gen_apply();
    (void)hal_dac_write(HAL_DAC1, 0U);
    (void)hal_dac_write(HAL_DAC2, 0U);
    return SCPI_ERR_NONE;
}

static int32_t h_tst(scpi_t *s, scpi_args_t *a)
{
    (void)a;
    /* self test: VDDA plausible and ADC answering */
    const uint32_t vdda = hal_adc_vdda_mv();
    scpi_result_int(s, ((vdda > 2000U) && (vdda < 3600U)) ? 0 : 1);
    return SCPI_ERR_NONE;
}

static int32_t h_meas_volt(scpi_t *s, scpi_args_t *a)
{
    int32_t ch;
    if (!channel_arg(a, &ch)) {
        return SCPI_ERR_DATA_OUT_OF_RANGE;
    }
    const uint16_t raw = hal_adc_read_avg((uint8_t)ch, AVG_LOG2);
    scpi_result_fixed(s, (int32_t)hal_adc_raw_to_mv(raw, hal_adc_vdda_mv()), VOLT_DECIMALS);
    return SCPI_ERR_NONE;
}

static int32_t measure_capture(scpi_args_t *a, adc_measure_t *m)
{
    int32_t ch;
    if (!channel_arg(a, &ch)) {
        return SCPI_ERR_DATA_OUT_OF_RANGE;
    }
    if (adc_capture((uint8_t)ch, AC_RATE_HZ, s_buf, AC_SAMPLES) != HAL_OK) {
        return SCPI_ERR_EXECUTION;
    }
    adc_capture_analyse(s_buf, AC_SAMPLES, AC_RATE_HZ, m);
    return SCPI_ERR_NONE;
}

static int32_t h_meas_ac(scpi_t *s, scpi_args_t *a)
{
    adc_measure_t m;
    const int32_t err = measure_capture(a, &m);
    if (err == SCPI_ERR_NONE) {
        scpi_result_fixed(s, (int32_t)m.vrms_ac_mv, VOLT_DECIMALS);
    }
    return err;
}

static int32_t h_meas_freq(scpi_t *s, scpi_args_t *a)
{
    adc_measure_t m;
    const int32_t err = measure_capture(a, &m);
    if (err == SCPI_ERR_NONE) {
        scpi_result_fixed(s, (int32_t)m.freq_centihz, 2U);
    }
    return err;
}

static int32_t h_meas_temp(scpi_t *s, scpi_args_t *a)
{
    (void)a;
    scpi_result_fixed(s, hal_adc_temp_c10(), 1U);
    return SCPI_ERR_NONE;
}

static int32_t h_sour_volt(scpi_t *s, scpi_args_t *a)
{
    int32_t mv;
    (void)s;
    if ((a->suffix[0] < 1) || (a->suffix[0] > 2)) {
        return SCPI_ERR_SUFFIX_RANGE;
    }
    if (scpi_args_empty(a)) {
        return SCPI_ERR_MISSING_PARAM;
    }
    if (!scpi_param_fixed(a, VOLT_DECIMALS, 0, (int32_t)HAL_ADC_VREF_MV, 0, &mv)) {
        return SCPI_ERR_DATA_OUT_OF_RANGE;
    }
    const hal_dac_t dac = (a->suffix[0] == 1) ? HAL_DAC1 : HAL_DAC2;
    if (hal_dac_init(dac) != HAL_OK) {
        return SCPI_ERR_EXECUTION;          /* no DAC on this chip */
    }
    return (hal_dac_write_mv(dac, (uint32_t)mv) == HAL_OK) ? SCPI_ERR_NONE : SCPI_ERR_EXECUTION;
}

static int32_t h_sour_volt_q(scpi_t *s, scpi_args_t *a)
{
    if ((a->suffix[0] < 1) || (a->suffix[0] > 2)) {
        return SCPI_ERR_SUFFIX_RANGE;
    }
    const uint16_t raw = hal_dac_get((a->suffix[0] == 1) ? HAL_DAC1 : HAL_DAC2);
    scpi_result_fixed(s, (int32_t)((raw * HAL_ADC_VREF_MV + (HAL_DAC_MAX / 2U)) / HAL_DAC_MAX), VOLT_DECIMALS);
    return SCPI_ERR_NONE;
}

static int32_t h_func(scpi_t *s, scpi_args_t *a)
{
    static const char *const shapes[] = { "SINusoid", "TRIangle", "RAMP", "SQUare", "DC" };
    (void)s;
    const int32_t k = scpi_param_choice(a, shapes, 5U);
    if (k < 0) {
        return SCPI_ERR_DATA_TYPE;
    }
    if (k == 4) {
        s_gen.amplitude = 0U;                /* DC = offset only */
        s_gen.wave      = DDS_SINE;
    } else {
        static const dds_wave_t map[4] = { DDS_SINE, DDS_TRIANGLE, DDS_SAW, DDS_SQUARE };
        s_gen.wave = map[k];
        gen_apply();
    }
    return SCPI_ERR_NONE;
}

static int32_t h_freq(scpi_t *s, scpi_args_t *a)
{
    int32_t fc;
    (void)s;
    if (!scpi_param_fixed(a, 2U, 1, (int32_t)(GEN_SAMPLE_HZ * 100U / 4U), 100000, &fc)) {
        return SCPI_ERR_DATA_OUT_OF_RANGE;   /* Nyquist / 2 for a usable shape */
    }
    s_gen_freq_c = (uint32_t)fc;
    gen_apply();
    return SCPI_ERR_NONE;
}

static int32_t h_freq_q(scpi_t *s, scpi_args_t *a)
{
    (void)a;
    scpi_result_fixed(s, (int32_t)s_gen_freq_c, 2U);
    return SCPI_ERR_NONE;
}

static int32_t h_ampl(scpi_t *s, scpi_args_t *a)
{
    int32_t mv;
    (void)s;
    if (!scpi_param_fixed(a, VOLT_DECIMALS, 0, (int32_t)HAL_ADC_VREF_MV, 2000, &mv)) {
        return SCPI_ERR_DATA_OUT_OF_RANGE;
    }
    s_gen_vpp_mv = (uint32_t)mv;
    gen_apply();
    return SCPI_ERR_NONE;
}

static int32_t h_outp(scpi_t *s, scpi_args_t *a)
{
    (void)s;
    if (!scpi_param_bool(a, &s_gen_on)) {
        return SCPI_ERR_DATA_TYPE;
    }
    gen_apply();
    return SCPI_ERR_NONE;
}

static int32_t h_outp_q(scpi_t *s, scpi_args_t *a)
{
    (void)a;
    scpi_result_bool(s, s_gen_on);
    return SCPI_ERR_NONE;
}

static int32_t h_dig_out(scpi_t *s, scpi_args_t *a)
{
    char      name[8];
    hal_pin_t pin;
    bool      level;
    (void)s;
    if (!scpi_param_string(a, name, sizeof(name)) || !hal_gpio_parse(name, &pin)) {
        return SCPI_ERR_DATA_TYPE;
    }
    if (!scpi_param_bool(a, &level)) {
        return SCPI_ERR_MISSING_PARAM;
    }
    hal_gpio_mode(pin, HAL_GPIO_OUTPUT);
    hal_gpio_write(pin, level);
    return SCPI_ERR_NONE;
}

static int32_t h_dig_in(scpi_t *s, scpi_args_t *a)
{
    char      name[8];
    hal_pin_t pin;
    if (!scpi_param_string(a, name, sizeof(name)) || !hal_gpio_parse(name, &pin)) {
        return SCPI_ERR_DATA_TYPE;
    }
    scpi_result_bool(s, hal_gpio_read(pin));
    return SCPI_ERR_NONE;
}

static const scpi_command_t commands[] = {
    { "*RST",                       h_rst },
    { "*TST?",                      h_tst },
    { "MEASure:VOLTage[:DC]?",      h_meas_volt },
    { "MEASure:VOLTage:AC?",        h_meas_ac },
    { "MEASure:FREQuency?",         h_meas_freq },
    { "MEASure:TEMPerature?",       h_meas_temp },
    { "SOURce:VOLTage:AMPLitude",   h_ampl },
    { "SOURce#:VOLTage[:LEVel]",    h_sour_volt },
    { "SOURce#:VOLTage[:LEVel]?",   h_sour_volt_q },
    { "SOURce:FUNCtion[:SHAPe]",    h_func },
    { "SOURce:FREQuency",           h_freq },
    { "SOURce:FREQuency?",          h_freq_q },
    { "OUTPut[:STATe]",             h_outp },
    { "OUTPut[:STATe]?",            h_outp_q },
    { "DIGital:OUTPut",             h_dig_out },
    { "DIGital:INPut?",             h_dig_in },
};

int main(void)
{
    uint32_t uid[3];

    hal_sys_init();
    hal_time_init();
    hal_console_init(BOARD_CONSOLE_BAUD);
    io_init(hal_console_putc, hal_console_getc);
    (void)hal_adc_init();
    hal_sys_unique_id(uid);
    (void)fmt_snprintf(s_idn, sizeof(s_idn), "ELISE,STM32-INSTR,%08lX,1.0", (unsigned long)uid[0]);

    scpi_init(&s_scpi, commands, sizeof(commands) / sizeof(commands[0]), s_idn, scpi_write, NULL);
    (void)h_rst(&s_scpi, NULL);

    for (;;) {
        int c;
        while ((c = io_getc()) >= 0) {
            const char ch = (char)c;
            scpi_input(&s_scpi, &ch, 1U);
        }
        hal_sys_idle();
    }
    return 0;
}
