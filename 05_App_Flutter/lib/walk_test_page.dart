import 'dart:async';
import 'dart:io';
import 'package:flutter/material.dart';
import 'package:path_provider/path_provider.dart';
import 'package:share_plus/share_plus.dart';
import 'models/localization_result.dart';
import 'services/inference_engine.dart';
import 'services/wifi_scanner.dart';

class WalkTestSample {
  final DateTime timestamp;
  final String actual;
  final String predicted;
  final double? confidence;

  const WalkTestSample({
    required this.timestamp,
    required this.actual,
    required this.predicted,
    required this.confidence,
  });
}

class WalkTestPage extends StatefulWidget {
  final InferenceEngine engine;
  final RealTimeScanner? scanner;

  const WalkTestPage({super.key, required this.engine, this.scanner});

  @override
  State<WalkTestPage> createState() => _WalkTestPageState();
}

class _WalkTestPageState extends State<WalkTestPage> {
  StreamSubscription? _sub;
  LocalizationResult? _latestResult;
  String? _selectedActual;
  final List<WalkTestSample> _samples = [];
  bool _exported = false;

  @override
  void initState() {
    super.initState();
    _sub = widget.scanner?.updates.listen((update) {
      if (!mounted) return;
      setState(() => _latestResult = update.result as LocalizationResult?);
    });
  }

  @override
  void dispose() {
    _sub?.cancel();
    super.dispose();
  }

  void _logSample() {
    final actual = _selectedActual;
    final result = _latestResult;
    if (actual == null || result == null) return;

    final sample = WalkTestSample(
      timestamp: DateTime.now(),
      actual: actual,
      predicted: result.locationLabel,
      confidence: result.confidence,
    );
    setState(() {
      _samples.add(sample);
      _exported = false;
    });
  }

  Future<void> _exportCsv() async {
    if (_samples.isEmpty) return;

    final buf = StringBuffer()..writeln('timestamp,actual,predicted,confidence');
    for (final s in _samples) {
      final conf = s.confidence?.toStringAsFixed(6) ?? '';
      buf.writeln('${s.timestamp.toIso8601String()},'
          '${_csvEscape(s.actual)},'
          '${_csvEscape(s.predicted)},'
          '$conf');
    }

    Directory? dir;
    try {
      dir = Platform.isAndroid
          ? await getExternalStorageDirectory()
          : await getDownloadsDirectory();
    } catch (_) {}
    dir ??= await getApplicationDocumentsDirectory();

    final ts = DateTime.now()
        .toIso8601String()
        .replaceAll(':', '-')
        .replaceAll('.', '-');
    final fileName = 'walk_test_$ts.csv';
    final file = File('${dir.path}/$fileName');
    await file.writeAsString(buf.toString());

    // Share the file
    await Share.shareXFiles([XFile(file.path)],
        text: 'Indoor Localization Walk Test Export');

    if (!mounted) return;
    setState(() => _exported = true);
    ScaffoldMessenger.of(context)
      ..hideCurrentSnackBar()
      ..showSnackBar(SnackBar(content: Text('Exported and shared: $fileName')));
  }

  String _csvEscape(String v) {
    if (v.contains(',') || v.contains('"') || v.contains('\n')) {
      return '"${v.replaceAll('"', '""')}"';
    }
    return v;
  }

  Future<bool> _confirmDiscard() async {
    if (_samples.isEmpty || _exported) return true;
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Discard samples?'),
        content: Text(
            '${_samples.length} sample${_samples.length == 1 ? '' : 's'} not exported. Leaving will discard them.'),
        actions: [
          TextButton(
              onPressed: () => Navigator.of(ctx).pop(false),
              child: const Text('Cancel')),
          TextButton(
              onPressed: () => Navigator.of(ctx).pop(true),
              child: const Text('Discard')),
        ],
      ),
    );
    return ok ?? false;
  }

  @override
  Widget build(BuildContext context) {
    final labels = widget.engine.labelMap.values.toList()..sort();
    final scanning = widget.scanner?.state == ScanState.scanning;
    final result = _latestResult;
    final canLog = _selectedActual != null && result != null;
    final tt = Theme.of(context).textTheme;

    return PopScope(
      canPop: false,
      onPopInvokedWithResult: (didPop, _) async {
        if (didPop) return;
        final navigator = Navigator.of(context);
        final ok = await _confirmDiscard();
        if (ok && mounted) navigator.pop();
      },
      child: Scaffold(
        appBar: AppBar(title: const Text('Walk Test'), centerTitle: true),
        body: SafeArea(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                _PredictionCard(result: result, isScanning: scanning),
                const SizedBox(height: 16),
                DropdownButtonFormField<String>(
                  decoration: const InputDecoration(
                    labelText: 'Actual location',
                    border: OutlineInputBorder(),
                  ),
                  value: _selectedActual,
                  isExpanded: true,
                  items: labels
                      .map((l) => DropdownMenuItem(
                            value: l,
                            child: Text(l, overflow: TextOverflow.ellipsis),
                          ))
                      .toList(),
                  onChanged: (v) => setState(() => _selectedActual = v),
                ),
                const SizedBox(height: 12),
                FilledButton.icon(
                  onPressed: canLog ? _logSample : null,
                  icon: const Icon(Icons.add_task),
                  label: const Text('Log Sample'),
                  style: FilledButton.styleFrom(
                      minimumSize: const Size.fromHeight(48)),
                ),
                if (_samples.isNotEmpty) ...[
                  const SizedBox(height: 24),
                  Text('Recent Logs (Latest 5)', style: tt.titleSmall),
                  const SizedBox(height: 8),
                  ..._samples.reversed
                      .take(5)
                      .map((s) => _RecentSampleTile(sample: s)),
                ],
                const Spacer(),
                _SampleCounterBar(
                  count: _samples.length,
                  exported: _exported,
                  onExport: _samples.isNotEmpty ? _exportCsv : null,
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _PredictionCard extends StatelessWidget {
  final LocalizationResult? result;
  final bool isScanning;
  const _PredictionCard({required this.result, required this.isScanning});

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    final tt = Theme.of(context).textTheme;

    return Card(
      color: isScanning ? cs.primaryContainer : cs.surfaceContainerLow,
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          children: [
            Icon(
              Icons.my_location,
              size: 36,
              color: isScanning ? cs.onPrimaryContainer : cs.onSurfaceVariant,
            ),
            const SizedBox(height: 10),
            Text(
              result?.locationLabel ??
                  (isScanning
                      ? 'Waiting for scan...'
                      : 'Start scanning from home'),
              style: tt.titleMedium?.copyWith(
                color: isScanning ? cs.onPrimaryContainer : cs.onSurface,
                fontWeight: FontWeight.bold,
              ),
              textAlign: TextAlign.center,
            ),
            if (result?.confidence != null) ...[
              const SizedBox(height: 4),
              Text(
                'Confidence: ${(result!.confidence! * 100).toStringAsFixed(1)}%',
                style: tt.bodySmall?.copyWith(
                  color:
                      isScanning ? cs.onPrimaryContainer : cs.onSurfaceVariant,
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }
}

class _RecentSampleTile extends StatelessWidget {
  final WalkTestSample sample;
  const _RecentSampleTile({required this.sample});

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    final tt = Theme.of(context).textTheme;
    final isCorrect = sample.actual == sample.predicted;

    return Container(
      margin: const EdgeInsets.only(bottom: 8),
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      decoration: BoxDecoration(
        color: cs.surfaceContainerLow,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: cs.outlineVariant.withOpacity(0.5)),
      ),
      child: Row(
        children: [
          Icon(
            isCorrect ? Icons.check_circle_outline : Icons.error_outline,
            color: isCorrect ? Colors.green : cs.error,
            size: 18,
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                RichText(
                  text: TextSpan(
                    style: tt.bodySmall?.copyWith(color: cs.onSurface),
                    children: [
                      const TextSpan(
                          text: 'Actual: ',
                          style: TextStyle(fontWeight: FontWeight.bold)),
                      TextSpan(text: sample.actual),
                    ],
                  ),
                ),
                RichText(
                  text: TextSpan(
                    style: tt.bodySmall?.copyWith(color: cs.onSurface),
                    children: [
                      const TextSpan(
                          text: 'Predicted: ',
                          style: TextStyle(fontWeight: FontWeight.bold)),
                      TextSpan(
                        text: sample.predicted,
                        style: TextStyle(
                          color: isCorrect ? null : cs.error,
                        ),
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ),
          if (sample.confidence != null)
            Text(
              '${(sample.confidence! * 100).toStringAsFixed(0)}%',
              style: tt.labelSmall?.copyWith(color: cs.onSurfaceVariant),
            ),
        ],
      ),
    );
  }
}

class _SampleCounterBar extends StatelessWidget {
  final int count;
  final bool exported;
  final VoidCallback? onExport;
  const _SampleCounterBar({
    required this.count,
    required this.exported,
    required this.onExport,
  });

  @override
  Widget build(BuildContext context) {
    final tt = Theme.of(context).textTheme;
    final cs = Theme.of(context).colorScheme;
    return Row(
      children: [
        Expanded(
          child: Text(
            '$count sample${count == 1 ? '' : 's'}'
            '${exported ? ' · exported' : ''}',
            style: tt.bodyMedium?.copyWith(color: cs.onSurfaceVariant),
          ),
        ),
        OutlinedButton.icon(
          onPressed: onExport,
          icon: const Icon(Icons.share),
          label: const Text('Export & Share'),
        ),
      ],
    );
  }
}
