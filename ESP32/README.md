# ESP32

ESP32 client for [localization_api](../localization_api/). The board scans
2.4 GHz WiFi, POSTs the scan to `/predict`, shows the predicted location on a
1.3" I2C OLED and pulses an RGB LED (blue while scanning, green on a
prediction, red on any error). An MPU6050 is streamed over serial at 100 Hz to
build the dataset for the navigation step.

## Layout

```
ESP32/
├── hw_check/        Bench bring-up: LED, I2C scan, MPU6050 readout, OLED driver modes
├── localizer/       Main firmware
│   ├── localizer.ino
│   ├── config.h           Pins, OLED mode, API model, scan/IMU settings
│   └── secrets.example.h  Copy to secrets.h (git-ignored) and fill in
└── tools/
    └── serial_logger.py   Records IMU + scans + predictions to Data/05_ESP32_Sessions/
```

## Why a separate 2.4 GHz model

The Android collector saw both bands; 73 of the 149 feature BSSIDs are 5 GHz,
which a classic ESP32 cannot receive. With those columns stuck at -100 dBm the
stock Random Forest drops from 94% to 40% test accuracy.
[../Notebooks/Wifi/06_Train_2_4GHz_Model.py](../Notebooks/Wifi/06_Train_2_4GHz_Model.py)
retrains it with the 5 GHz columns masked and registers it in the API as
`Random Forest 2.4GHz` (91.0% test / 95.2% val). The feature vector is
unchanged, so the API code needed no changes; the firmware just asks for that
model.

RSSI read by the ESP32 antenna is not calibrated against the phone. If live
accuracy is poor, compare `aps.csv` from a logged session against the raw
Android CSVs for the same spot and set `RSSI_OFFSET_DB` in `config.h`.

## Wiring (ESP32 DevKit / WROOM-32, e.g. ESP32-32X)

Boards like the ESP32-32X label pins `Dxx`; `Dxx` is GPIOxx. On the 30-pin
layout D25, D26, D27 are adjacent on one edge; D22, D21, D19, D18, 3V3 and GND
are on the other. Never use VIN (5 V).

| Part            | Pin      | ESP32            |
|-----------------|----------|------------------|
| OLED 1.3" I2C   | VCC      | 3V3              |
|                 | GND      | GND              |
|                 | SCL      | D22 (GPIO22)          |
|                 | SDA      | D21 (GPIO21)          |
| MPU6050 (GY-521)| VCC      | 3V3              |
|                 | GND      | GND              |
|                 | SCL      | D18 (GPIO18)          |
|                 | SDA      | D19 (GPIO19)          |
|                 | AD0      | GND (address 0x68) |
| RGB LED         | R        | D27 (direct, or 100 Ω–1 kΩ) |
|                 | G        | D26 (direct, or 100 Ω–1 kΩ) |
|                 | B        | D25 (direct, or 100 Ω–1 kΩ) |
|                 | common   | GND (common cathode) or 3V3 (common anode, set `LED_COMMON_ANODE 1`) |

Without series resistors keep `LED_NO_RESISTORS 1`: the LED pins are set to
their weakest drive strength (~5 mA), which protects the LED and the GPIO.

OLED pin order differs between modules (VCC/GND are often swapped), so follow
the silkscreen. The OLED and the MPU6050 sit on separate I2C buses so that a
display refresh never delays an IMU sample.

## Toolchain

Arduino IDE 2 and/or arduino-cli with:

- Board package `esp32` by Espressif (board URL
  `https://espressif.github.io/arduino-esp32/package_esp32_index.json`),
  board **ESP32 Dev Module** (`esp32:esp32:esp32`).
- Libraries `U8g2` and `ArduinoJson` (v7).

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 ESP32/hw_check
arduino-cli upload  --fqbn esp32:esp32:esp32 -p COM5 ESP32/hw_check
```

## Bring-up

1. Flash `hw_check`, open the serial monitor at 115200. Expect `0x3C` on bus 0
   and `0x68` on bus 1, the LED cycling R/G/B/white, and the OLED stepping
   through modes 1–6. Pick the mode with a solid box and no stripes and set
   `OLED_MODE` in `localizer/config.h`. If none is clean the panel really is
   damaged; keep `OLED_ROW_DOUBLE 1` so text stays readable on alternate lines.
2. Start the API on the laptop (`localization_api/run.ps1`). Allow Python
   through Windows Firewall on private networks.
3. Put the ESP32 and the laptop on the same 2.4 GHz network. Simplest:
   Windows Mobile Hotspot with band set to 2.4 GHz; the laptop is then
   `192.168.137.1`.
4. `cp localizer/secrets.example.h localizer/secrets.h`, fill in SSID,
   password and `API_BASE_URL`, flash `localizer`.

## Serial protocol (460800 baud)

| Prefix | Fields |
|--------|--------|
| `IMU`  | `t_us,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps` |
| `SCAN` | `t_us,cycle,n_aps` |
| `AP`   | `t_us,cycle,bssid,rssi,channel` |
| `PRED` | `t_us,cycle,ok,class_index,confidence,ap_detected,ap_seen,latency_ms,"label_or_error"` |
| `ERR`  | `t_us,message` |
| `#`    | free-text log |

`t_us` is microseconds since boot on the ESP32, shared by all record types,
so IMU samples and WiFi fixes line up without any host-side clock sync.

## Collecting IMU data

```bash
.venv/Scripts/python ESP32/tools/serial_logger.py --note "floor0 corridor loop"
```

Walk with the breadboard held flat. Each time you pass a labelled spot, type
its name and press Enter; those marks are the ground truth for evaluating
pedestrian dead reckoning later.
