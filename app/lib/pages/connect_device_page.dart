import 'package:flutter/material.dart';
import 'package:sensor_puck/pages/device_page.dart';
import 'package:sensor_puck/sensor_puck.dart';
import 'package:universal_ble/universal_ble.dart';

class ConnectDevicePage extends StatefulWidget {
  const ConnectDevicePage({super.key});

  @override
  State<ConnectDevicePage> createState() => _ConnectDevicePageState();
}

class _ConnectDevicePageState extends State<ConnectDevicePage> {
  Map<String, BleDevice> devices = {};
  bool loading = false;

  @override
  void initState() {
    super.initState();

    UniversalBle.onScanResult = (BleDevice device) {
      if (device.name == null) return;
      if (!devices.containsKey(device.name)) {
        devices[device.name!] = device;
        print(
            "Found ${device.name}! Services: ${device.services}, Mfg Data: ${device.manufacturerDataList}");
      }
      setState(() {});
    };
    UniversalBle.startScan();
  }

  Future<void> connectToDevice(BleDevice device) async {
    setState(() => loading = true);
    try {
      await UniversalBle.stopScan();
      UniversalBle.onScanResult = null;
      var puck = await SensorPuck.connect(device);
      if (!mounted) return;
      Navigator.pushReplacement(
        context,
        MaterialPageRoute(builder: (_) => DevicePage(device: puck)),
      );
    } catch (e) {
      debugPrint("Failed connecting to device ${device.name}: $e");
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text("Verbindung fehlgeschlagen: $e")));
    }
  }

  @override
  void dispose() {
    UniversalBle.stopScan();
    devices = {};
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Gerät verbinden")),
      body: devices.isNotEmpty && !loading
          ? ListView.builder(
              itemCount: devices.length,
              itemBuilder: (context, i) {
                var device = devices.values.elementAt(i);
                return ListTile(
                  title: Text(device.name!),
                  subtitle: Text(device.deviceId),
                  onTap: () => connectToDevice(device),
                );
              },
            )
          : const Center(child: CircularProgressIndicator()),
    );
  }
}
