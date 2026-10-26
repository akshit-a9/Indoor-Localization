import numpy as np

from app import config
from app.services import inference
from app.services.artifacts import artifacts


def test_random_forest_pipeline_accuracy_on_test_split() -> None:
    artifacts.load()
    test_X = np.load(config.SPLITS_DIR / "test_X.npy")
    test_y = np.load(config.SPLITS_DIR / "test_y.npy")
    bssids = artifacts.feature_bssids

    correct = 0
    for row, y in zip(test_X, test_y):
        scans = [
            {"bssid": bssids[i], "rssi": float(row[i])}
            for i in range(len(bssids))
            if float(row[i]) != config.MISSING_RSSI
        ]
        result = inference.predict(scans, "Random Forest")
        if result["class_index"] == int(y):
            correct += 1

    accuracy = correct / len(test_y)
    assert accuracy >= 0.94, f"Random Forest pipeline accuracy {accuracy:.4f} below 0.94"
