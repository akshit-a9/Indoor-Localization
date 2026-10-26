# 04_API

FastAPI service that loads the trained scikit-learn models from
`02_Data/02_Processed_Wifi_Daytime/` and serves indoor-location predictions over
HTTP.

## Layout

```
04_API/
├── app/
│   ├── main.py            FastAPI app, route definitions, lifespan loader
│   ├── config.py          Paths to artifacts and runtime constants
│   ├── schemas.py         Pydantic request/response models
│   └── services/
│       ├── artifacts.py   Loads models, scaler, label map, feature BSSIDs
│       └── inference.py   Builds the feature vector and runs prediction
├── tests/                 Pytest suite (FastAPI TestClient)
├── conftest.py            Test config
├── requirements.txt
├── run.sh                 Linux/macOS launcher (activates ../.venv)
└── run.ps1                Windows PowerShell launcher
```

## Running

From the repo root:

```bash
python -m venv .venv
source .venv/bin/activate                     # Windows: .venv\Scripts\activate
pip install -r 04_API/requirements.txt
./04_API/run.sh                               # Windows: ./04_API/run.ps1
```

The launchers expect the virtualenv at `<repo_root>/.venv` and start uvicorn on
`0.0.0.0:8000`. Interactive docs are at `http://localhost:8000/docs`.

## Endpoints

| Method | Path         | Purpose                                                |
|--------|--------------|--------------------------------------------------------|
| GET    | `/health`    | Readiness, loaded models, feature/label counts.        |
| GET    | `/models`    | Registry entries with `test_accuracy`, `val_accuracy`. |
| GET    | `/locations` | `{class_index: location_label}` map.                   |
| POST   | `/predict`   | Predict a location from a list of WiFi scans.          |

### `POST /predict`

Request:

```json
{
  "scans": [
    { "bssid": "70:ea:1a:22:a9:4e", "rssi": -55 },
    { "bssid": "70:ea:1a:22:a9:41", "rssi": -60 }
  ],
  "model": "Random Forest"
}
```

- `model` is optional; defaults to `Random Forest`. An unknown name returns 400.
- BSSIDs are case-insensitive.
- BSSIDs not present in `feature_bssids.json` are ignored.
- Features missing from the request are filled with `-100 dBm`.

Response:

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

`probabilities` is `null` when the underlying model does not expose
`predict_proba`.

## Artifacts

The server reads from `02_Data/02_Processed_Wifi_Daytime/` at startup:

- `models/models_registry.json` — model list and metadata.
- `models/*.joblib` — model bundles. Models with `needs_scaling: true` go
  through the scaler before prediction.
- `splits/feature_bssids.json` — defines the order of the feature vector.
- `splits/label_map.json` — class index to location label.
- `splits/scaler.joblib` — fallback scaler used when a bundle has none.

To roll out a retrained model, rerun the notebooks in
[../03_Training/](../03_Training/) and restart the server.

## Tests

```bash
cd 04_API
pytest
```

The tests use `fastapi.testclient.TestClient`, which exercises the real
artifact loader against the files in `02_Data/02_Processed_Wifi_Daytime/`.
