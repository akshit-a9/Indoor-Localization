import json
from typing import Any, Dict, List

import joblib

from app import config


class Artifacts:
    def __init__(self) -> None:
        self.feature_bssids: List[str] = []
        self.label_map: Dict[int, str] = {}
        self.fallback_scaler: Any = None
        self.models: Dict[str, Any] = {}
        self.scalers: Dict[str, Any] = {}
        self.registry: Dict[str, Dict[str, Any]] = {}
        self._loaded = False

    @property
    def is_loaded(self) -> bool:
        return self._loaded

    def load(self) -> None:
        if self._loaded:
            return

        self.feature_bssids = [
            b.lower()
            for b in json.loads(config.FEATURE_BSSIDS_PATH.read_text(encoding="utf-8"))
        ]

        raw_map = json.loads(config.LABEL_MAP_PATH.read_text(encoding="utf-8"))
        self.label_map = {int(k): v for k, v in raw_map.items()}

        self.fallback_scaler = joblib.load(config.SCALER_PATH)

        self.registry = json.loads(
            config.MODELS_REGISTRY_PATH.read_text(encoding="utf-8")
        )
        self.models = {}
        self.scalers = {}
        for name, meta in self.registry.items():
            bundle = joblib.load(config.MODELS_DIR / meta["file"])
            # joblib files are bundles: {'model', 'scaler', 'feature_bssids', ...}
            self.models[name] = bundle["model"] if isinstance(bundle, dict) else bundle
            self.scalers[name] = bundle.get("scaler") if isinstance(bundle, dict) else None

        self._loaded = True

    def get_model(self, name: str):
        if name not in self.models:
            raise KeyError(name)
        return self.models[name], self.registry[name]

    def get_scaler(self, name: str):
        return self.scalers.get(name) or self.fallback_scaler


artifacts = Artifacts()
