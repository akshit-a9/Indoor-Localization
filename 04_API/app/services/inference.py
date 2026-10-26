from typing import Any, Iterable, List, Optional, Tuple

import numpy as np

from app import config
from app.services.artifacts import artifacts


def _scan_field(scan: Any, name: str) -> Any:
    if hasattr(scan, name):
        return getattr(scan, name)
    return scan[name]


def build_feature_vector(scans: Iterable[Any]) -> Tuple[np.ndarray, int]:
    rssi_map: dict[str, float] = {}
    for s in scans:
        bssid = str(_scan_field(s, "bssid")).lower()
        rssi = float(_scan_field(s, "rssi"))
        rssi_map[bssid] = rssi

    vec = np.array(
        [rssi_map.get(b, config.MISSING_RSSI) for b in artifacts.feature_bssids],
        dtype=np.float32,
    )
    detected = sum(1 for b in artifacts.feature_bssids if b in rssi_map)
    return vec, detected


def predict(scans: Iterable[Any], model_name: str) -> dict:
    model, meta = artifacts.get_model(model_name)
    vec, detected = build_feature_vector(scans)
    X = vec.reshape(1, -1)
    if meta.get("needs_scaling"):
        X = artifacts.get_scaler(model_name).transform(X)

    class_index = int(model.predict(X)[0])

    probabilities: Optional[List[float]] = None
    if hasattr(model, "predict_proba"):
        try:
            probabilities = [float(p) for p in model.predict_proba(X)[0]]
        except Exception:
            probabilities = None

    label = artifacts.label_map.get(class_index, f"Unknown ({class_index})")
    return {
        "label": label,
        "class_index": class_index,
        "probabilities": probabilities,
        "ap_detected": detected,
        "ap_total": len(artifacts.feature_bssids),
        "model": model_name,
    }
