#!/usr/bin/env python3
"""
Talk SCPI to the board (apps/scpi_instrument.c), like any bench instrument.

    python3 tools/scpi_client.py --sim                      # PC simulation, no board
    python3 tools/scpi_client.py --port /dev/ttyUSB0        # pyserial
    python3 tools/scpi_client.py --visa ASRL/dev/ttyUSB0::INSTR   # PyVISA (pyvisa-py)

    ... --sweep            DAC1 -> ADC4 transfer curve, saved to sweep.csv
    ... --shell            interactive prompt: type SCPI, see answers

The --sim mode starts build/pc/scpi_instrument.pc and talks to it through
pipes, so the same script can be developed at home and used on the bench.
"""

import argparse
import csv
import os
import subprocess
import sys
import time


class SimTransport:
    """The PC build of the firmware: its stdin/stdout are the 'UART'."""

    def __init__(self, binary):
        if not os.path.exists(binary):
            sys.exit(f"{binary} not found: run 'make APP=scpi_instrument' first")
        self.proc = subprocess.Popen([binary], stdin=subprocess.PIPE, stdout=subprocess.PIPE)

    def write(self, line):
        self.proc.stdin.write((line + "\n").encode())
        self.proc.stdin.flush()

    def read(self):
        return self.proc.stdout.readline().decode(errors="replace").strip()

    def close(self):
        self.proc.stdin.close()
        self.proc.wait(timeout=5)


class SerialTransport:
    def __init__(self, port, baud):
        import serial  # pip install pyserial
        self.ser = serial.Serial(port, baud, timeout=2)
        time.sleep(0.1)
        self.ser.reset_input_buffer()

    def write(self, line):
        self.ser.write((line + "\n").encode())

    def read(self):
        return self.ser.readline().decode(errors="replace").strip()

    def close(self):
        self.ser.close()


class VisaTransport:
    def __init__(self, resource, baud):
        import pyvisa  # pip install pyvisa pyvisa-py pyserial
        rm = pyvisa.ResourceManager("@py")
        self.inst = rm.open_resource(resource)
        self.inst.baud_rate = baud
        self.inst.read_termination = "\n"
        self.inst.write_termination = "\n"
        self.inst.timeout = 2000

    def write(self, line):
        self.inst.write(line)

    def read(self):
        return self.inst.read().strip()

    def close(self):
        self.inst.close()


class Instrument:
    """Thin SCPI wrapper: write(), query(), and error checking."""

    def __init__(self, transport):
        self.t = transport

    def write(self, cmd):
        self.t.write(cmd)

    def query(self, cmd):
        self.t.write(cmd)
        return self.t.read()

    def check_errors(self):
        errors = []
        while True:
            answer = self.query("SYST:ERR?")
            code = int(answer.split(",")[0])
            if code == 0:
                return errors
            errors.append(answer)

    def volts(self, channel):
        return float(self.query(f"MEAS:VOLT? {channel}"))


def sweep(inst, filename):
    """Classic lab exercise: drive DAC1 and read it back on ADC channel 4."""
    rows = []
    print(" set (V)   read (V)   error (mV)")
    for step in range(0, 34):
        v_set = step * 0.1
        inst.write(f"SOUR1:VOLT {v_set:.3f}")
        v_read = inst.volts(4)
        rows.append((v_set, v_read))
        print(f"  {v_set:5.2f}     {v_read:6.3f}     {1000 * (v_read - v_set):+7.1f}")
    with open(filename, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["set_V", "read_V"])
        w.writerows(rows)
    print(f"saved {filename}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sim", action="store_true", help="use the PC simulation")
    ap.add_argument("--binary", default="build/pc/scpi_instrument.pc")
    ap.add_argument("--port", help="serial port (pyserial)")
    ap.add_argument("--visa", help="VISA resource string (pyvisa)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--sweep", action="store_true")
    ap.add_argument("--shell", action="store_true")
    args = ap.parse_args()

    if args.sim:
        transport = SimTransport(args.binary)
    elif args.visa:
        transport = VisaTransport(args.visa, args.baud)
    elif args.port:
        transport = SerialTransport(args.port, args.baud)
    else:
        ap.error("choose --sim, --port or --visa")

    inst = Instrument(transport)
    print("connected to:", inst.query("*IDN?"))
    print("self test   :", "pass" if inst.query("*TST?") == "0" else "FAIL")
    print("temperature :", inst.query("MEAS:TEMP?"), "C")

    if args.sweep:
        sweep(inst, "sweep.csv")
    elif args.shell:
        print("type SCPI commands, empty line to quit")
        while True:
            line = input("scpi> ").strip()
            if not line:
                break
            if "?" in line:
                print(inst.query(line))
            else:
                inst.write(line)
            for e in inst.check_errors():
                print("  error:", e)
    else:
        inst.write("SOUR:FUNC SIN;SOUR:FREQ 250;SOUR:VOLT:AMPL 2;OUTP ON")
        print("generator 250 Hz sine 2 Vpp on DAC1, measured on ADC4:")
        print("  frequency :", inst.query("MEAS:FREQ? 4"), "Hz")
        print("  AC rms    :", inst.query("MEAS:VOLT:AC? 4"), "V")
        inst.write("OUTP OFF")

    errors = inst.check_errors()
    print("errors:", errors if errors else "none")
    transport.close()


if __name__ == "__main__":
    main()
