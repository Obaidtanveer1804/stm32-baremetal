#!/bin/sh
# ---------------------------------------------------------------------------
# End-to-end test of the shell: pipe a script into the PC build of
# apps/cli.c and check the answers. The simulator powers off when the
# script ends (stdin EOF), so no timeout tricks are needed.
#
#   ./tests/test_cli.sh            (run from lib/, after "make APP=cli")
# ---------------------------------------------------------------------------
set -u

BIN=${BIN:-build/pc/cli.pc}
WORK=build/test/cli
mkdir -p "$WORK"

if [ ! -x "$BIN" ]; then
    make --no-print-directory APP=cli >/dev/null || exit 1
fi

pass=0
fail=0

# expect "<command line>" "<text that must appear in the output>"
expect() {
    out=$(printf '%s\n' "$1" | (cd "$WORK" && "../../../$BIN") 2>&1 | sed 's/\x1b\[[0-9;]*[A-Za-z]//g')
    if printf '%s' "$out" | grep -qF -- "$2"; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        echo "  FAIL: '$1'  expected '$2'"
        printf '%s\n' "$out" | tail -5 | sed 's/^/        /'
    fi
}

echo "cli end-to-end tests"
expect "calc 0x10 + 0b11"                  "19  0x00000013"
expect "calc 1/0"                          "division by zero"
expect "let a 5 ; calc a * a"              "25  0x00000019"
expect "conv 255"                          "popcount 8"
expect "bits 0x8001"                       "(1U << 0) | (1U << 15)"
expect "gpio set PC8 ; gpio read PC8"      "PC8 = 1"
expect "gpio port C 0x0300 ; gpio port C"  "ODR 0x0300"
expect "peek GPIOC+0x0C"                   "0x4001100C:"
expect "gpio mode PC9 out ; poke 0x40011010 0x200 ; gpio read PC9" "PC9 = 1"
expect "peek 0x12345678"                   "not readable"
expect "dump 0x20000000 16"                "20000000"
expect "i2c init 1 ; i2c scan 1"           "4 device(s)"
expect "i2c init 1 ; i2c reg 1 0x48 0 2"   "[00] 19 00"
expect "eeprom write 0 \"abc\" ; eeprom read 0 16" "|abc"
expect "eeprom test"                       "EEPROM test passed"
expect "flash id"                          "JEDEC EF 40 15"
expect "flash erase 0 ; flash write 0 1 2 3 ; flash read 0 4" "01 02 03 FF"
expect "sim temp 42 ; temp"                "LM75    42.0 C"
expect "sim adc 2 4095 ; adc read 2"       "3.300 V"
expect "adc vdda"                          "VDDA = 3.300 V"
expect "dac 1 1000mV ; adc read 4"         "1.000 V"
expect "pwm init t4c2 2000 ; pwm duty t4c2 12.5 ; sim status" "12.5%"
expect "timer calc 50"                     "-> 50 Hz (error 0 Hz)"
expect "servo t3c1 90"                     "1500 us pulse"
expect "date 2030-01-02 03:04:05 ; date"   "2030-01-02 03:04"
expect "config format ; config set 5 1234 ; config list" "1234 (0x4D2)"
expect "rtcx 2027-07-14 12:00:00"          "DS1307 2027-07-14 12:00:00 Wed"
expect "crc 0x31 0x32 0x33 0x34 0x35 0x36 0x37 0x38 0x39" "crc32        0xCBF43926"
expect "b64 hello ; b64 -d aGVsbG8="       "aGVsbG8="
expect "seg7 8"                            "gfedcba: 0x7F"
expect "morse S"                           "..."
expect "unknown_cmd"                       "unknown command"
expect "gp"                                "gpio: sub-command needed"
expect "help gpio"                         "gpio monitor"
expect "echo a ; echo b ; echo c"          "c"
expect "repeat 3 echo x"                   "x"
expect "uart init 2 9600 ; sim uart 2 loop on ; uart send 2 hi ; uart recv 2" "|hi.|"
expect "info"                              "STM32VL-Discovery"
expect "top"                               "cli"
expect "log debug ; log"                   "log level: debug"
expect "time delay 50"                     "took"
expect "count 0 3 1 1 bin"                 "binary counter: 00"
expect "pwm init t3c3 1000 ; pwm duty t3c3 25 ; peek TIM3+0x3C" "0x00001770"
expect "pwm init t3c3 1000 ; poke TIM3+0x28 1 ; timer info" "TIM3 running     500 Hz"
expect "sim wave 1 sine 2048 800 4 ; adc fft 1 8000 7" "peak 250.00 Hz"
expect "sim wave 1 sine 2048 800 4 ; adc freq 1" "freq 250.00 Hz"
expect "bench"                             "crc32 1 KiB"
expect "geo 47.2378 6.0241 47.2466 6.0219" "distance 993 m"

echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
