"""Send PC stats (CPU, RAM, temperature) to the pet over USB every second.

Setup (once):   python -m pip install psutil pyserial
Run:            python tools/pc_monitor.py            (finds the board automatically)
                python tools/pc_monitor.py COM4       (or name the port)
Keep it running in the background; close it with Ctrl+C.
"""
import json
import sys
import time

import psutil
import serial
from serial.tools import list_ports

ESPRESSIF_VID = 0x303A


def find_port():
    for p in list_ports.comports():
        if p.vid == ESPRESSIF_VID:
            return p.device
    return None


def cpu_temp():
    try:
        temps = psutil.sensors_temperatures()  # not available on Windows
        for entries in temps.values():
            for t in entries:
                if t.current:
                    return round(t.current, 1)
    except (AttributeError, OSError):
        pass
    return 0


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_port()
    if not port:
        sys.exit("Board not found. Connect it on USB2 or pass the port, e.g. COM4")
    print(f"Sending PC stats to {port} (Ctrl+C to stop)")
    psutil.cpu_percent(None)
    while True:
        try:
            s = serial.Serial()
            s.port, s.baudrate, s.timeout = port, 115200, 1
            s.dtr = False  # set before open: toggling DTR/RTS would reset the board
            s.rts = False
            with s:  # opens the port
                while True:
                    msg = {"cpu": psutil.cpu_percent(None), "ram": psutil.virtual_memory().percent, "temp": cpu_temp()}
                    s.write((json.dumps(msg) + "\n").encode())
                    s.reset_input_buffer()  # drop the board's log lines
                    time.sleep(1)
        except serial.SerialException as e:
            print("Lost connection, retrying in 3 s:", e)
            time.sleep(3)
        except KeyboardInterrupt:
            break


if __name__ == "__main__":
    main()
