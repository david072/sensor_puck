import 'package:flutter/material.dart';
import 'package:sensor_puck/pages/connect_device_page.dart';

void main() {
  runApp(MaterialApp(
    debugShowCheckedModeBanner: false,
    theme: ThemeData.dark(useMaterial3: true),
    themeMode: ThemeMode.dark,
    home: const ConnectDevicePage(),
  ));
}
