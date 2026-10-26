# 04_Testing

Live walk-test recordings produced by the `05_App_Flutter` app, plus the
evaluation figures derived from them.

## Files

- `walk_test_<YYYY-MM-DD>.csv` — one row per inference made on-device during a
  walk test. Typical columns: timestamp, predicted label, ground-truth label,
  predicted class index, top-1 confidence, AP-detection counts and the model
  used.
- `figures/` — output of
  [../../03_Training/05_Testing_Statistics.ipynb](../../03_Training/05_Testing_Statistics.ipynb).
  - `confusion_matrix.png` — full confusion matrix on the walk-test set.
  - `confidence_distribution.png` — histogram of top-1 confidence.
  - `confidence_correct_vs_wrong.png` — top-1 confidence split by correctness.
  - `map_correct_floor0.png`, `map_correct_floor1.png` — correct predictions
    plotted on the floor maps.
  - `map_incorrect_floor0.png`, `map_incorrect_floor1.png` — incorrect
    predictions, with arrows from prediction to ground truth.

## Adding a new test run

1. Record a walk test in `05_App_Flutter` (see
   [../../05_App_Flutter/README.md](../../05_App_Flutter/README.md)).
2. Drop the exported CSV into this folder as
   `walk_test_<YYYY-MM-DD>.csv`.
3. Rerun `05_Testing_Statistics.ipynb` to refresh the figures.
