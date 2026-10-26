import 'dart:convert';
import 'dart:typed_data';
import 'package:flutter/services.dart';
import 'package:onnxruntime/onnxruntime.dart';
import '../models/localization_result.dart';

class ModelInfo {
  final String name;
  final String onnxFile;
  final double testAccuracy;
  const ModelInfo({required this.name, required this.onnxFile, required this.testAccuracy});
}

class InferenceEngine {
  OrtSession? _session;
  List<String> _featureBssids = [];
  Map<int, String> _labelMap = {};
  List<ModelInfo> _models = [];
  ModelInfo? _activeModel;

  bool get isReady => _session != null && _featureBssids.isNotEmpty;
  int get featureCount => _featureBssids.length;
  List<String> get featureBssids => List.unmodifiable(_featureBssids);
  List<ModelInfo> get availableModels => List.unmodifiable(_models);
  ModelInfo? get activeModel => _activeModel;
  Map<int, String> get labelMap => Map.unmodifiable(_labelMap);

  Future<void> init() async {
    OrtEnv.instance.init();

    final registryStr = await rootBundle.loadString('assets/models/models_registry.json');
    final registry = jsonDecode(registryStr) as Map<String, dynamic>;
    _models = registry.entries
        .map((e) => ModelInfo(
              name: e.key,
              onnxFile: e.value['onnx_file'] as String,
              testAccuracy: (e.value['test_accuracy'] as num).toDouble(),
            ))
        .toList()
      ..sort((a, b) => b.testAccuracy.compareTo(a.testAccuracy));

    final bssidsStr = await rootBundle.loadString('assets/models/feature_bssids.json');
    _featureBssids = (jsonDecode(bssidsStr) as List<dynamic>)
        .map((e) => (e as String).toLowerCase())
        .toList();

    final labelsStr = await rootBundle.loadString('assets/models/label_map.json');
    final labelsRaw = jsonDecode(labelsStr) as Map<String, dynamic>;
    _labelMap = labelsRaw.map((k, v) => MapEntry(int.parse(k), v as String));

    await loadModel(_models.first);
  }

  Future<void> loadModel(ModelInfo model) async {
    _session?.release();
    _session = null;

    final raw = await rootBundle.load('assets/models/${model.onnxFile}');
    final bytes = raw.buffer.asUint8List();
    final opts = OrtSessionOptions();
    _session = OrtSession.fromBuffer(bytes, opts);
    _activeModel = model;
  }

  /// Build a feature vector from a map of {bssid: rssi}.
  /// Missing BSSIDs are filled with -100 (sentinel from training).
  Float32List buildFeatureVector(Map<String, double> bssidToRssi) {
    return Float32List.fromList(
      _featureBssids.map((bssid) => bssidToRssi[bssid] ?? -100.0).toList(),
    );
  }

  /// Run inference on a pre-built feature vector.
  Future<LocalizationResult> runInference(
    Float32List featureVector, {
    required int apDetectedCount,
    required int scanCount,
  }) async {
    final session = _session;
    if (session == null) throw StateError('Model not loaded');

    final inputTensor = OrtValueTensor.createTensorWithDataList(
      featureVector,
      [1, _featureBssids.length],
    );
    final runOpts = OrtRunOptions();

    try {
      final outputs = await session.runAsync(runOpts, {'rssi_input': inputTensor});

      // Output 0: predicted class (int64), shape [1]
      final rawLabel = outputs?[0]?.value;
      final classIndex = _extractInt(rawLabel);

      // Output 1: class probabilities (float32), shape [1, n_classes] — present in most sklearn ONNX exports
      List<double>? probabilities;
      if (outputs != null && outputs.length > 1) {
        final rawProbs = outputs[1]?.value;
        probabilities = _extractProbabilities(rawProbs);
      }

      outputs?.forEach((e) => e?.release());

      final label = _labelMap[classIndex] ?? 'Unknown ($classIndex)';
      return LocalizationResult(
        classIndex: classIndex,
        locationLabel: label,
        apDetectedCount: apDetectedCount,
        apTotalCount: _featureBssids.length,
        scanCount: scanCount,
        probabilities: probabilities,
      );
    } finally {
      inputTensor.release();
      runOpts.release();
    }
  }

  int _extractInt(dynamic value) {
    if (value is int) return value;
    if (value is List && value.isNotEmpty) return _extractInt(value[0]);
    throw StateError('Unexpected ONNX output type for class label: ${value.runtimeType}');
  }

  List<double>? _extractProbabilities(dynamic value) {
    try {
      if (value is List && value.isNotEmpty) {
        final inner = value[0];
        // Tensor output: [[p0, p1, ..., pN]]
        if (inner is List) {
          return inner.map((e) => (e as num).toDouble()).toList();
        }
        // sklearn ONNX probability output: sequence(map(int64, float))
        // OrtValueSequence.value returns List<OrtValueMap>
        if (inner is OrtValueMap) {
          final map = inner.value as Map;
          final sorted = map.entries.toList()
            ..sort((a, b) => (a.key as num).compareTo(b.key as num));
          return sorted.map((e) => (e.value as num).toDouble()).toList();
        }
        // Plain map fallback
        if (inner is Map) {
          final sorted = inner.entries.toList()
            ..sort((a, b) => (a.key as num).compareTo(b.key as num));
          return sorted.map((e) => (e.value as num).toDouble()).toList();
        }
      }
    } catch (_) {}
    return null;
  }

  void dispose() {
    _session?.release();
    OrtEnv.instance.release();
  }
}
