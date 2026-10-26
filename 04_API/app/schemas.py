from typing import List, Optional
from pydantic import BaseModel


class ScanItem(BaseModel):
    bssid: str
    rssi: float


class PredictRequest(BaseModel):
    scans: List[ScanItem]
    model: Optional[str] = None


class PredictResponse(BaseModel):
    label: str
    class_index: int
    probabilities: Optional[List[float]] = None
    ap_detected: int
    ap_total: int
    model: str


class ModelEntry(BaseModel):
    name: str
    file: str
    test_accuracy: float
    val_accuracy: float
    needs_scaling: bool


class HealthResponse(BaseModel):
    status: str
    loaded_models: List[str]
    default_model: str
    feature_count: int
    label_count: int
