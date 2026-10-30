# 03_Training

End-to-end WiFi-fingerprint pipeline. The notebooks are numbered and intended
to be run in order; each one consumes the artifacts produced by the previous
step.

## Pipeline

| # | Notebook | Inputs | Outputs |
|---|----------|--------|---------|
| 01 | [01_Data_Preprocessing.ipynb](01_Data_Preprocessing.ipynb) | `02_Data/01_Wifi_Daytime/*.csv` | `02_Data/02_Processed_Wifi_Daytime/fingerprints.csv`, EDA plots |
| 02 | [02_Feature_Extraction.ipynb](02_Feature_Extraction.ipynb) | `fingerprints.csv` | `splits/` (train/val/test arrays, scaler, label map, feature BSSIDs) |
| 03 | [03_Model_Training.ipynb](03_Model_Training.ipynb) | `splits/` | `models/*.joblib`, `models_registry.json`, evaluation plots |
| 04 | [04_Export_ONNX.ipynb](04_Export_ONNX.ipynb) | `models/*.joblib` | `models/*.onnx` |
| 05 | [05_Testing_Statistics.ipynb](05_Testing_Statistics.ipynb) | `02_Data/04_Testing/walk_test_*.csv`, `02_Data/03_Map_Data/` | `02_Data/04_Testing/figures/` |
| 06 | [06_Train_2_4GHz_Model.py](06_Train_2_4GHz_Model.py) | `splits/`, raw CSVs (for each BSSID's band) | `models/Random_Forest_2_4GHz.joblib`, registry entry `Random Forest 2.4GHz` |
| 07 | [07_Compare_ESP32_vs_Phone.py](07_Compare_ESP32_vs_Phone.py) | `02_Data/05_Wifi_ESP32/*.csv`, raw phone CSVs, `splits/` | `02_Data/05_Wifi_ESP32/analysis/` (`calibration.json`, figure) |

## Notes

- `06_Train_2_4GHz_Model.py` is a script, not a notebook. Run it from the repo root:
  `.venv/Scripts/python 03_Training/06_Train_2_4GHz_Model.py`. It trains a Random Forest
  that ignores 5 GHz APs, for 2.4 GHz-only clients such as the ESP32 in the WayPoint repo.
- `07_Compare_ESP32_vs_Phone.py` is also a script: `python3 03_Training/07_Compare_ESP32_vs_Phone.py`.
  It compares ESP32 scans recorded with [`07_ESP32_Collector`](../07_ESP32_Collector/) to the phone
  scans of the same spots: the dB offset, which APs the ESP32 misses, and whether shifting the phone
  data onto the ESP32's scale fixes localization. Calibration is fitted leave-one-spot-out, so record
  at least 3 spots before trusting the accuracy numbers.
- Notebook 01 controls AP-coverage filtering. Adjusting the threshold changes
  the `feature_bssids.json` produced by notebook 02, which in turn invalidates
  every downstream artifact (models, ONNX exports, the API and the Flutter app
  assets) — always rerun the full chain after touching it.
- Notebook 03 writes `models_registry.json`; this is the file the FastAPI server
  reads at startup to discover available models.
- Notebook 04 copies its ONNX outputs and JSONs into
  [`06_App_Android/app/src/main/assets/models/`](../06_App_Android/app/src/main/assets/models/).
  Copy them into [`05_App_Flutter/assets/models/`](../05_App_Flutter/assets/models/) by hand.
- Notebook 05 is the only notebook that depends on field data
  (`02_Data/04_Testing/`); the first four can be rerun without leaving the desk.
