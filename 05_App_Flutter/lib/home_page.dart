import 'dart:async';
import 'dart:ui';
import 'package:flutter/material.dart';
import 'package:permission_handler/permission_handler.dart';
import 'models/localization_result.dart';
import 'services/coordinates_service.dart';
import 'services/inference_engine.dart';
import 'services/wifi_scanner.dart';
import 'walk_test_page.dart';
import 'widgets/location_map.dart';

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  final _engine = InferenceEngine();
  final _coords = CoordinatesService();
  RealTimeScanner? _scanner;
  StreamSubscription? _updateSub;

  bool _engineReady = false;
  String _statusMessage = 'Initializing...';
  LocalizationResult? _latestResult;
  DateTime? _lastScanTime;
  int _scanCount = 0;

  @override
  void initState() {
    super.initState();
    _initEngine();
  }

  Future<void> _initEngine() async {
    try {
      await Future.wait([_engine.init(), _coords.load()]);
      setState(() {
        _engineReady = true;
        _statusMessage = 'Ready';
      });
    } catch (e) {
      setState(() => _statusMessage = 'Engine error: $e');
    }
  }

  Future<void> _startScan() async {
    final granted = await _requestPermissions();
    if (!granted) {
      setState(() => _statusMessage = 'Location permission required');
      return;
    }

    _scanner?.stop();
    _updateSub?.cancel();

    _scanner = RealTimeScanner(engine: _engine);
    _scanCount = 0;
    _lastScanTime = null;

    _updateSub = _scanner!.updates.listen((update) {
      setState(() {
        _latestResult = update.result as LocalizationResult?;
        _lastScanTime = update.timestamp;
        _scanCount++;
        _statusMessage = 'Scanning...';
      });
    });

    setState(() {
      _latestResult = null;
      _statusMessage = 'Starting scan...';
    });

    final started = await _scanner!.start();
    if (!started) {
      setState(() => _statusMessage = 'Could not start scan. Enable Location & WiFi.');
    }
  }

  void _stopScan() {
    _scanner?.stop();
    setState(() {
      _statusMessage = 'Stopped';
    });
  }

  Future<bool> _requestPermissions() async {
    Map<Permission, PermissionStatus> statuses = await [
      Permission.location,
      Permission.nearbyWifiDevices,
    ].request();
    return statuses[Permission.location]?.isGranted ?? false;
  }

  bool get _isScanning => _scanner != null && _scanner!.state == ScanState.scanning;

  LocationCoord? get _currentCoord {
    final label = _latestResult?.locationLabel;
    if (label == null) return null;
    return _coords.lookup(label);
  }

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;

    return Scaffold(
      appBar: AppBar(
        title: const Text('Indoor Localization'),
        centerTitle: true,
        backgroundColor: cs.surface,
        bottom: PreferredSize(
          preferredSize: const Size.fromHeight(1),
          child: Divider(height: 1, color: cs.outlineVariant),
        ),
      ),
      body: SafeArea(
        child: SingleChildScrollView(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              _ModelCard(engine: _engine, onModelChanged: () => setState(() {})),
              const SizedBox(height: 12),
              _LocationCard(result: _latestResult, isScanning: _isScanning),
              const SizedBox(height: 12),
              _QuickView(
                status: _statusMessage,
                scanCount: _scanCount,
                lastScanTime: _lastScanTime,
                isScanning: _isScanning,
                onWalkTest: _engineReady
                    ? () => Navigator.of(context).push(
                          MaterialPageRoute(
                            builder: (_) => WalkTestPage(
                              engine: _engine,
                              scanner: _scanner,
                            ),
                          ),
                        )
                    : null,
              ),
              const SizedBox(height: 12),
              LocationMap(coord: _currentCoord),
              const SizedBox(height: 20),
              _ScanButton(
                engineReady: _engineReady,
                isScanning: _isScanning,
                onStart: _startScan,
                onStop: _stopScan,
              ),
            ],
          ),
        ),
      ),
    );
  }

  @override
  void dispose() {
    _updateSub?.cancel();
    _scanner?.dispose();
    _engine.dispose();
    super.dispose();
  }
}

class _QuickView extends StatelessWidget {
  final String status;
  final int scanCount;
  final DateTime? lastScanTime;
  final bool isScanning;
  final VoidCallback? onWalkTest;

  const _QuickView({
    required this.status,
    required this.scanCount,
    required this.lastScanTime,
    required this.isScanning,
    this.onWalkTest,
  });

  String _formatTime(DateTime t) =>
      '${t.hour.toString().padLeft(2, '0')}:${t.minute.toString().padLeft(2, '0')}:${t.second.toString().padLeft(2, '0')}';

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    final tt = Theme.of(context).textTheme;

    return Card.outlined(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 8),
        child: Row(
          children: [
            Icon(
              isScanning ? Icons.sync : Icons.pause_circle_outline,
              size: 16,
              color: isScanning ? Colors.blue : cs.onSurfaceVariant,
            ),
            const SizedBox(width: 8),
            Expanded(
              child: Text(
                status,
                style: tt.bodySmall?.copyWith(fontWeight: FontWeight.w600),
                overflow: TextOverflow.ellipsis,
              ),
            ),
            Text(
              '$scanCount scan${scanCount == 1 ? '' : 's'}',
              style: tt.bodySmall?.copyWith(color: cs.onSurfaceVariant, fontSize: 11),
            ),
            if (lastScanTime != null) ...[
              const SizedBox(width: 6),
              Text('·', style: tt.bodySmall?.copyWith(color: cs.onSurfaceVariant)),
              const SizedBox(width: 6),
              Text(
                _formatTime(lastScanTime!),
                style: tt.bodySmall?.copyWith(
                  color: cs.onSurfaceVariant,
                  fontSize: 11,
                  fontFeatures: const [FontFeature.tabularFigures()],
                ),
              ),
            ],
            if (onWalkTest != null) ...[
              const SizedBox(width: 12),
              SizedBox(
                width: 24,
                height: 24,
                child: IconButton(
                  onPressed: onWalkTest,
                  icon: const Icon(Icons.directions_walk, size: 16),
                  padding: EdgeInsets.zero,
                  color: Colors.blue,
                  tooltip: 'Walk Test',
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }
}

class _ModelCard extends StatefulWidget {
  final InferenceEngine engine;
  final VoidCallback onModelChanged;
  const _ModelCard({required this.engine, required this.onModelChanged});

  @override
  State<_ModelCard> createState() => _ModelCardState();
}

class _ModelCardState extends State<_ModelCard> {
  @override
  Widget build(BuildContext context) {
    final models = widget.engine.availableModels;
    final active = widget.engine.activeModel;
    if (models.isEmpty) return const SizedBox.shrink();

    return Card.outlined(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
        child: DropdownButtonHideUnderline(
          child: DropdownButton<ModelInfo>(
            isExpanded: true,
            value: active,
            items: models
                .map((m) => DropdownMenuItem(
                      value: m,
                      child: Text('${m.name} (${(m.testAccuracy * 100).toStringAsFixed(1)}%)'),
                    ))
                .toList(),
            onChanged: (m) async {
              if (m == null) return;
              await widget.engine.loadModel(m);
              widget.onModelChanged();
              setState(() {});
            },
          ),
        ),
      ),
    );
  }
}

class _LocationCard extends StatelessWidget {
  final LocalizationResult? result;
  final bool isScanning;
  const _LocationCard({this.result, required this.isScanning});

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    final tt = Theme.of(context).textTheme;

    return Card(
      color: isScanning ? cs.primaryContainer : cs.surfaceContainerLow,
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          children: [
            Icon(
              Icons.location_pin,
              size: 40,
              color: isScanning ? cs.onPrimaryContainer : cs.onSurfaceVariant,
            ),
            const SizedBox(height: 12),
            Text(
              result?.locationLabel ?? 'Waiting for scan...',
              style: tt.headlineSmall?.copyWith(
                color: isScanning ? cs.onPrimaryContainer : cs.onSurface,
                fontWeight: FontWeight.bold,
              ),
              textAlign: TextAlign.center,
            ),
            if (result?.confidence != null) ...[
              const SizedBox(height: 6),
              Text(
                'Confidence: ${(result!.confidence! * 100).toStringAsFixed(1)}%',
                style: tt.bodySmall?.copyWith(
                  color: isScanning ? cs.onPrimaryContainer : cs.onSurfaceVariant,
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }
}

class _ScanButton extends StatelessWidget {
  final bool engineReady;
  final bool isScanning;
  final VoidCallback onStart;
  final VoidCallback onStop;
  const _ScanButton({
    required this.engineReady,
    required this.isScanning,
    required this.onStart,
    required this.onStop,
  });

  @override
  Widget build(BuildContext context) {
    if (isScanning) {
      return OutlinedButton.icon(
        onPressed: onStop,
        icon: const Icon(Icons.stop),
        label: const Text('Stop Real-Time Scan'),
        style: OutlinedButton.styleFrom(minimumSize: const Size.fromHeight(52)),
      );
    }
    return FilledButton.icon(
      onPressed: engineReady ? onStart : null,
      icon: const Icon(Icons.wifi_find),
      label: const Text('Start Real-Time Scan'),
      style: FilledButton.styleFrom(minimumSize: const Size.fromHeight(52)),
    );
  }
}
