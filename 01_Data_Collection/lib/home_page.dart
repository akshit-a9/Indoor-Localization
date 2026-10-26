import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'sensor_manager.dart';
import 'permissions.dart';

class CsvDataScreen extends StatefulWidget {
  @override
  _CsvDataScreenState createState() => _CsvDataScreenState();
}

class _CsvDataScreenState extends State<CsvDataScreen> {
  final SensorManager sensorManager = SensorManager();

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('CSV Data Display'),
      ),
      body: FutureBuilder<List<List<dynamic>>>(
        future: sensorManager.readCsvData(),
        builder: (context, snapshot) {
          if (snapshot.connectionState == ConnectionState.waiting) {
            return const Center(child: CircularProgressIndicator());
          } else if (snapshot.hasError) {
            return Center(child: Text("Error: ${snapshot.error}"));
          } else if (snapshot.hasData) {
            return ListView.builder(
              itemCount: snapshot.data!.length,
              itemBuilder: (context, index) {
                if (index == 0) return Container();
                List<dynamic> row = snapshot.data![index];
                if (row.length < 27) {
                   return const ListTile(title: Text("Old data format or invalid row"));
                }
                return ListTile(
                  title: Text('TS: ${row[0]} | Loc: ${row[26]}'),
                  subtitle: Text('Act: ${row[25]} | Accel: (${row[1].toStringAsFixed(1)}, ${row[2].toStringAsFixed(1)}, ${row[3].toStringAsFixed(1)})'),
                );
              },
            );
          } else {
            return const Center(child: Text("No data available"));
          }
        },
      ),
    );
  }
}

class MyHomePage extends StatefulWidget {
  @override
  _MyHomePageState createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> {
  bool _isCollectingData = false;
  final SensorManager sensorManager = SensorManager();
  String _selectedActivity = 'STATIONARY';
  final TextEditingController _locationController = TextEditingController(text: 'Room_1');
  
  bool _collectSensors = true;
  bool _collectWifi = true;

  final List<String> _activityLabels = [
    'STATIONARY',
    'WALKING',
    'STAIRS UP',
    'STAIRS DOWN',
    'ELEVATOR UP',
    'ELEVATOR DOWN'
  ];

  static const MethodChannel platform = MethodChannel('com.example.biometrics/background');

  @override
  void dispose() {
    sensorManager.dispose();
    _locationController.dispose();
    super.dispose();
  }

  Future<void> _startForegroundService() async {
    try {
      await platform.invokeMethod('startService');
    } on PlatformException catch (e) {
      debugPrint("Failed to start service: '${e.message}'.");
    }
  }

  void _toggleDataCollection() async {
    if (!_isCollectingData) {
      bool permissionsGranted = await requestLocalizationPermissions();
      if (!permissionsGranted) {
        _showErrorDialog("Location permissions are required for WiFi scanning.");
        return;
      }
      
      if (!_collectSensors && !_collectWifi) {
        _showErrorDialog("Please select at least one data source to collect.");
        return;
      }
    }

    setState(() {
      _isCollectingData = !_isCollectingData;
    });

    if (_isCollectingData) {
      try {
        sensorManager.updateLabels(
          activity: _selectedActivity,
          location: _locationController.text.isEmpty ? "UNKNOWN" : _locationController.text,
        );
        await sensorManager.startCollection(
          collectSensors: _collectSensors,
          collectWifi: _collectWifi,
        );
        await _startForegroundService();
        ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text("Data collection started."))
        );
      } catch (e) {
        setState(() {
          _isCollectingData = false;
        });
        _showErrorDialog(e.toString());
      }
    } else {
      sensorManager.stopCollection();
      ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text("Data collection stopped."))
      );
    }
  }

  void _showErrorDialog(String message) {
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text("Error"),
        content: Text(message),
        actions: <Widget>[
          TextButton(
            child: const Text('Ok'),
            onPressed: () => Navigator.of(ctx).pop(),
          )
        ],
      ),
    );
  }

  Future<void> _shareCsvFile() async {
    try {
      await sensorManager.shareDataFiles(context);
    } catch (e) {
      _showErrorDialog("Failed to share the CSV file: ${e.toString()}");
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Localization Logger'),
      ),
      body: GestureDetector(
        onPanUpdate: (details) => sensorManager.updateTouchData(details.localPosition),
        behavior: HitTestBehavior.opaque,
        child: SingleChildScrollView(
          padding: const EdgeInsets.all(20.0),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: <Widget>[
              const Text(
                "1. Data Sources",
                style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
              ),
              CheckboxListTile(
                title: const Text("Collect Sensors (IMU, Baro)"),
                value: _collectSensors,
                onChanged: _isCollectingData ? null : (val) => setState(() => _collectSensors = val!),
              ),
              CheckboxListTile(
                title: const Text("Collect WiFi Fingerprints"),
                value: _collectWifi,
                onChanged: _isCollectingData ? null : (val) => setState(() => _collectWifi = val!),
              ),
              const Divider(),
              const SizedBox(height: 10),
              const Text(
                "2. Activity Label",
                style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
              ),
              DropdownButton<String>(
                value: _selectedActivity,
                isExpanded: true,
                items: _activityLabels.map((String value) {
                  return DropdownMenuItem<String>(
                    value: value,
                    child: Text(value),
                  );
                }).toList(),
                onChanged: _isCollectingData ? null : (newValue) {
                  setState(() {
                    _selectedActivity = newValue!;
                  });
                },
              ),
              const SizedBox(height: 20),
              const Text(
                "3. Location Label (e.g. Room_101)",
                style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
              ),
              TextField(
                controller: _locationController,
                enabled: !_isCollectingData,
                decoration: const InputDecoration(
                  hintText: "Enter location name",
                  border: OutlineInputBorder(),
                ),
              ),
              const SizedBox(height: 30),
              ElevatedButton(
                onPressed: _toggleDataCollection,
                style: ElevatedButton.styleFrom(
                  backgroundColor: _isCollectingData ? Colors.red : Colors.green,
                  foregroundColor: Colors.white,
                  padding: const EdgeInsets.all(15),
                ),
                child: Text(
                  _isCollectingData ? 'STOP LOGGING' : 'START LOGGING',
                  style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                ),
              ),
              const SizedBox(height: 40),
              const Divider(),
              const SizedBox(height: 10),
              OutlinedButton.icon(
                onPressed: () {
                  Navigator.of(context).push(
                    MaterialPageRoute(builder: (context) => CsvDataScreen()),
                  );
                },
                icon: const Icon(Icons.list_alt),
                label: const Text('View Sensor CSV'),
              ),
              const SizedBox(height: 10),
              OutlinedButton.icon(
                onPressed: _shareCsvFile,
                icon: const Icon(Icons.share),
                label: const Text('Export CSVs'),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
