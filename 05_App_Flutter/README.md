# 05_App_Flutter

Flutter demo that runs the trained classifiers on-device with ONNX Runtime,
shows the predicted location on the floor map, and can record walk-test CSVs
for offline evaluation.

## Layout

```
05_App_Flutter/
├── lib/
│   ├── main.dart                        App entry point
│   ├── home_page.dart                   Live prediction UI with map overlay
│   ├── walk_test_page.dart              Walk-test recorder (writes CSV)
│   ├── models/
│   │   └── localization_result.dart     Prediction result type
│   ├── services/
│   │   ├── wifi_scanner.dart            Wraps wifi_scan, handles throttling
│   │   ├── inference_engine.dart        Loads ONNX models, builds feature vector
│   │   └── coordinates_service.dart     Reads Coordinates.csv for the map plot
│   └── widgets/
│       └── location_map.dart            Map + predicted-point widget
├── assets/
│   ├── models/                          ONNX models, registry, label map, BSSIDs
│   └── Maps/                            Floor plans and Coordinates.csv
├── android/, ios/, linux/, macos/, windows/, web/
└── pubspec.yaml
```

## Key dependencies

- `wifi_scan` — WiFi scan results on Android.
- `onnxruntime` (1.4.1) — on-device inference.
- `permission_handler` — runtime permissions.
- `path_provider`, `share_plus` — write and export walk-test CSVs.

## Bundled assets

`assets/models/` contains a self-sufficient copy of the inference artifacts so
the app does not need the API at runtime:

- `Random_Forest.onnx`, `Gradient_Boosting.onnx`, `kNN.onnx`, `SVM.onnx`
- `models_registry.json` — metadata, mirrors the training-side registry.
- `feature_bssids.json` — BSSID order used to build the feature vector.
- `label_map.json` — class index to location label.

`assets/Maps/` mirrors [../02_Data/03_Map_Data/](../02_Data/03_Map_Data/).

After retraining + ONNX export
([../03_Training/04_Export_ONNX.ipynb](../03_Training/04_Export_ONNX.ipynb)),
copy the new `.onnx` files and updated registry/label/BSSID JSONs into
`assets/models/` and rebuild.

## Running

From this folder:

```bash
flutter pub get
flutter run
```

The app:

1. Asks for location and nearby-devices permissions.
2. Starts WiFi scans on a timer.
3. Builds the 149-element RSSI feature vector (missing APs filled with -100 dBm).
4. Runs the selected ONNX model and renders the predicted point on the map.

## Walk tests

The walk-test page records every prediction to a CSV during a session and
exports it via the share sheet. Save the exported file as
[../02_Data/04_Testing/walk_test_<YYYY-MM-DD>.csv](../02_Data/04_Testing/), then rerun
[../03_Training/05_Testing_Statistics.ipynb](../03_Training/05_Testing_Statistics.ipynb)
to refresh the evaluation figures.
