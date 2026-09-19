"""U, I, P measurement on a three-phase system over TCP.

Python equivalent of examples/cpp/simple_power_measurement.cpp.

Usage: python simple_power_measurement.py [host]
"""

import sys
import time

import flukenorma as norma


def main() -> int:
    host = sys.argv[1] if len(sys.argv) > 1 else "192.168.1.100"

    with norma.Norma(host) as instrument:
        print(f"Connected to {instrument.identify().model}")

        instrument.reset()
        instrument.set_wiring_system(norma.WiringSystem.THREE_WATTMETER)
        instrument.sync_to_voltage(1)
        instrument.set_voltage_range(1, 300.0)
        instrument.set_current_autorange(1)
        instrument.set_aperture(1.0)  # averaging time: 1 s

        functions = [
            norma.fn.voltage(1),       # "VOLT1"
            norma.fn.current(1),       # "CURR1"
            norma.fn.active_power(1),  # "POW1:ACT"
        ]
        instrument.set_functions(functions)
        instrument.set_continuous(True)
        instrument.check_errors()  # raise early if the configuration was rejected

        # DATA? does not wait for the averaging interval; give the instrument
        # time to produce the first complete measurement.
        time.sleep(2.0)

        reading = instrument.data_with_status(functions)
        for name, value, status in zip(("U1 [V]", "I1 [A]", "P1 [W]"),
                                       reading.values, reading.status):
            print(f"{name:8s} {value:12.6g}  (status {status})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
