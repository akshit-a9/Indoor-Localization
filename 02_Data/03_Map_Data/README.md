# 03_Map_Data

Floor plans and pixel coordinates of every labeled location, used to render
predictions on a map in `05_App_Flutter/` and in the evaluation notebooks.

## Files

- `map_0.png` — ground floor.
- `map_1.png` — first floor.
- `Coordinates.csv` — pixel coordinates of every labeled location.

## `Coordinates.csv` schema

| Column  | Description                                                |
|---------|------------------------------------------------------------|
| `S.No`  | Sequential index, not a class index.                       |
| `Label` | Location label, matching `label_map.json`.                 |
| `x`     | Pixel x-coordinate on the corresponding `map_<MAP>.png`.   |
| `y`     | Pixel y-coordinate on the corresponding `map_<MAP>.png`.   |
| `MAP`   | Floor: `0` = ground floor, `1` = first floor.              |

## Consumers

- [05_App_Flutter](../../05_App_Flutter/) ships these files under
  `assets/Maps/` and uses them to plot the predicted location.
- [03_Training/05_Testing_Statistics.ipynb](../../03_Training/05_Testing_Statistics.ipynb)
  uses them to render the per-floor correct/incorrect overlays in
  `../04_Testing/figures/`.
