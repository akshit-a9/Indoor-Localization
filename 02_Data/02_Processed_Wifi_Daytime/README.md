# 02_Processed_Wifi_Daytime

Processed fingerprints, train/val/test splits, EDA figures, and trained models.
This folder is the source of truth that both `04_API/` and
`05_App_Flutter/` consume.

## Top-level files

- `fingerprints.csv` — wide RSSI matrix, one row per scan, columns indexed by
  BSSID, plus a `label` column. Output of `01_Data_Preprocessing.ipynb`.
- `ap_coverage.csv` — per-AP coverage statistics used to filter low-signal BSSIDs.
- `*.png` — EDA plots (`eda_raw`, `class_balance`, `rssi_heatmap`,
  `feature_stats`, `scans_per_location`, `ap_coverage*`).

## `splits/`

Train/val/test artifacts produced by `02_Feature_Extraction.ipynb`.

- `train_X.npy`, `train_y.npy`, `val_X.npy`, `val_y.npy`, `test_X.npy`, `test_y.npy`
  — numeric splits in feature order.
- `train_X_scaled.npy`, `val_X_scaled.npy`, `test_X_scaled.npy` — pre-scaled
  versions for models that need standardization (kNN, SVM).
- `scaler.joblib` — fitted `StandardScaler`. Used by the API as a fallback when
  a model bundle does not ship its own scaler.
- `feature_bssids.json` — ordered list of 149 BSSIDs that define the feature
  vector. Order matters; both training and inference must use it verbatim.
- `label_map.json` — `{class_index: location_label}`.
- `label_encoder.joblib` — fitted `LabelEncoder` used during training.
- `ap_stats.csv` — per-AP descriptive statistics over the training set.

## `models/`

Trained classifiers and their evaluation artifacts, written by
`03_Model_Training.ipynb` and `04_Export_ONNX.ipynb`.

- `models_registry.json` — registry consumed by the API. Each entry has:
  - `file` — joblib bundle filename.
  - `test_accuracy`, `val_accuracy`.
  - `needs_scaling` — whether the input must go through the scaler before
    `predict`.
- `kNN.joblib`, `Random_Forest.joblib`, `SVM.joblib`, `Gradient_Boosting.joblib`
  — joblib bundles used by the FastAPI server. Each is a dict containing at
  least `model`, and optionally `scaler` and `feature_bssids`.
- `*.onnx` — ONNX exports of the same models, shipped inside `05_App_Flutter/`.
- `confusion_random_forest.png`, `feature_importance.png`,
  `model_comparison.png`, `per_class_accuracy.png` — evaluation plots.

## Reproducing

The notebooks in [../../03_Training/](../../03_Training/) regenerate every
file in this folder from scratch given the raw CSVs in `../01_Wifi_Daytime/`.
