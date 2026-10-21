#ifndef GETTIME_H
#define GETTIME_H

#include <stdint.h>

void gettime_init(void);
uint32_t gettime(void);
uint32_t gettime_cpu_freq(void);
uint32_t gettime_cycles_to_ns(uint32_t cycles, uint32_t freq_hz);
uint32_t gettime_cycles_to_us(uint32_t cycles, uint32_t freq_hz);

#endif
