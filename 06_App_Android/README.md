# 06_App_Android

Native Android (Kotlin) demo of the localization pipeline. Runs the ONNX models
on-device using ONNX Runtime, the same way `05_App_Flutter` does, but without the
Flutter layer.

This module is the lightweight reference implementation; `05_App_Flutter` is the
one used for live walk tests and map visualization.

## Layout

```
06_App_Android/
├── app/
│   ├── src/main/
│   │   ├── java/com/example/demo/MainActivity.kt    Single-activity app
│   │   ├── assets/models/                           ONNX models + metadata
│   │   ├── res/                                     Layouts and resources
│   │   └── AndroidManifest.xml
│   ├── build.gradle.kts
│   └── proguard-rules.pro
├── gradle/, gradlew, gradlew.bat
├── build.gradle.kts
└── settings.gradle.kts
```

## What it does

`MainActivity` performs one full prediction cycle per button tap:

1. Loads `assets/models/models_registry.json`, `feature_bssids.json` and
   `label_map.json` at startup, populates a model picker, and creates an
   ONNX Runtime session for the selected model.
2. Requests `ACCESS_FINE_LOCATION` / `ACCESS_COARSE_LOCATION` (required for
   `WifiManager.getScanResults`).
3. Reads the latest WiFi scan results, builds the 149-element RSSI feature
   vector (missing APs filled with `-100 dBm`), and runs the ONNX session.
4. Looks up the predicted class index in `label_map.json` and shows the label.

The ONNX input tensor name is `rssi_input` and the output is a `LongArray` of
class indices — both are fixed by `04_Export_ONNX.ipynb`.

## Bundled assets

`app/src/main/assets/models/` mirrors the trained ONNX bundle:

- `Random_Forest.onnx`, `Gradient_Boosting.onnx`, `kNN.onnx`, `SVM.onnx`
- `models_registry.json` — note the `onnx_file` key (this module reads
  `onnx_file`, not the `file` key the Python API uses).
- `feature_bssids.json`, `label_map.json`

After retraining, refresh these files from
[../02_Data/02_Processed_Wifi_Daytime/models/](../02_Data/02_Processed_Wifi_Daytime/models/)
and
[../02_Data/02_Processed_Wifi_Daytime/splits/](../02_Data/02_Processed_Wifi_Daytime/splits/).

## Building

```bash
./gradlew assembleDebug                       # Windows: gradlew.bat assembleDebug
./gradlew installDebug                        # install on a connected device
```
