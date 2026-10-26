import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:wifi_scan/wifi_scan.dart';
import 'inference_engine.dart';

/// One completed WiFi scan: map of lowercase BSSID -> RSSI (dBm).
typedef ScanSnapshot = Map<String, double>;

enum ScanState { idle, scanning }

class ScanUpdate {
  final dynamic result; // LocalizationResult?
  final int apDetectedCount;
  final DateTime timestamp;
  const ScanUpdate({
    required this.result,
    required this.apDetectedCount,
    required this.timestamp,
  });
}

class RealTimeScanner {
  final InferenceEngine engine;

  ScanState _state = ScanState.idle;
  StreamSubscription<List<WiFiAccessPoint>>? _wifiSub;
  Timer? _scanTriggerTimer;
  bool _isRequesting = false;

  ScanState get state => _state;

  final _resultController = StreamController<ScanUpdate>.broadcast();
  Stream<ScanUpdate> get updates => _resultController.stream;

  RealTimeScanner({required this.engine});

  Future<bool> start() async {
    if (_state != ScanState.idle) return false;

    // Check if we can scan
    final canScan = await WiFiScan.instance.canStartScan();
    if (canScan != CanStartScan.yes) return false;

    _state = ScanState.scanning;

    // Listen for results whenever they are available (system or our trigger)
    _wifiSub = WiFiScan.instance.onScannedResultsAvailable.listen(_onResults);

    // Reduced interval to 500ms for faster updates.
    // Note: Effectiveness depends on "Wi-Fi scan throttling" being disabled in Developer Options.
    _scanTriggerTimer = Timer.periodic(const Duration(milliseconds: 500), (_) {
      _requestScan();
    });
    
    _requestScan();
    return true;
  }

  Future<void> _requestScan() async {
    if (_isRequesting || _state != ScanState.scanning) return;
    _isRequesting = true;
    try {
      await WiFiScan.instance.startScan();
    } finally {
      _isRequesting = false;
    }
  }

  void stop() {
    _state = ScanState.idle;
    _scanTriggerTimer?.cancel();
    _wifiSub?.cancel();
  }

  void _onResults(List<WiFiAccessPoint> aps) async {
    if (_state != ScanState.scanning || !engine.isReady) return;

    final snapshot = <String, double>{};
    for (final ap in aps) {
      final bssid = ap.bssid?.toLowerCase();
      if (bssid != null) snapshot[bssid] = ap.level.toDouble();
    }

    try {
      final vector = engine.buildFeatureVector(snapshot);
      int detected = 0;
      for (var val in vector) {
        if (val > -100) detected++;
      }

      final result = await engine.runInference(
        vector,
        apDetectedCount: detected,
        scanCount: 1,
      );

      _resultController.add(ScanUpdate(
        result: result,
        apDetectedCount: detected,
        timestamp: DateTime.now(),
      ));
    } catch (e) {
      debugPrint('Inference error: $e');
    }
  }

  void dispose() {
    stop();
    _resultController.close();
  }
}
