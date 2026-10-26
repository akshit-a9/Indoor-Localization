import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

class LocationCoord {
  final int id;
  final String label;
  final double x;
  final double y;
  final int map;
  const LocationCoord({
    required this.id,
    required this.label,
    required this.x,
    required this.y,
    required this.map,
  });
}

class CoordinatesService {
  final Map<String, LocationCoord> _byLabel = {};
  bool _loaded = false;

  bool get isLoaded => _loaded;

  Future<void> load() async {
    if (_loaded) return;
    try {
      final raw = await rootBundle.loadString('assets/Maps/Coordinates.json');
      final list = jsonDecode(raw) as List<dynamic>;
      for (final e in list) {
        final m = e as Map<String, dynamic>;
        final c = LocationCoord(
          id: m['id'] as int,
          label: (m['label'] as String).trim(),
          x: (m['x'] as num).toDouble(),
          y: (m['y'] as num).toDouble(),
          map: m['map'] as int,
        );
        _byLabel[c.label] = c;
      }
      _loaded = true;
    } catch (e) {
      debugPrint('Error loading coordinates: $e');
    }
  }

  LocationCoord? lookup(String label) {
    final key = label.trim();
    // Try exact match first
    if (_byLabel.containsKey(key)) return _byLabel[key];
    
    // Fallback: Case-insensitive match
    final lowerKey = key.toLowerCase();
    for (final entry in _byLabel.entries) {
      if (entry.key.toLowerCase() == lowerKey) return entry.value;
    }
    return null;
  }
}
