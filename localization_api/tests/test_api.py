from fastapi.testclient import TestClient

from app.main import app


def test_health_reports_loaded_artifacts() -> None:
    with TestClient(app) as client:
        r = client.get("/health")
        assert r.status_code == 200
        data = r.json()
        assert data["status"] == "ok"
        assert data["feature_count"] == 149
        assert data["label_count"] == 52
        assert data["default_model"] == "Random Forest"
        assert "Random Forest" in data["loaded_models"]


def test_models_lists_registry_entries() -> None:
    with TestClient(app) as client:
        r = client.get("/models")
        assert r.status_code == 200
        names = [m["name"] for m in r.json()]
        assert {"kNN", "Random Forest", "SVM", "Gradient Boosting"} <= set(names)


def test_locations_returns_full_label_map() -> None:
    with TestClient(app) as client:
        r = client.get("/locations")
        assert r.status_code == 200
        body = r.json()
        assert body["0"] == "1st Stairs midway"
        assert len(body) == 52


def test_predict_with_empty_scans_uses_default_model() -> None:
    with TestClient(app) as client:
        r = client.post("/predict", json={"scans": []})
        assert r.status_code == 200
        data = r.json()
        assert data["model"] == "Random Forest"
        assert data["ap_detected"] == 0
        assert data["ap_total"] == 149
        assert isinstance(data["class_index"], int)
        assert isinstance(data["label"], str)


def test_predict_unknown_model_returns_400() -> None:
    with TestClient(app) as client:
        r = client.post("/predict", json={"scans": [], "model": "NotAModel"})
        assert r.status_code == 400


def test_predict_lowercases_bssid_input() -> None:
    with TestClient(app) as client:
        r = client.post(
            "/predict",
            json={
                "scans": [
                    {"bssid": "70:EA:1A:22:A9:4E", "rssi": -55},
                    {"bssid": "70:ea:1a:22:a9:41", "rssi": -60},
                ]
            },
        )
        assert r.status_code == 200
        data = r.json()
        assert data["ap_detected"] == 2


def test_predict_with_2_4ghz_model() -> None:
    with TestClient(app) as client:
        r = client.post(
            "/predict",
            json={
                "scans": [{"bssid": "70:ea:1a:22:a9:41", "rssi": -60}],
                "model": "Random Forest 2.4GHz",
            },
        )
        assert r.status_code == 200
        data = r.json()
        assert data["model"] == "Random Forest 2.4GHz"
        assert len(data["probabilities"]) == 52
