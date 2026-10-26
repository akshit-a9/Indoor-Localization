from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
ARTIFACTS_DIR = REPO_ROOT / "02_Data" / "02_Processed_Wifi_Daytime"
MODELS_DIR = ARTIFACTS_DIR / "models"
SPLITS_DIR = ARTIFACTS_DIR / "splits"

MODELS_REGISTRY_PATH = MODELS_DIR / "models_registry.json"
FEATURE_BSSIDS_PATH = SPLITS_DIR / "feature_bssids.json"
LABEL_MAP_PATH = SPLITS_DIR / "label_map.json"
SCALER_PATH = SPLITS_DIR / "scaler.joblib"

DEFAULT_MODEL = "Random Forest"
MISSING_RSSI = -100.0
