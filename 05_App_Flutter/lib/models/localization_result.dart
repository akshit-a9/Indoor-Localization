class LocalizationResult {
  final int classIndex;
  final String locationLabel;
  final List<double>? probabilities; // null if model doesn't output probs
  final int apDetectedCount;
  final int apTotalCount;
  final int scanCount;

  const LocalizationResult({
    required this.classIndex,
    required this.locationLabel,
    required this.apDetectedCount,
    required this.apTotalCount,
    required this.scanCount,
    this.probabilities,
  });

  double? get confidence {
    if (probabilities == null || classIndex >= probabilities!.length) return null;
    return probabilities![classIndex];
  }

  // Stub for future floor map integration.
  // When a coordinate map JSON is loaded, this will return (x, y, floor).
  MapCoordinates? get coordinates => null;
}

/// Placeholder for future floor map coordinate resolution.
class MapCoordinates {
  final double x;
  final double y;
  final int floor;
  const MapCoordinates(this.x, this.y, this.floor);
}
