"""Compare ESP32 scans with the phone scans of the same spots.

Question: is the ESP32 just "the phone, shifted by N dB"? If so, the 52
locations of phone data can be reused for the ESP32 instead of recollecting
everything.

Input: ESP32 CSVs from 07_ESP32_Collector (download from the collector page,
or laptop_logger.py), saved under 02_Data/05_Wifi_ESP32/. Only spots present
in both datasets are compared; 3+ spots are needed for honest numbers.

What it reports (2.4 GHz APs only, the ESP32 cannot hear 5 GHz):
  1. RSSI relationship: per (spot, AP) mean RSSI on phone vs ESP32, the
     median offset and a linear fit esp = slope * phone + intercept.
  2. Hearing: how often the ESP32 hears an AP that the phone heard, by the
     phone's RSSI. This shows the ESP32's sensitivity floor.
  3. Does it help localization? Top-1 / top-3 accuracy on the ESP32 scans of
       a. the phone model fed raw ESP32 scans (what WayPoint does today)
       b. the phone model fed ESP32 scans shifted onto the phone scale
       c. a model trained on phone data shifted onto the ESP32 scale, with
          APs below the ESP32 floor dropped ("reuse the old data")
     For b and c the calibration is fitted without the spot being tested
     (leave-one-spot-out), so the accuracy is not flattered by its own data.

Outputs 02_Data/05_Wifi_ESP32/analysis/: calibration.json and figures.

Run from the repo root:
    python3 03_Training/07_Compare_ESP32_vs_Phone.py [--esp-dir DIR]
"""

import argparse
import glob
import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier

REPO = Path(__file__).resolve().parents[1]
PHONE_DIR = REPO / "02_Data" / "01_Wifi_Daytime"
ESP_DIR = REPO / "02_Data" / "05_Wifi_ESP32"
SPLITS = REPO / "02_Data" / "02_Processed_Wifi_Daytime" / "splits"

MISSING_RSSI = -100.0
MIN_HEARD = 0.3  # an AP counts as "heard at a spot" if it is in >= 30% of its scans


def load_scans(files: list[str], scan_cols: list[str]) -> pd.DataFrame:
    """One row per (scan, AP): label, scan, bssid, rssi. 2.4 GHz only."""
    df = pd.concat((pd.read_csv(f, encoding="utf-8-sig") for f in files), ignore_index=True)
    df = df[df["Frequency"] < 3000].copy()
    df["label"] = df["Location Label"].str.strip()
    df["bssid"] = df["BSSID"].str.lower()
    df["rssi"] = df["RSSI"].astype(float)
    df["scan"] = df[scan_cols].astype(str).agg("|".join, axis=1)
    return df[["label", "scan", "bssid", "rssi"]]


def per_ap(scans: pd.DataFrame) -> pd.DataFrame:
    """Per (label, bssid): mean RSSI when heard, and fraction of scans it was heard in."""
    n_scans = scans.groupby("label")["scan"].nunique()
    stats = scans.groupby(["label", "bssid"])["rssi"].agg(["mean", "count"]).reset_index()
    stats["heard"] = stats["count"] / stats["label"].map(n_scans)
    return stats.drop(columns="count")


def calibrate(pairs: pd.DataFrame, esp: pd.DataFrame) -> dict:
    """Fits the phone <-> ESP32 relationship on APs heard reliably by both."""
    both = pairs[(pairs["heard_phone"] >= MIN_HEARD) & (pairs["heard_esp"] >= MIN_HEARD)]
    slope, intercept = np.polyfit(both["mean_phone"], both["mean_esp"], 1)
    inv_slope, inv_intercept = np.polyfit(both["mean_esp"], both["mean_phone"], 1)
    return {
        "offset_db": float(np.median(both["mean_esp"] - both["mean_phone"])),  # esp ~= phone + offset
        "slope": float(slope), "intercept": float(intercept),                  # esp ~= slope*phone + intercept
        "inv_slope": float(inv_slope), "inv_intercept": float(inv_intercept),  # phone ~= inv_slope*esp + inv_intercept
        "esp_floor_dbm": float(np.percentile(esp["rssi"], 0.5)),               # weakest RSSI the ESP32 reports
        "n_pairs": int(len(both)),
    }


def to_matrix(scans: pd.DataFrame, bssids: list[str]) -> tuple[np.ndarray, np.ndarray]:
    """Wide fingerprint matrix (one row per scan) in feature order, and the labels."""
    wide = scans.pivot_table(index=["label", "scan"], columns="bssid", values="rssi", aggfunc="max")
    wide = wide.reindex(columns=bssids).fillna(MISSING_RSSI)
    return wide.to_numpy(dtype=np.float32), wide.index.get_level_values("label").to_numpy()


def shift_heard(X: np.ndarray, slope: float, intercept: float, floor: float | None = None) -> np.ndarray:
    """Applies slope*x + intercept to heard values; optionally drops those below floor."""
    heard = X > MISSING_RSSI
    out = np.where(heard, slope * X + intercept, MISSING_RSSI)
    if floor is not None:
        out[out < floor] = MISSING_RSSI
    return out


def train_rf(X: np.ndarray, y: np.ndarray) -> RandomForestClassifier:
    # Same hyperparameters as 06_Train_2_4GHz_Model.py
    return RandomForestClassifier(n_estimators=300, n_jobs=-1, random_state=69).fit(X, y)


def topk(model: RandomForestClassifier, X: np.ndarray, y_idx: np.ndarray) -> tuple[float, float]:
    proba = model.predict_proba(X)
    order = model.classes_[np.argsort(-proba, axis=1)]
    return float((order[:, 0] == y_idx).mean()), float((order[:, :3] == y_idx[:, None]).any(axis=1).mean())


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--esp-dir", type=Path, default=ESP_DIR)
    args = parser.parse_args()
    out_dir = args.esp_dir / "analysis"

    esp_files = sorted(glob.glob(str(args.esp_dir / "*.csv")))
    if not esp_files:
        raise SystemExit(f"No ESP32 CSVs in {args.esp_dir}. Download them from the collector first.")

    phone = load_scans(sorted(glob.glob(str(PHONE_DIR / "*.csv"))), ["Timestamp"])
    esp = load_scans(esp_files, ["Location Label", "Device", "Take", "Scan"])  # take numbers restart per spot
    esp = esp.drop_duplicates(["scan", "bssid"])  # the same take downloaded twice
    spots = sorted(set(esp["label"]) & set(phone["label"]))
    unknown = sorted(set(esp["label"]) - set(phone["label"]))
    if unknown:
        print(f"Ignoring ESP32 labels with no phone data: {unknown}")
    esp = esp[esp["label"].isin(spots)]
    print(f"Spots with both phone and ESP32 data: {len(spots)}")
    print(f"  ESP32 scans: {esp['scan'].nunique()}   phone scans at those spots: "
          f"{phone[phone['label'].isin(spots)]['scan'].nunique()}")
    if len(spots) < 3:
        print("  WARNING: under 3 spots. Calibration is fitted on the tested spot itself; treat numbers as rough.")

    pairs = per_ap(phone[phone["label"].isin(spots)]).merge(
        per_ap(esp), on=["label", "bssid"], how="outer", suffixes=("_phone", "_esp"))
    pairs[["heard_phone", "heard_esp"]] = pairs[["heard_phone", "heard_esp"]].fillna(0.0)

    # ---------------------------------------------------------------- 1. RSSI
    cal = calibrate(pairs, esp)
    both = pairs[(pairs["heard_phone"] >= MIN_HEARD) & (pairs["heard_esp"] >= MIN_HEARD)]
    diff = both["mean_esp"] - both["mean_phone"]
    resid_offset = diff - cal["offset_db"]
    resid_linear = both["mean_esp"] - (cal["slope"] * both["mean_phone"] + cal["intercept"])
    r = np.corrcoef(both["mean_phone"], both["mean_esp"])[0, 1]

    print("\n1. RSSI, APs heard reliably by both at the same spot")
    print(f"  pairs (spot, AP): {len(both)}   correlation r = {r:.3f}")
    print(f"  ESP32 = phone {cal['offset_db']:+.1f} dB (median)   spread after that offset: "
          f"+-{resid_offset.std():.1f} dB")
    print(f"  linear fit: ESP32 = {cal['slope']:.2f} * phone {cal['intercept']:+.1f}   spread: "
          f"+-{resid_linear.std():.1f} dB")
    per_spot = diff.groupby(both["label"]).median()
    print("  median offset per spot (a constant offset should be similar everywhere):")
    for label, off in per_spot.items():
        print(f"    {off:+6.1f} dB  {label}")

    # ---------------------------------------------------------------- 2. hearing
    phone_heard = pairs[pairs["heard_phone"] >= MIN_HEARD].copy()
    bins = np.arange(-100, -29, 5)
    phone_heard["bin"] = pd.cut(phone_heard["mean_phone"], bins)
    hearing = phone_heard.groupby("bin", observed=True)["heard_esp"].agg(lambda s: (s > 0).mean())
    esp_only = pairs[(pairs["heard_phone"] == 0) & (pairs["heard_esp"] >= MIN_HEARD)]
    print("\n2. Hearing: share of the phone's APs that the ESP32 also heard, by phone RSSI")
    for interval, share in hearing.items():
        print(f"    {interval.left:4.0f} to {interval.right:4.0f} dBm: {share:5.0%}")
    print(f"  ESP32 floor (0.5th percentile of its readings): {cal['esp_floor_dbm']:.0f} dBm")
    print(f"  APs the ESP32 heard but the phone never did: {len(esp_only)}")

    # ---------------------------------------------------------------- 3. localization
    feature_bssids = [b.lower() for b in json.loads((SPLITS / "feature_bssids.json").read_text())]
    label_map = {v.strip(): int(k) for k, v in json.loads((SPLITS / "label_map.json").read_text()).items()}
    X_phone = np.vstack([np.load(SPLITS / f"{s}_X.npy") for s in ("train", "val", "test")])
    y_phone = np.concatenate([np.load(SPLITS / f"{s}_y.npy") for s in ("train", "val", "test")])

    # Same 5 GHz masking as 06_Train_2_4GHz_Model.py: those columns are never heard.
    raw = pd.concat(pd.read_csv(f) for f in glob.glob(str(PHONE_DIR / "*.csv")))
    raw["BSSID"] = raw["BSSID"].str.lower()
    freq = raw.groupby("BSSID")["Frequency"].agg(lambda s: s.mode()[0]).reindex(feature_bssids)
    X_phone[:, (freq >= 3000).to_numpy()] = MISSING_RSSI

    X_esp, esp_labels = to_matrix(esp, feature_bssids)
    y_esp = np.array([label_map[l] for l in esp_labels])
    known = (X_esp > MISSING_RSSI).sum(axis=1)
    print(f"\n3. Localization on {len(y_esp)} ESP32 scans (52 possible spots); "
          f"model APs heard per scan: {np.median(known):.0f} median")

    phone_model = train_rf(X_phone, y_phone)
    results = {"a. phone model, raw ESP32 scans": topk(phone_model, X_esp, y_esp)}

    # b and c: calibrate without the spot under test
    shifted_hits, adapted_hits = [], []
    for spot in spots:
        rows = esp_labels == spot
        held_out = pairs[pairs["label"] != spot] if len(spots) >= 3 else pairs
        c = calibrate(held_out, esp[esp["label"] != spot] if len(spots) >= 3 else esp)
        X_shift = shift_heard(X_esp[rows], c["inv_slope"], c["inv_intercept"])
        shifted_hits.append(np.array(topk(phone_model, X_shift, y_esp[rows])) * rows.sum())
        adapted = train_rf(shift_heard(X_phone, c["slope"], c["intercept"], c["esp_floor_dbm"]), y_phone)
        adapted_hits.append(np.array(topk(adapted, X_esp[rows], y_esp[rows])) * rows.sum())
    results["b. phone model, ESP32 shifted to phone scale"] = tuple(np.sum(shifted_hits, axis=0) / len(y_esp))
    results["c. model trained on phone data shifted to ESP32"] = tuple(np.sum(adapted_hits, axis=0) / len(y_esp))

    for name, (top1, top3) in results.items():
        print(f"  {name:50s} top-1 {top1:6.1%}   top-3 {top3:6.1%}")

    per_spot_acc = pd.DataFrame({"label": esp_labels,
                                 "raw": phone_model.predict(X_esp) == y_esp}).groupby("label")["raw"].mean()
    print("  top-1 per spot, raw (a):")
    for label, acc in per_spot_acc.items():
        print(f"    {acc:5.0%}  {label}")

    # ---------------------------------------------------------------- outputs
    out_dir.mkdir(parents=True, exist_ok=True)
    cal.update(n_spots=len(spots), correlation=float(r),
               spread_after_offset_db=float(resid_offset.std()), spread_after_linear_db=float(resid_linear.std()),
               accuracy={k: {"top1": v[0], "top3": v[1]} for k, v in results.items()})
    (out_dir / "calibration.json").write_text(json.dumps(cal, indent=2))

    fig, axes = plt.subplots(1, 3, figsize=(16, 5))
    ax = axes[0]
    ax.scatter(both["mean_phone"], both["mean_esp"], s=14, alpha=0.6)
    lims = np.array([-100, -30])
    ax.plot(lims, lims, "k:", lw=1, label="same RSSI")
    ax.plot(lims, lims + cal["offset_db"], "--", label=f"offset {cal['offset_db']:+.1f} dB")
    ax.plot(lims, cal["slope"] * lims + cal["intercept"], label=f"{cal['slope']:.2f}x {cal['intercept']:+.1f}")
    ax.set(xlabel="phone mean RSSI (dBm)", ylabel="ESP32 mean RSSI (dBm)", title="Same spot, same AP")
    ax.legend()
    ax = axes[1]
    ax.bar([f"{i.left:.0f}" for i in hearing.index], hearing.to_numpy())
    ax.set(xlabel="phone RSSI bin (dBm)", ylabel="share also heard by ESP32", title="ESP32 hearing", ylim=(0, 1))
    ax = axes[2]
    ax.barh(range(len(per_spot)), per_spot.to_numpy())
    ax.set_yticks(range(len(per_spot)), per_spot.index, fontsize=8)
    ax.axvline(cal["offset_db"], color="k", ls="--", lw=1)
    ax.set(xlabel="median ESP32 - phone (dB)", title="Offset per spot")
    fig.tight_layout()
    fig.savefig(out_dir / "esp32_vs_phone.png", dpi=120)
    print(f"\nSaved {out_dir / 'calibration.json'} and {out_dir / 'esp32_vs_phone.png'}")


if __name__ == "__main__":
    main()
