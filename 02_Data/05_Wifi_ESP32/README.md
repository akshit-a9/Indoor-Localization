# 05_Wifi_ESP32

WiFi scans recorded by the ESP32 with
[07_ESP32_Collector](../../07_ESP32_Collector/), at the same spots as
[01_Wifi_Daytime](../01_Wifi_Daytime/), using the same 90 s, turn-45°-right
protocol.

## Files

- `esp32_scans*.csv`: downloaded from the collector page ("Download all").
- `<id>_<label>_take<N>.csv`: saved by `07_ESP32_Collector/laptop_logger.py`.
- `analysis/`: output of `03_Training/07_Compare_ESP32_vs_Phone.py`.

Both CSV kinds can sit here together; the comparison script drops takes that
appear twice.

## CSV schema

The phone collector's columns first, so the same preprocessing works:

| Column | Notes |
|--------|-------|
| `Timestamp` | Phone's local time at the start of each scan |
| `SSID`, `BSSID`, `RSSI` | BSSID lowercase, RSSI in dBm |
| `Frequency` | MHz, from the channel (2.4 GHz only) |
| `Activity Label` | Always `STATIONARY` |
| `Location Label` | Canonical label from `label_map.json` |
| `Take` | 1, 2, ... per spot, if a spot was recorded more than once |
| `Scan` | Scan number within the take |
| `Heading` | 0-7: how many 45° right turns had been made when the scan started |
| `Device` | ESP32 MAC address |
