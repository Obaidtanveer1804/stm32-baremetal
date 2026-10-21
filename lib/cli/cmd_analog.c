/**
 * @file    cmd_analog.c
 * @brief   ADC / DAC commands, with statistics, histogram, an ASCII
 *          oscilloscope and a timer-driven DAC waveform generator.
 */
#include "cli_cmds.h"
#include "hal.h"
#include "io.h"
#include "stats.h"
#include "fixmath.h"
#include "strutil.h"
#include "dsp.h"
#include "adc_capture.h"

#define SCOPE_WIDTH        (60U)
#define DAC_WAVE_SAMPLES   (32U)      /* samples per period            */
#define DAC_WAVE_TIMER     HAL_TIMER2

static bool parse_channel(const char *s, uint8_t *ch)
{
    uint32_t v;
    if (str_ieq(s, "temp")) {
        *ch = HAL_ADC_CH_TEMP;
        return true;
    }
    if (str_ieq(s, "vref")) {
        *ch = HAL_ADC_CH_VREFINT;
        return true;
    }
    if (!cli_arg_u32_range(s, 0U, HAL_ADC_CH_COUNT - 1U, &v)) {
        return false;
    }
    *ch = (uint8_t)v;
    return true;
}

static void print_mv(uint32_t mv)
{
    io_printf("%lu.%03lu V", (unsigned long)(mv / 1000U), (unsigned long)(mv % 1000U));
}

static int sub_read(int argc, char *argv[])
{
    uint8_t  ch;
    uint32_t n = 1U;
    if ((argc < 2) || !parse_channel(argv[1], &ch)) {
        return CLI_ERR_USAGE;
    }
    if ((argc >= 3) && !cli_arg_u32_range(argv[2], 1U, 10000U, &n)) {
        return CLI_ERR;
    }
    (void)hal_adc_init();
    const uint32_t vdda = hal_adc_vdda_mv();
    for (uint32_t i = 0U; i < n; i++) {
        const uint16_t raw = hal_adc_read(ch);
        io_printf("ch%u raw %4u  ", (unsigned)ch, (unsigned)raw);
        print_mv(hal_adc_raw_to_mv(raw, vdda));
        io_puts(IO_NEWLINE);
        if ((n > 1U) && cli_abort_requested()) {
            break;
        }
    }
    return CLI_OK;
}

static int sub_scan(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    (void)hal_adc_init();
    const uint32_t vdda = hal_adc_vdda_mv();
    for (uint8_t ch = 0U; ch < HAL_ADC_CH_COUNT; ch++) {
        const hal_pin_t pin = hal_adc_channel_pin(ch);
        char name[6];
        const uint16_t raw = hal_adc_read(ch);
        io_printf("ch%-2u %-5s %4u  ", (unsigned)ch,
                  (pin == HAL_PIN_NONE) ? ((ch == HAL_ADC_CH_TEMP) ? "temp" : "vref") : hal_gpio_name(pin, name),
                  (unsigned)raw);
        print_mv(hal_adc_raw_to_mv(raw, vdda));
        io_puts(IO_NEWLINE);
    }
    return CLI_OK;
}

static int sub_temp(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    (void)hal_adc_init();
    const int32_t t = hal_adc_temp_c10();
    io_printf("die temperature %ld.%ld C (+-1.5 C typical)" IO_NEWLINE,
              (long)(t / 10), (long)fx_abs(t % 10));
    return CLI_OK;
}

static int sub_vdda(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    (void)hal_adc_init();
    io_puts("VDDA = ");
    print_mv(hal_adc_vdda_mv());
    io_println(" (measured with VREFINT)");
    return CLI_OK;
}

static int sub_stats(int argc, char *argv[])
{
    uint8_t  ch;
    uint32_t n    = 256U;
    uint32_t bins[12];
    stats_t  st;
    histo_t  h;

    if ((argc < 2) || !parse_channel(argv[1], &ch)) {
        return CLI_ERR_USAGE;
    }
    if ((argc >= 3) && !cli_arg_u32_range(argv[2], 2U, 100000U, &n)) {
        return CLI_ERR;
    }
    (void)hal_adc_init();
    stats_reset(&st);
    /* first pass: statistics; second pass: histogram around the mean */
    for (uint32_t i = 0U; i < n; i++) {
        stats_add(&st, hal_adc_read(ch));
    }
    stats_print(&st, "adc");
    const int32_t span = ((st.max - st.min) < 12) ? 12 : (st.max - st.min + 1);
    histo_init(&h, bins, 12U, st.min, st.min + span);
    for (uint32_t i = 0U; i < n; i++) {
        histo_add(&h, hal_adc_read(ch));
    }
    histo_print(&h, 40U);
    return CLI_OK;
}

static int sub_scope(int argc, char *argv[])
{
    uint8_t  ch;
    uint32_t period = 50U;
    if ((argc < 2) || !parse_channel(argv[1], &ch)) {
        return CLI_ERR_USAGE;
    }
    if ((argc >= 3) && !cli_arg_u32_range(argv[2], 1U, 10000U, &period)) {
        return CLI_ERR;
    }
    (void)hal_adc_init();
    io_println("0V                          ->                         VDDA  (q stops)");
    while (!cli_abort_requested()) {
        const uint16_t raw = hal_adc_read(ch);
        const uint32_t pos = ((uint32_t)raw * (SCOPE_WIDTH - 1U)) / HAL_ADC_MAX;
        io_putc('|');
        io_repeat(' ', pos);
        io_putc('*');
        io_repeat(' ', SCOPE_WIDTH - 1U - pos);
        io_printf("| %4u" IO_NEWLINE, (unsigned)raw);
        hal_delay_ms(period);
    }
    return CLI_OK;
}


#if CLI_WITH_DSP
/* -------------------- timed capture + spectrum -------------------- */

#define CAPTURE_MAX_LOG2  (8U)                  /* 256 samples        */
#define CAPTURE_MAX       (1U << CAPTURE_MAX_LOG2)
#define SPECTRUM_ROWS     (32U)
#define SPECTRUM_WIDTH    (48U)

static int16_t *s_cap_buf;      /* points into the CLI scratch area */

/* scratch layout: samples | FFT work | magnitudes  (~2 KiB for 256 points) */
#define SCRATCH_SAMPLES_BYTES  (CAPTURE_MAX * sizeof(int16_t))
#define SCRATCH_WORK_BYTES     (CAPTURE_MAX * sizeof(dsp_cplx_t))
#define SCRATCH_TOTAL_BYTES    (SCRATCH_SAMPLES_BYTES + SCRATCH_WORK_BYTES + (((CAPTURE_MAX / 2U) + 1U) * sizeof(uint16_t)))

static uint8_t *scratch_layout(void)
{
    uint8_t *mem = (uint8_t *)cli_scratch(SCRATCH_TOTAL_BYTES);
    s_cap_buf = (int16_t *)(void *)mem;
    return mem;
}

static bool capture(uint8_t ch, uint32_t rate, uint32_t n)
{
    if (scratch_layout() == NULL) {
        (void)cli_error("scratch area too small for %lu samples", (unsigned long)n);
        return false;
    }
    const hal_status_t st = adc_capture(ch, rate, s_cap_buf, n);
    if (st != HAL_OK) {
        (void)cli_error("capture at %lu Hz failed: %s", (unsigned long)rate, hal_status_str(st));
        return false;
    }
    return true;
}

static int sub_fft(int argc, char *argv[])
{
    uint8_t  ch;
    uint32_t rate = 8000U;
    uint32_t log2n = 7U;

    if ((argc < 2) || !parse_channel(argv[1], &ch)) {
        return CLI_ERR_USAGE;
    }
    if (((argc >= 3) && !cli_arg_u32_range(argv[2], 100U, 40000U, &rate)) ||
        ((argc >= 4) && !cli_arg_u32_range(argv[3], 4U, CAPTURE_MAX_LOG2, &log2n))) {
        return CLI_ERR;
    }
    const uint32_t n = 1U << log2n;
    if (!capture(ch, rate, n)) {
        return CLI_ERR;
    }
    uint8_t    *mem  = scratch_layout();
    dsp_cplx_t *work = (dsp_cplx_t *)(void *)&mem[SCRATCH_SAMPLES_BYTES];
    uint16_t   *mag  = (uint16_t *)(void *)&mem[SCRATCH_SAMPLES_BYTES + SCRATCH_WORK_BYTES];
    const uint32_t rms_ac = (dsp_remove_dc(s_cap_buf, n), dsp_rms(s_cap_buf, n));
    dsp_window_hann(s_cap_buf, n);
    (void)dsp_fft_magnitude(s_cap_buf, log2n, work, mag);
    uint16_t       peak_mag;
    const uint32_t peak = dsp_peak(mag, 1U, (n / 2U) + 1U, &peak_mag);
    const uint32_t res  = dsp_bin_freq_c(1U, rate, n);

    io_printf("%lu samples at %lu Hz, %lu.%02lu Hz per bin" IO_NEWLINE, (unsigned long)n,
              (unsigned long)rate, (unsigned long)(res / 100U), (unsigned long)(res % 100U));
    /* group bins into rows, show the max of each group */
    const uint32_t bins = n / 2U;
    const uint32_t rows = (bins < SPECTRUM_ROWS) ? bins : SPECTRUM_ROWS;
    const uint32_t per  = bins / rows;
    for (uint32_t r = 0U; r < rows; r++) {
        uint16_t v;
        (void)dsp_peak(mag, (r * per) + 1U, ((r + 1U) * per) + 1U, &v);
        const uint32_t f   = dsp_bin_freq_c((r * per) + 1U, rate, n) / 100U;
        const uint32_t len = (peak_mag == 0U) ? 0U : (((uint32_t)v * SPECTRUM_WIDTH) / peak_mag);
        io_printf("%6lu Hz |", (unsigned long)f);
        io_repeat('#', len);
        io_puts(IO_NEWLINE);
    }
    const uint32_t pf = dsp_bin_freq_c(peak, rate, n);
    io_printf("peak %lu.%02lu Hz (bin %lu), AC rms %lu LSB = %lu mV" IO_NEWLINE,
              (unsigned long)(pf / 100U), (unsigned long)(pf % 100U), (unsigned long)peak,
              (unsigned long)rms_ac, (unsigned long)hal_adc_raw_to_mv(rms_ac, HAL_ADC_VREF_MV));
    return CLI_OK;
}

static int sub_freq(int argc, char *argv[])
{
    uint8_t       ch;
    uint32_t      rate = 10000U;
    adc_measure_t m;
    if ((argc < 2) || !parse_channel(argv[1], &ch)) {
        return CLI_ERR_USAGE;
    }
    if ((argc >= 3) && !cli_arg_u32_range(argv[2], 100U, 40000U, &rate)) {
        return CLI_ERR;
    }
    if (!capture(ch, rate, CAPTURE_MAX)) {
        return CLI_ERR;
    }
    adc_capture_analyse(s_cap_buf, CAPTURE_MAX, rate, &m);
    io_printf("freq %lu.%02lu Hz  Vpp %lu mV  Vrms(ac) %lu mV  Vdc %ld mV" IO_NEWLINE,
              (unsigned long)(m.freq_centihz / 100U), (unsigned long)(m.freq_centihz % 100U),
              (unsigned long)m.vpp_mv, (unsigned long)m.vrms_ac_mv, (long)m.vdc_mv);
    return CLI_OK;
}

#endif /* CLI_WITH_DSP */

static const cli_subcmd_t adc_subs[] = {
    { "read",  sub_read,  "<ch|temp|vref> [n]", "raw value and voltage" },
    { "scan",  sub_scan,  "",                   "all 18 channels" },
    { "temp",  sub_temp,  "",                   "internal temperature sensor" },
    { "vdda",  sub_vdda,  "",                   "supply voltage via VREFINT" },
    { "stats", sub_stats, "<ch> [n]",           "noise: min/max/mean/sd + histogram" },
    { "scope", sub_scope, "<ch> [ms]",          "ASCII oscilloscope" },
#if CLI_WITH_DSP
    { "fft",   sub_fft,   "<ch> [rate] [log2n]", "spectrum (Hann window, Q15 FFT)" },
    { "freq",  sub_freq,  "<ch> [rate]",        "frequency, Vpp, Vrms of a signal" },
#endif
    { NULL, NULL, NULL, NULL }
};

/* ------------------------------- dac ------------------------------ */

static volatile uint32_t s_wave_phase;
static uint8_t           s_wave_kind;     /* 0 sine 1 tri 2 saw 3 square */
static hal_dac_t         s_wave_ch;

/* Runs in the timer interrupt: one sample per call. */
static void dac_wave_isr(hal_timer_t t, void *arg)
{
    (void)t;
    (void)arg;
    const uint32_t i     = s_wave_phase;
    const uint16_t angle = (uint16_t)((i * 65536U) / DAC_WAVE_SAMPLES);
    uint32_t       v;

    switch (s_wave_kind) {
    case 1U:  /* triangle */
        v = (i < (DAC_WAVE_SAMPLES / 2U)) ? ((i * 2U * HAL_DAC_MAX) / DAC_WAVE_SAMPLES)
                                          : (((DAC_WAVE_SAMPLES - i) * 2U * HAL_DAC_MAX) / DAC_WAVE_SAMPLES);
        break;
    case 2U:  /* saw */
        v = (i * HAL_DAC_MAX) / (DAC_WAVE_SAMPLES - 1U);
        break;
    case 3U:  /* square */
        v = (i < (DAC_WAVE_SAMPLES / 2U)) ? HAL_DAC_MAX : 0U;
        break;
    default:  /* sine, centred on mid-scale */
        v = (uint32_t)(2048 + ((2047 * (int32_t)fx_sin_q15(angle)) / 32767));
        break;
    }
    (void)hal_dac_write(s_wave_ch, (uint16_t)v);
    s_wave_phase = (i + 1U) % DAC_WAVE_SAMPLES;
}

static int cmd_dac(int argc, char *argv[])
{
    uint32_t ch;
    uint32_t value;
    if (argc < 2) {
        return CLI_ERR_USAGE;
    }
    if (str_eq(argv[1], "stop")) {
        hal_timer_stop(DAC_WAVE_TIMER);
        return CLI_OK;
    }
    if ((argc < 3) || !cli_arg_u32_range(argv[1], 1U, 2U, &ch)) {
        return CLI_ERR_USAGE;
    }
    const hal_dac_t dac = (ch == 1U) ? HAL_DAC1 : HAL_DAC2;
    hal_status_t    st  = hal_dac_init(dac);
    if (st != HAL_OK) {
        return cli_error("dac: %s", hal_status_str(st));
    }
    if (str_eq(argv[2], "wave")) {
        static const char *const kinds[] = { "sine", "tri", "saw", "square" };
        uint32_t hz;
        if ((argc != 5) || !cli_arg_u32_range(argv[4], 1U, 2000U, &hz)) {
            return CLI_ERR_USAGE;
        }
        s_wave_kind = 0xFFU;
        for (uint8_t k = 0U; k < 4U; k++) {
            if (str_eq(argv[3], kinds[k])) {
                s_wave_kind = k;
            }
        }
        if (s_wave_kind == 0xFFU) {
            return cli_error("waves: sine tri saw square");
        }
        s_wave_ch    = dac;
        s_wave_phase = 0U;
        st = hal_timer_start_periodic(DAC_WAVE_TIMER, hz * DAC_WAVE_SAMPLES, dac_wave_isr, NULL);
        if (st != HAL_OK) {
            return cli_error("timer: %s", hal_status_str(st));
        }
        io_printf("%s %lu Hz on DAC%lu (TIM2 at %lu Hz), 'dac stop' to end" IO_NEWLINE, argv[3],
                  (unsigned long)hz, (unsigned long)ch, (unsigned long)(hz * DAC_WAVE_SAMPLES));
        return CLI_OK;
    }
    if (str_ends_with(argv[2], "mV") || str_ends_with(argv[2], "mv")) {
        char buf[12];
        (void)str_copy(buf, argv[2], sizeof(buf));
        buf[str_len(buf) - 2U] = '\0';
        if (!cli_arg_u32_range(buf, 0U, HAL_ADC_VREF_MV, &value)) {
            return CLI_ERR;
        }
        st = hal_dac_write_mv(dac, value);
    } else {
        if (!cli_arg_u32_range(argv[2], 0U, HAL_DAC_MAX, &value)) {
            return CLI_ERR;
        }
        st = hal_dac_write(dac, (uint16_t)value);
    }
    return (st == HAL_OK) ? CLI_OK : cli_error("dac: %s", hal_status_str(st));
}

static const cli_command_t analog_cmds[] = {
    { "adc", NULL,    "<sub> ...", "12-bit ADC1", adc_subs },
    { "dac", cmd_dac, "<1|2> <0..4095|NNNNmV|wave <sine|tri|saw|square> <hz>> | stop",
                                   "DAC output (F100 only)", NULL },
};

static const cli_group_t analog_group = { "analog", analog_cmds, sizeof(analog_cmds) / sizeof(analog_cmds[0]) };

const cli_group_t *cmd_analog_group(void)
{
    return &analog_group;
}
