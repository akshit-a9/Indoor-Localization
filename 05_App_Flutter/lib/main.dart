import 'package:flutter/material.dart';
import 'home_page.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const IndoorLocalizationApp());
}

class IndoorLocalizationApp extends StatelessWidget {
  const IndoorLocalizationApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Indoor Localization',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorSchemeSeed: const Color(0xFF1565C0),
        useMaterial3: true,
        brightness: Brightness.light,
      ),
      darkTheme: ThemeData(
        colorSchemeSeed: const Color(0xFF1565C0),
        useMaterial3: true,
        brightness: Brightness.dark,
      ),
      home: const HomePage(),
    );
  }
}
