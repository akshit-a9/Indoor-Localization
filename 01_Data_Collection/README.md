# 01_Data_Collection

Flutter app used on Android to record labeled WiFi scans for the fingerprint
dataset. Each session writes one CSV that gets dropped into
[../02_Data/01_Wifi_Daytime/](../02_Data/01_Wifi_Daytime/).

## Layout

```
01_Data_Collection/
├── lib/
│   ├── main.dart              App entry point
│   ├── home_page.dart         UI: pick a label, start/stop a recording session
│   ├── permissions.dart       Runtime permission helpers (location, WiFi)
│   └── sensor_manager.dart    Scan scheduling, CSV writing, foreground service glue
├── android/                   Android Gradle project
├── ios/                       iOS scaffold (not used for collection)
├── test/
└── pubspec.yaml
```

## Key dependencies

- `wifi_scan` — scan results from the Android WiFi stack.
- `permission_handler` — runtime permission prompts.
- `flutter_background_service` + `flutter_local_notifications` — keep scanning
  while the screen is off.
- `path_provider`, `csv`, `share_plus`, `flutter_email_sender` — write the
  recorded CSV and ship it off the device.
- `sensors_plus` — IMU readings collected alongside the WiFi scans for future
  modalities.

## Recording a session

1. Plug in an Android device with USB debugging enabled.
2. From this folder:

   ```bash
   flutter pub get
   flutter run
   ```

3. Grant location and nearby-devices permissions when prompted (Android requires
   them for WiFi scanning).
4. Pick a location label, start the session, and stand at the labeled point for
   the recording window.
5. Stop the session and export the CSV (share sheet / email).
6. Save the file as
   `../02_Data/01_Wifi_Daytime/<index>_<location_label>.csv`.

## Notes

- Android throttles WiFi scans to roughly four per two minutes on most devices.
  The collector requests scans at the throttled rate and keeps running in the
  background to gather enough samples per location.
- The CSV schema is tolerant — preprocessing only requires `bssid` and `rssi`
  columns, so additional sensor channels can be appended without breaking the
  pipeline.
