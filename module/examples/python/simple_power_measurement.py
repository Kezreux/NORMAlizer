"""U, I, P measurement on a three-phase system over TCP.

Python equivalent of examples/cpp/simple_power_measurement.cpp.

Usage: python simple_power_measurement.py [host] [port]

With no instrument on the bench, start the simulator and point this at it:

    ./build/linux-make/module/simulator/norma_sim --port 2300
    python simple_power_measurement.py 127.0.0.1 2300
"""

import sys
import time

from flukenorma import DEFAULT_PORT, Norma, WiringSystem, fn


def main() -> int:
    host = sys.argv[1] if len(sys.argv) > 1 else "192.168.1.100"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_PORT

    with Norma.connect(host, port) as instrument:
        print(f"Connected to {instrument.identify().model}")

        # The transfer format and the concurrency flag survive a disconnect, so a
        # previous session could have left the instrument answering in a binary
        # format. prepare() puts that right without touching the measurement
        # configuration.
        instrument.prepare()
        instrument.reset()
        instrument.wiring_system = WiringSystem.THREE_WATTMETER
        instrument.sync_to_voltage(1)
        instrument.set_voltage_range(1, 300.0)
        instrument.set_current_autorange(1)
        instrument.aperture = 1.0  # averaging time: 1 s

        instrument.functions = [
            fn.voltage(1),       # "VOLT1"
            fn.current(1),       # "CURR1"
            fn.active_power(1),  # "POW1:ACT"
        ]
        instrument.continuous = True
        instrument.check_errors()  # raise early if the configuration was rejected

        # DATA? does not wait for the averaging interval; give the instrument
        # time to produce the first complete measurement.
        time.sleep(2.0)

        reading = instrument.read()
        for label, measurement in zip(("U1 [V]", "I1 [A]", "P1 [W]"), reading):
            print(f"{label:8s} {measurement.value:12.6g}  (status {measurement.status!r})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
