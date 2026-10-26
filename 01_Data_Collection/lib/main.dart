import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'home_page.dart';

void main() {
  runApp(MyApp());
}

class MyApp extends StatelessWidget {
  // static var platform = MethodChannel('com.example.biometrics/background');
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
        debugShowCheckedModeBanner: false,
      home: MyHomePage(),
    );
  }
}
