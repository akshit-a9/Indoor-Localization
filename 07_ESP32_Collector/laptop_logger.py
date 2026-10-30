"""Optional: save the collector's recordings straight to the laptop over USB.

With the ESP32 plugged into a laptop, the firmware copies every take to
Serial as it records. This script saves each finished take to
02_Data/05_Wifi_ESP32/ and right away compares it with the phone recording
of the same spot, so you can see the dB offset while still standing there.
The phone page works exactly the same with or without this running.

    pip install pyserial pandas
    python3 07_ESP32_Collector/laptop_logger.py            # finds the ESP32 port
    python3 07_ESP32_Collector/laptop_logger.py --port /dev/cu.usbserial-0001

Takes saved here are also on the ESP32's flash. Downloading them again from
the phone page is harmless: 03_Training/07_Compare_ESP32_vs_Phone.py drops
the duplicates.
"""

import argparse
import csv
import io
import re
import sys
from pathlib import Path

import pandas as pd

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("Needs pyserial. If the wrong 'serial' package is installed: "
             "pip uninstall serial && pip install pyserial")

REPO = Path(__file__).resolve().parents[1]
OUT_DIR = REPO / "02_Data" / "05_Wifi_ESP32"
PHONE_DIR = REPO / "02_Data" / "01_Wifi_Daytime"

# Same columns as the download from the collector page (storage.cpp)
HEADER = "Timestamp,SSID,BSSID,RSSI,Frequency,Activity Label,Location Label,Take,Scan,Heading,Device\n"
BAUD = 115200


def find_port() -> str:
    ports = [p.device for p in list_ports.comports()
             if re.search(r"usbserial|SLAB|wchusb|ttyUSB|COM\d", p.device)]
    if not ports:
        sys.exit("No ESP32 found on USB. Plug it in, or pass --port.")
    return ports[0]


def quick_compare(path: Path, location_id: int) -> None:
    """Median ESP32 - phone RSSI over APs both heard, for this one spot."""
    phone_files = list(PHONE_DIR.glob(f"{location_id:02d}_*.csv"))
    if not phone_files:
        return
    phone = pd.read_csv(phone_files[0])
    phone = phone[phone["Frequency"] < 3000]
    esp = pd.read_csv(path)
    p = phone.groupby(phone["BSSID"].str.lower())["RSSI"].agg(["mean", "count"])
    e = esp.groupby("BSSID")["RSSI"].agg(["mean", "count"])
    p_heard = p[p["count"] >= 0.3 * phone["Timestamp"].nunique()]
    e_heard = e[e["count"] >= 0.3 * esp["Scan"].nunique()]
    common = p_heard.index.intersection(e_heard.index)
    if len(common) == 0:
        print("  No AP heard reliably by both. Is this the right spot?")
        return
    diff = e_heard.loc[common, "mean"] - p_heard.loc[common, "mean"]
    print(f"  vs phone: ESP32 = phone {diff.median():+.1f} dB (median over {len(common)} APs, "
          f"spread +-{diff.std():.1f})")
    print(f"  APs heard: phone {len(p_heard)}, ESP32 {len(e_heard)}, both {len(common)}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--port")
    args = parser.parse_args()
    port = args.port or find_port()

    # DTR/RTS low so opening the port doesn't reset the ESP32 mid-recording
    link = serial.Serial()
    link.port, link.baudrate, link.timeout = port, BAUD, 1
    link.dtr = link.rts = False
    link.open()
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    print(f"Listening on {port}. Takes go to {OUT_DIR.relative_to(REPO)}/  (Ctrl+C to stop)")

    rows: list[str] = []
    while True:
        line = link.readline().decode("utf-8", errors="replace").rstrip("\r\n")
        if not line:
            continue
        if line.startswith("@take_start,"):
            rows = []
        elif line.startswith("@row,"):
            rows.append(line[len("@row,"):] + "\n")
        elif line.startswith("@take_end,"):
            _, location_id, take, result = line.split(",")
            if result != "saved" or not rows:
                print(f"Take {take} of spot #{location_id} cancelled, nothing saved")
                continue
            label = next(csv.reader(io.StringIO(rows[0])))[6]
            name = f"{int(location_id):02d}_{re.sub(r'[^A-Za-z0-9-]+', '_', label).strip('_')}_take{take}.csv"
            path = OUT_DIR / name
            path.write_text(HEADER + "".join(rows))
            print(f"Saved {path.relative_to(REPO)} ({len(rows)} rows)")
            quick_compare(path, int(location_id))
        else:
            print(f"  esp32> {line}")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
