"""Record the localizer's serial stream into per-session CSV files.

Usage (repo root, with the ESP32 plugged in and running `localizer`):
    .venv/Scripts/python ESP32/tools/serial_logger.py            # auto-detect port
    .venv/Scripts/python ESP32/tools/serial_logger.py --port COM5 --note "floor0 loop"

While it runs, type a location name and press Enter to drop a ground-truth
waypoint mark (e.g. "DLT8/1"). A bare Enter marks "waypoint". Type q to stop.

Output: Data/05_ESP32_Sessions/<YYYY-mm-dd_HH-MM-SS>/
    imu.csv    t_us,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps        (~100 Hz)
    aps.csv    t_us,cycle,bssid,rssi,channel                    (raw 2.4 GHz scans)
    preds.csv  t_us,cycle,ok,class_index,confidence,ap_detected,ap_seen,latency_ms,label
    marks.csv  host_time,t_us,label                             (your waypoint marks)
    raw.log    every line exactly as received
"""

import argparse
import csv
import datetime as dt
import sys
import threading
import time
from pathlib import Path

import serial
from serial.tools import list_ports

REPO = Path(__file__).resolve().parents[2]
ESP_USB_VIDS = {0x10C4, 0x1A86, 0x0403, 0x303A}  # CP210x, CH34x, FTDI, Espressif native

HEADERS = {
    "IMU": ["t_us", "ax_g", "ay_g", "az_g", "gx_dps", "gy_dps", "gz_dps"],
    "AP": ["t_us", "cycle", "bssid", "rssi", "channel"],
    "PRED": ["t_us", "cycle", "ok", "class_index", "confidence", "ap_detected", "ap_seen", "latency_ms", "label"],
}
FILES = {"IMU": "imu.csv", "AP": "aps.csv", "PRED": "preds.csv"}


def find_port() -> str:
    ports = [p for p in list_ports.comports() if p.vid in ESP_USB_VIDS]
    if len(ports) == 1:
        return ports[0].device
    listing = ", ".join(f"{p.device} ({p.description})" for p in list_ports.comports()) or "none"
    sys.exit(f"Could not pick a port automatically. Pass --port. Available: {listing}")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port")
    ap.add_argument("--baud", type=int, default=460800)
    ap.add_argument("--out", type=Path, default=REPO / "Data" / "05_ESP32_Sessions")
    ap.add_argument("--note", default="", help="free-text description saved with the session")
    args = ap.parse_args()

    port = args.port or find_port()
    session = args.out / dt.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    session.mkdir(parents=True)
    if args.note:
        (session / "note.txt").write_text(args.note + "\n", encoding="utf-8")

    handles = {k: open(session / f, "w", newline="", encoding="utf-8") for k, f in FILES.items()}
    writers = {k: csv.writer(h) for k, h in handles.items()}
    for k, w in writers.items():
        w.writerow(HEADERS[k])
    marks_fh = open(session / "marks.csv", "w", newline="", encoding="utf-8")
    marks = csv.writer(marks_fh)
    marks.writerow(["host_time", "t_us", "label"])
    marks_fh.flush()
    raw = open(session / "raw.log", "w", encoding="utf-8")

    last_t_us = ["0"]
    stop = threading.Event()

    def read_marks() -> None:
        for line in sys.stdin:
            label = line.strip()
            if label.lower() == "q":
                stop.set()
                return
            label = label or "waypoint"
            marks.writerow([dt.datetime.now().isoformat(), last_t_us[0], label])
            marks_fh.flush()
            print(f"  [mark] {label} @ t_us={last_t_us[0]}")

    threading.Thread(target=read_marks, daemon=True).start()

    print(f"Logging {port} @ {args.baud} -> {session}")
    print("Type a location + Enter to mark a waypoint, q + Enter to stop.\n")
    imu_count, t_report = 0, time.monotonic()
    with serial.Serial(port, args.baud, timeout=0.5) as ser:
        try:
            while not stop.is_set():
                line = ser.readline().decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                raw.write(line + "\n")
                kind, _, rest = line.partition(",")
                if kind in writers:
                    row = next(csv.reader([rest]))
                    writers[kind].writerow(row)
                    last_t_us[0] = row[0]
                    if kind == "IMU":
                        imu_count += 1
                    elif kind == "PRED":
                        ok, conf, label = row[2], row[4], row[-1]
                        print(f"  #{row[1]:>4} {'OK ' if ok == '1' else 'ERR'} {float(conf):5.2f}  {label}")
                elif kind.startswith("#") or kind == "ERR":
                    print(f"  {line}")
                if time.monotonic() - t_report >= 5:
                    print(f"  IMU {imu_count / (time.monotonic() - t_report):.0f} Hz")
                    imu_count, t_report = 0, time.monotonic()
                    for h in (*handles.values(), raw):
                        h.flush()
        except KeyboardInterrupt:
            pass

    for h in (*handles.values(), marks_fh, raw):
        h.close()
    print(f"Saved session to {session}")


if __name__ == "__main__":
    main()
