# Indoor Localization

WiFi-fingerprint based indoor localization for two floors of an academic building.
The pipeline collects RSSI scans on Android, trains classical ML classifiers over
the fingerprints, and serves predictions either through a Python REST API or
on-device via ONNX Runtime in a Flutter app.

## Repository structure

```
.
├── 01_Data_Collection/   Flutter app used to record labeled WiFi scans on Android
├── 02_Data/              Raw scans, processed fingerprints, trained models, maps, walk tests
├── 03_Training/          Notebooks + scripts: preprocess, features, train, ONNX export, evaluate
├── 04_API/               FastAPI service that serves the trained scikit-learn models
├── 05_App_Flutter/       Flutter app running the ONNX models on-device, with map overlay + walk tests
└── 06_App_Android/       Native Android (Kotlin) reference app, same ONNX models
```

Folders are numbered in pipeline order. The ESP32 client that queries the API
(and the navigation work built on it) lives in its own repo,
[WayPoint](https://github.com/akshit-a9/WayPoint).
## Pipeline overview

1. **Collect** — `01_Data_Collection/` records timestamped WiFi scans tagged with a
   location label and writes them to CSV (`02_Data/01_Wifi_Daytime/`).
2. **Preprocess & engineer features** — `03_Training/01_Data_Preprocessing.ipynb`
   and `02_Feature_Extraction.ipynb` build the fingerprint matrix indexed by BSSID,
   dropping low-coverage APs and producing train/val/test splits.
3. **Train** — `03_Model_Training.ipynb` trains kNN, SVM, Random Forest and
   Gradient Boosting classifiers, persisting them as `.joblib` bundles together with
   the scaler, label map and feature BSSID list under
   `02_Data/02_Processed_Wifi_Daytime/`.
4. **Export** — `04_Export_ONNX.ipynb` converts the same models to ONNX so they can
   run on-device.
5. **Serve** — `04_API/` loads the joblib bundles and exposes a
   `/predict` endpoint; `05_App_Flutter/` ships the ONNX models inside the app.
6. **Evaluate** — `05_Testing_Statistics.ipynb` analyses live walk-test data
   collected by the Flutter demo (`02_Data/04_Testing/`).

## Current status

- 52 labeled locations spanning the ground and first floors.
- 149 BSSIDs retained as features after coverage filtering.
- Held-out test accuracy on the trained models:

  | Model            | Test acc. | Val acc. | Needs scaling |
  |------------------|-----------|----------|---------------|
  | Random Forest    | 0.9415    | 0.9679   | no            |
  | Gradient Boosting| 0.9096    | 0.8877   | no            |
  | kNN              | 0.8777    | 0.8770   | yes           |
  | SVM              | 0.8670    | 0.8770   | yes           |

- Live walk-test results and figures are stored under `02_Data/04_Testing/`.

## Using the localization API

The API loads the trained joblib bundles directly from `02_Data/02_Processed_Wifi_Daytime/`
at startup and serves predictions over HTTP.

### Run locally

```bash
python -m venv .venv
source .venv/bin/activate                     # Windows: .venv\Scripts\activate
pip install -r 04_API/requirements.txt
./04_API/run.sh                               # Windows: ./04_API/run.ps1
```

The server listens on `http://localhost:8000`.

### Endpoints

| Method | Path         | Purpose                                         |
|--------|--------------|-------------------------------------------------|
| GET    | `/health`    | Readiness, loaded models, feature/label counts  |
| GET    | `/models`    | Available models with accuracy metadata         |
| GET    | `/locations` | Map of class index → location label             |
| POST   | `/predict`   | Predict a location from a list of WiFi scans    |

### Predict request

```json
POST /predict
{
  "scans": [
    { "bssid": "70:ea:1a:22:a9:4e", "rssi": -55 },
    { "bssid": "70:ea:1a:22:a9:41", "rssi": -60 }
  ],
  "model": "Random Forest"
}
```

`model` is optional and defaults to `Random Forest`. BSSIDs are case-insensitive;
unseen BSSIDs are ignored and missing features are filled with `-100 dBm`.

### Predict response

```json
{
  "label": "DLT8/1",
  "class_index": 19,
  "probabilities": [0.01, 0.00, ..., 0.84, ...],
  "ap_detected": 23,
  "ap_total": 149,
  "model": "Random Forest"
}
```

`probabilities` is omitted for models that do not expose `predict_proba`.

## Folder-level documentation

Each top-level folder contains its own `README.md` with the details specific to
that part of the project.
