import 'dart:async';
import 'dart:io';
import 'package:flutter/material.dart';
import 'package:sensors_plus/sensors_plus.dart';
import 'package:path_provider/path_provider.dart';
import 'package:csv/csv.dart';
import 'package:share_plus/share_plus.dart';
import 'package:wifi_scan/wifi_scan.dart';

class SensorManager {
  final List<StreamSubscription<dynamic>> _streamSubscriptions = [];
  final List<List<dynamic>> _sensorData = [];
  final List<List<dynamic>> _wifiData = [];
  final List<Offset> _touchStrokeData = [];
  final StreamController<List<dynamic>> _dataController = StreamController.broadcast();

  double? _accelerometerX, _accelerometerY, _accelerometerZ;
  double? _gyroscopeX, _gyroscopeY, _gyroscopeZ;
  double? _magnetometerX, _magnetometerY, _magnetometerZ;
  double? _rotationVectorX, _rotationVectorY, _rotationVectorZ;
  double? _tiltX, _tiltY, _tiltZ;
  double? _autoRotationX, _autoRotationY, _autoRotationZ;
  double? _motionX, _motionY, _motionZ;
  double? _barometer;
  double? _lastTouchX, _lastTouchY;
  String _currentActivity = 'STATIONARY';
  String _currentLocation = 'UNKNOWN';

  Timer? _wifiScanTimer;

  Stream<List<dynamic>> get dataStream => _dataController.stream;

  void updateLabels({required String activity, required String location}) {
    _currentActivity = activity;
    _currentLocation = location;
  }

  Future<void> startCollection({bool collectSensors = true, bool collectWifi = true}) async {
    // Clear lists and existing files to ensure we only share fresh data
    _sensorData.clear();
    _wifiData.clear();
    await _deleteExistingFiles();
    
    if (collectSensors) _collectSensorData();
    if (collectWifi) _startWifiScanning();
  }

  Future<void> _deleteExistingFiles() async {
    final directory = await getExternalStorageDirectory();
    if (directory != null) {
      final sensorFile = File('${directory.path}/sensor_data.csv');
      final wifiFile = File('${directory.path}/wifi_data.csv');
      if (await sensorFile.exists()) await sensorFile.delete();
      if (await wifiFile.exists()) await wifiFile.delete();
    }
  }

  void dispose() {
    _stopAll();
    _dataController.close();
  }

  void _stopAll() {
    for (var subscription in _streamSubscriptions) {
      subscription.cancel();
    }
    _streamSubscriptions.clear();
    _wifiScanTimer?.cancel();
    _wifiScanTimer = null;
  }

  void updateTouchData(Offset touchPosition) {
    _touchStrokeData.add(touchPosition);
    _lastTouchX = touchPosition.dx;
    _lastTouchY = touchPosition.dy;
    _dataController.add([
      DateTime.now().toIso8601String(),
      'Touch',
      touchPosition.dx,
      touchPosition.dy,
      _currentActivity,
      _currentLocation
    ]);
  }

  void _collectSensorData() {
    _streamSubscriptions.add(accelerometerEventStream().listen((event) {
      _accelerometerX = event.x;
      _accelerometerY = event.y;
      _accelerometerZ = event.z;
      _addSensorData();
    }));

    _streamSubscriptions.add(gyroscopeEventStream().listen((event) {
      _gyroscopeX = event.x;
      _gyroscopeY = event.y;
      _gyroscopeZ = event.z;
      _addSensorData();
    }));

    _streamSubscriptions.add(magnetometerEventStream().listen((event) {
      _magnetometerX = event.x;
      _magnetometerY = event.y;
      _magnetometerZ = event.z;
      _addSensorData();
    }));

    _streamSubscriptions.add(userAccelerometerEventStream().listen((event) {
      _rotationVectorX = event.x;
      _rotationVectorY = event.y;
      _rotationVectorZ = event.z;
      _addSensorData();
    }));

    _streamSubscriptions.add(gyroscopeEventStream().listen((event) {
      _tiltX = event.x;
      _tiltY = event.y;
      _tiltZ = event.z;
      _addSensorData();
    }));

    _streamSubscriptions.add(userAccelerometerEventStream().listen((event) {
      _autoRotationX = event.x;
      _autoRotationY = event.y;
      _autoRotationZ = event.z;
      _addSensorData();
    }));

    _streamSubscriptions.add(userAccelerometerEventStream().listen((event) {
      _motionX = event.x;
      _motionY = event.y;
      _motionZ = event.z;
      _addSensorData();
    }));

    _streamSubscriptions.add(barometerEventStream().listen((event) {
      _barometer = event.pressure;
      _addSensorData();
    }));
  }

  void _startWifiScanning() async {
    final canScan = await WiFiScan.instance.canStartScan();
    if (canScan != CanStartScan.yes) {
      debugPrint("Cannot start WiFi scan: $canScan");
      return;
    }

    _streamSubscriptions.add(WiFiScan.instance.onScannedResultsAvailable.listen((results) {
      final timestamp = DateTime.now().toIso8601String();
      for (var network in results) {
        _wifiData.add([
          timestamp,
          network.ssid,
          network.bssid,
          network.level,
          network.frequency,
          _currentActivity,
          _currentLocation
        ]);
      }
      debugPrint("WiFi Scan results recorded: ${results.length} APs found.");
    }));

    _wifiScanTimer = Timer.periodic(const Duration(seconds: 2), (timer) async {
      await WiFiScan.instance.startScan();
    });
    
    await WiFiScan.instance.startScan();
  }

  void _addSensorData() {
    List<dynamic> sensorEvent = [
      DateTime.now().toIso8601String(),
      _accelerometerX ?? 0, _accelerometerY ?? 0, _accelerometerZ ?? 0,
      _gyroscopeX ?? 0, _gyroscopeY ?? 0, _gyroscopeZ ?? 0,
      _magnetometerX ?? 0, _magnetometerY ?? 0, _magnetometerZ ?? 0,
      _rotationVectorX ?? 0, _rotationVectorY ?? 0, _rotationVectorZ ?? 0,
      _tiltX ?? 0, _tiltY ?? 0, _tiltZ ?? 0,
      _autoRotationX ?? 0, _autoRotationY ?? 0, _autoRotationZ ?? 0,
      _motionX ?? 0, _motionY ?? 0, _motionZ ?? 0,
      _barometer ?? 0,
      _lastTouchX ?? 0, _lastTouchY ?? 0,
      _currentActivity,
      _currentLocation
    ];

    _sensorData.add(sensorEvent);
    _dataController.add(sensorEvent);
  }

  Future<void> saveDataToFile() async {
    final directory = await getExternalStorageDirectory();
    if (directory == null) return;

    if (_sensorData.isNotEmpty) {
      final sensorFile = File('${directory.path}/sensor_data.csv');
      final List<List<dynamic>> sensorRows = List<List<dynamic>>.from(_sensorData);
      sensorRows.insert(0, [
        "Timestamp",
        "Accelerometer X", "Accelerometer Y", "Accelerometer Z",
        "Gyroscope X", "Gyroscope Y", "Gyroscope Z",
        "Magnetometer X", "Magnetometer Y", "Magnetometer Z",
        "Rotation Vector X", "Rotation Vector Y", "Rotation Vector Z",
        "Tilt Detector X", "Tilt Detector Y", "Tilt Detector Z",
        "Auto-rotation X", "Auto-rotation Y", "Auto-rotation Z",
        "Motion X", "Motion Y", "Motion Z",
        "Barometer",
        "Last Touch X", "Last Touch Y",
        "Activity Label",
        "Location Label"
      ]);
      await sensorFile.writeAsString(CsvCodec().encode(sensorRows));
    }

    if (_wifiData.isNotEmpty) {
      final wifiFile = File('${directory.path}/wifi_data.csv');
      final List<List<dynamic>> wifiRows = List<List<dynamic>>.from(_wifiData);
      wifiRows.insert(0, ["Timestamp", "SSID", "BSSID", "RSSI", "Frequency", "Activity Label", "Location Label"]);
      await wifiFile.writeAsString(CsvCodec().encode(wifiRows));
    }
    
    debugPrint("Files saved to ${directory.path}");
  }

  Future<void> stopCollection() async {
    _stopAll();
    await saveDataToFile(); // Await the save process before clearing memory
    _sensorData.clear();
    _wifiData.clear();
  }

  Future<void> shareDataFiles(BuildContext context) async {
    final directory = await getExternalStorageDirectory();
    if (directory != null) {
      final sensorPath = '${directory.path}/sensor_data.csv';
      final wifiPath = '${directory.path}/wifi_data.csv';
      
      List<XFile> filesToShare = [];
      if (await File(sensorPath).exists()) filesToShare.add(XFile(sensorPath));
      if (await File(wifiPath).exists()) filesToShare.add(XFile(wifiPath));

      if (filesToShare.isNotEmpty) {
        await Share.shareXFiles(filesToShare, text: 'Localization data.');
      } else {
        if (context.mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
              const SnackBar(content: Text("No data files found to share."))
          );
        }
      }
    }
  }

  Future<List<List<dynamic>>> readCsvData() async {
    try {
      final directory = await getExternalStorageDirectory();
      final filePath = '${directory?.path}/sensor_data.csv';
      final File file = File(filePath);

      if (await file.exists()) {
        final csvString = await file.readAsString();
        return CsvCodec().decode(csvString);
      } else {
        throw Exception("CSV file does not exist.");
      }
    } catch (e) {
      throw Exception("Failed to read CSV file: $e");
    }
  }
}
