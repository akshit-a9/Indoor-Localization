# 02_Data

All datasets, intermediate artifacts and trained model outputs used by the
project.

## Layout

```
02_Data/
├── 01_Wifi_Daytime/             Raw labeled WiFi scans (one CSV per location)
├── 02_Processed_Wifi_Daytime/   Fingerprints, splits, scaler, label map, trained models
├── 03_Map_Data/                 Floor plans and pixel coordinates of each labeled point
├── 04_Testing/                  Live walk-test recordings and evaluation figures
└── 05_Wifi_ESP32/               ESP32 scans of the same spots, from 07_ESP32_Collector
```

## Conventions

- Floors are encoded as `MAP = 0` (ground floor) and `MAP = 1` (first floor).
- Locations are referenced both by an integer class index and a human-readable
  label; the canonical mapping lives in
  [02_Processed_Wifi_Daytime/splits/label_map.json](02_Processed_Wifi_Daytime/splits/label_map.json).
- Missing RSSI values are filled with `-100 dBm` everywhere downstream of
  preprocessing.
- BSSIDs are normalized to lowercase before being used as feature keys.

See each subfolder's `README.md` for the file-level details.
