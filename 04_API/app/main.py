from contextlib import asynccontextmanager
from typing import Dict, List

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware

from app import config
from app.schemas import (
    HealthResponse,
    ModelEntry,
    PredictRequest,
    PredictResponse,
)
from app.services import inference
from app.services.artifacts import artifacts


@asynccontextmanager
async def lifespan(_: FastAPI):
    artifacts.load()
    yield


app = FastAPI(title="Indoor Localisation API", lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=False,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.get("/health", response_model=HealthResponse)
def health() -> HealthResponse:
    return HealthResponse(
        status="ok" if artifacts.is_loaded else "loading",
        loaded_models=list(artifacts.models.keys()),
        default_model=config.DEFAULT_MODEL,
        feature_count=len(artifacts.feature_bssids),
        label_count=len(artifacts.label_map),
    )


@app.get("/models", response_model=List[ModelEntry])
def list_models() -> List[ModelEntry]:
    return [ModelEntry(name=name, **meta) for name, meta in artifacts.registry.items()]


@app.get("/locations", response_model=Dict[int, str])
def list_locations() -> Dict[int, str]:
    return artifacts.label_map


@app.post("/predict", response_model=PredictResponse)
def predict(req: PredictRequest) -> PredictResponse:
    model_name = req.model or config.DEFAULT_MODEL
    if model_name not in artifacts.models:
        raise HTTPException(status_code=400, detail=f"Unknown model: {model_name}")
    result = inference.predict(req.scans, model_name)
    return PredictResponse(**result)
