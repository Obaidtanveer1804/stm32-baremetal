#include <libopencm3/cm3/dwt.h>
#include <libopencm3/cm3/scs.h>
#include <libopencm3/stm32/rcc.h>
#include "gettime.h"

void gettime_init(void)
{
	SCS_DEMCR |= SCS_DEMCR_TRCENA;
	DWT_CYCCNT = 0;
	DWT_CTRL |= DWT_CTRL_CYCCNTENA;
}

uint32_t gettime(void)
{
	return DWT_CYCCNT;
}

uint32_t gettime_cpu_freq(void)
{
	return rcc_ahb_frequency;
}

uint32_t gettime_cycles_to_ns(uint32_t cycles, uint32_t freq_hz)
{
	return (uint32_t)(((uint64_t)cycles * 1000000000ULL) / freq_hz);
}

uint32_t gettime_cycles_to_us(uint32_t cycles, uint32_t freq_hz)
{
	return (uint32_t)(((uint64_t)cycles * 1000000ULL) / freq_hz);
}
