"""Train a Random Forest that only relies on 2.4 GHz access points.

The classic ESP32 radio cannot hear 5 GHz APs, but 73 of the 149 feature
BSSIDs were recorded on 5 GHz by the Android collector. Feeding ESP32 scans
into the stock model leaves those columns at -100 dBm and test accuracy drops
from ~94% to ~40%.

This script keeps the same 149-column feature vector (so the API needs no code
changes) but forces every 5 GHz column to the missing value during training.
The resulting bundle is registered as "Random Forest 2.4GHz".

Run from the repo root:
    .venv/Scripts/python Notebooks/Wifi/06_Train_2_4GHz_Model.py
"""

import glob
import json
from pathlib import Path

import joblib
import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier

REPO = Path(__file__).resolve().parents[2]
RAW = REPO / "Data" / "01_Wifi_Daytime"
PROC = REPO / "Data" / "02_Processed_Wifi_Daytime"
SPLITS = PROC / "splits"
MODELS = PROC / "models"

MODEL_NAME = "Random Forest 2.4GHz"
MODEL_FILE = "Random_Forest_2_4GHz.joblib"
MISSING_RSSI = -100.0

feature_bssids = [b.lower() for b in json.loads((SPLITS / "feature_bssids.json").read_text())]
label_map = json.loads((SPLITS / "label_map.json").read_text())

# Band of each BSSID, taken from the most common frequency in the raw scans.
raw = pd.concat(pd.read_csv(f) for f in glob.glob(str(RAW / "*.csv")))
raw["BSSID"] = raw["BSSID"].str.lower()
freq = raw.groupby("BSSID")["Frequency"].agg(lambda s: s.mode()[0]).reindex(feature_bssids)
is_5ghz = (freq >= 3000).to_numpy()
print(f"Features: {len(feature_bssids)}  2.4GHz: {(~is_5ghz).sum()}  5GHz: {is_5ghz.sum()}")


def mask_5ghz(X: np.ndarray) -> np.ndarray:
    X = X.copy()
    X[:, is_5ghz] = MISSING_RSSI
    return X


X_train = mask_5ghz(np.load(SPLITS / "train_X.npy"))
X_val = mask_5ghz(np.load(SPLITS / "val_X.npy"))
X_test = mask_5ghz(np.load(SPLITS / "test_X.npy"))
y_train = np.load(SPLITS / "train_y.npy")
y_val = np.load(SPLITS / "val_y.npy")
y_test = np.load(SPLITS / "test_y.npy")

# Same hyperparameters as the stock Random Forest in 03_Model_Training.
model = RandomForestClassifier(n_estimators=300, n_jobs=-1, random_state=69)
model.fit(X_train, y_train)
val_acc = float((model.predict(X_val) == y_val).mean())

model.fit(np.vstack([X_train, X_val]), np.concatenate([y_train, y_val]))
test_acc = float((model.predict(X_test) == y_test).mean())
print(f"{MODEL_NAME}: val={val_acc:.4f} test={test_acc:.4f}")

joblib.dump(
    {
        "model": model,
        "scaler": None,
        "feature_bssids": feature_bssids,
        "label_map": label_map,
        "needs_scaling": False,
        "model_name": MODEL_NAME,
        "test_accuracy": test_acc,
        "n_features": len(feature_bssids),
        "n_classes": len(label_map),
        "rssi_missing": MISSING_RSSI,
        "band_mask_5ghz": is_5ghz.tolist(),
    },
    MODELS / MODEL_FILE,
)

registry_path = MODELS / "models_registry.json"
registry = json.loads(registry_path.read_text())
registry[MODEL_NAME] = {
    "file": MODEL_FILE,
    "test_accuracy": round(test_acc, 4),
    "val_accuracy": round(val_acc, 4),
    "needs_scaling": False,
}
registry_path.write_text(json.dumps(registry, indent=2))
print(f"Saved {MODEL_FILE} and updated {registry_path.name}")
