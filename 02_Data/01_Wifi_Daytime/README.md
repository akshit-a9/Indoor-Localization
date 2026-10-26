# 01_Wifi_Daytime

Raw WiFi scans collected on Android during daytime, one CSV per labeled
location.

## File naming

`<index>_<location_label>.csv`

The numeric prefix is collection order, not a class index. The location label
maps onto the canonical names in
[../02_Processed_Wifi_Daytime/splits/label_map.json](../02_Processed_Wifi_Daytime/splits/label_map.json).

## CSV schema

Each row corresponds to one access point seen in one scan tick produced by the
[01_Data_Collection](../../01_Data_Collection/) Flutter app. Typical columns:

- `timestamp` — scan timestamp (ms since epoch or ISO).
- `ssid` — network name (kept for inspection only, not used as a feature).
- `bssid` — AP MAC address (lowercased downstream, used as the feature key).
- `rssi` — signal strength in dBm.

The exact column set is whatever the collector wrote at the time; preprocessing
is tolerant of extra columns and only relies on `bssid` and `rssi`.

## Coverage

52 locations across two floors. These files are the only input to
`03_Training/01_Data_Preprocessing.ipynb`.
