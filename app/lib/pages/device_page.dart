import 'dart:async';

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:sensor_puck/pages/connect_device_page.dart';
import 'package:sensor_puck/sensor_puck.dart';

const dateTimePickerDateRange = Duration(days: 365);

class TimeOfDayWithSeconds {
  final int _hour;
  final int _minute;
  final int _second;

  int get hour => _hour;
  int get minute => _minute;
  int get second => _second;

  TimeOfDayWithSeconds(this._hour, this._minute, this._second);

  TimeOfDayWithSeconds.fromDateTime(DateTime dt)
      : _hour = dt.hour,
        _minute = dt.minute,
        _second = dt.second;

  static TimeOfDayWithSeconds now() {
    var dt = DateTime.now();
    return TimeOfDayWithSeconds(dt.hour, dt.minute, dt.second);
  }
}

class DevicePage extends StatefulWidget {
  const DevicePage({super.key, required this.device});

  final SensorPuck device;

  @override
  State<DevicePage> createState() => _DevicePageState();
}

class _DevicePageState extends State<DevicePage> {
  final dateTimeController = TextEditingController();
  late Timer dateTimeControllerUpdater;

  /// Offset, that when subtracted from the current DateTime, will result in the system time of the puck.
  Duration deviceDateTimeOffset = Duration.zero;
  String? deviceCurrentSsid;

  bool loading = true;

  String formatDateTime(DateTime dt) {
    String el(int v, [int width = 2]) => v.toString().padLeft(width, '0');

    return "${el(dt.hour)}:${el(dt.minute)}:${el(dt.second)}, ${el(dt.day)}.${el(dt.month)}.${el(dt.year, 4)}";
  }

  DateTime deviceDateTime() => DateTime.now().subtract(deviceDateTimeOffset);

  @override
  void initState() {
    super.initState();
    dateTimeControllerUpdater = Timer.periodic(const Duration(seconds: 1), (_) {
      dateTimeController.text = formatDateTime(deviceDateTime());
      setState(() {});
    });

    setup();
  }

  @override
  void dispose() {
    dateTimeControllerUpdater.cancel();
    widget.device.disconnect();
    super.dispose();
  }

  Future<void> updateDeviceDateTimeOffset() async {
    deviceDateTimeOffset =
        DateTime.now().difference(await widget.device.getSystemTime());
  }

  Future<void> setup() async {
    await updateDeviceDateTimeOffset();
    deviceCurrentSsid = await widget.device.getWifiSsid();

    setState(() => loading = false);
  }

  Future<void> pickDeviceDateTime() async {
    var deviceDt = deviceDateTime();
    var firstDate = DateTime.now().subtract(dateTimePickerDateRange);
    if (deviceDt.isBefore(firstDate)) firstDate = deviceDt;
    var lastDate = DateTime.now().add(dateTimePickerDateRange);
    if (deviceDt.isAfter(lastDate)) lastDate = deviceDt;

    var dt = await showDatePicker(
      context: context,
      firstDate: firstDate,
      lastDate: lastDate,
      initialDate: deviceDt,
    );
    if (dt == null) return;
    if (!mounted) return;

    TimeOfDayWithSeconds? time = await showDialog(
      context: context,
      builder: (context) => _TimePickerWithSecondsDialog(
        initialTime: TimeOfDayWithSeconds.fromDateTime(deviceDt),
      ),
    );
    if (time == null) return;
    if (!mounted) return;

    await setDeviceDateTime(DateTime(
        dt.year, dt.month, dt.day, time.hour, time.minute, time.second));
  }

  Future<void> setDeviceDateTime(DateTime newDateTime) async {
    setState(() => loading = true);
    deviceDateTimeOffset = DateTime.now().difference(newDateTime);
    await widget.device.setSystemTime(newDateTime);
    setState(() => loading = false);
  }

  Future<void> editWifiSettings() async {
    var res = await showDialog<(String, String)>(
      context: context,
      builder: (_) => _WifiConfigurationDialog(ssid: deviceCurrentSsid),
    );
    if (res == null) return;
    if (!mounted) return;

    setState(() => loading = true);
    deviceCurrentSsid = res.$1;
    await widget.device.setWifiSsid(res.$1);
    await widget.device.setWifiPassword(res.$2);
    setState(() => loading = false);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text("Gerät"),
        actions: [
          IconButton(
            onPressed: () => Navigator.pushReplacement(context,
                MaterialPageRoute(builder: (_) => const ConnectDevicePage())),
            icon: const Icon(Icons.sensors_off_outlined),
          ),
        ],
      ),
      body: !loading
          ? Padding(
              padding: const EdgeInsets.all(10),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      Expanded(
                        child: TextField(
                          controller: dateTimeController,
                          decoration: const InputDecoration(
                            border: OutlineInputBorder(),
                            alignLabelWithHint: true,
                            labelText: "Uhrzeit",
                          ),
                          readOnly: true,
                          onTap: pickDeviceDateTime,
                        ),
                      ),
                      const SizedBox(width: 10),
                      FilledButton(
                        onPressed: () => setDeviceDateTime(DateTime.now()),
                        child: const Text("Jetzt"),
                      ),
                    ],
                  ),
                  const SizedBox(height: 10),
                  Card.outlined(
                    margin: EdgeInsets.zero,
                    child: Padding(
                      padding: const EdgeInsets.all(15),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const SizedBox(width: double.infinity),
                          Text(
                            "WiFi",
                            style: Theme.of(context).textTheme.headlineMedium,
                          ),
                          const SizedBox(height: 10),
                          Text("SSID: ${deviceCurrentSsid ?? "---"}"),
                          const SizedBox(height: 10),
                          FilledButton.icon(
                            onPressed: editWifiSettings,
                            icon: const Icon(Icons.edit),
                            label: const Text("Bearbeiten"),
                          ),
                        ],
                      ),
                    ),
                  ),
                ],
              ),
            )
          : const Center(child: CircularProgressIndicator()),
    );
  }
}

/// Timer picker dialog that uses a CupertinoTimerPicker to pick a time with seconds
class _TimePickerWithSecondsDialog extends StatefulWidget {
  const _TimePickerWithSecondsDialog({this.initialTime});

  final TimeOfDayWithSeconds? initialTime;

  @override
  State<_TimePickerWithSecondsDialog> createState() =>
      __TimePickerWithSecondsDialogState();
}

class __TimePickerWithSecondsDialogState
    extends State<_TimePickerWithSecondsDialog> {
  late TimeOfDayWithSeconds time;

  @override
  void initState() {
    super.initState();
    time = widget.initialTime ?? TimeOfDayWithSeconds.now();
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      content: SizedBox(
        width: 330,
        height: 216,
        child: CupertinoTimerPicker(
          initialTimerDuration: Duration(
              seconds: time.second, minutes: time.minute, hours: time.hour),
          onTimerDurationChanged: (d) {
            var hour = d.inHours;
            var minute = d.inMinutes - hour * 60;
            var second = d.inSeconds - minute * 60 - hour * 3600;
            time = TimeOfDayWithSeconds(hour, minute, second);
          },
        ),
      ),
      actions: [
        TextButton(
          child: const Text("Abbrechen"),
          onPressed: () => Navigator.pop(context, null),
        ),
        TextButton(
          child: const Text("Ok"),
          onPressed: () => Navigator.pop(context, time),
        ),
      ],
    );
  }
}

class _WifiConfigurationDialog extends StatefulWidget {
  const _WifiConfigurationDialog({this.ssid});

  final String? ssid;

  @override
  State<_WifiConfigurationDialog> createState() =>
      _WifiConfigurationDialogState();
}

class _WifiConfigurationDialogState extends State<_WifiConfigurationDialog> {
  final ssidController = TextEditingController();
  final formKey = GlobalKey<FormState>();

  String ssid = "";
  String password = "";
  bool passwordObscured = true;

  @override
  void initState() {
    super.initState();
    if (widget.ssid != null) {
      ssid = widget.ssid!;
      ssidController.text = widget.ssid!;
    }
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text("WiFi"),
      content: Form(
        key: formKey,
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextFormField(
              controller: ssidController,
              decoration: const InputDecoration(
                border: OutlineInputBorder(),
                alignLabelWithHint: true,
                labelText: "SSID",
              ),
              onChanged: (s) => ssid = s.trim(),
              validator: (s) {
                if (s == null || s.trim().isEmpty) return "SSID erforderlich";
                return null;
              },
            ),
            const SizedBox(height: 20),
            TextFormField(
              decoration: InputDecoration(
                border: const OutlineInputBorder(),
                alignLabelWithHint: true,
                labelText: "Passwort",
                suffixIcon: IconButton(
                  onPressed: () =>
                      setState(() => passwordObscured = !passwordObscured),
                  icon: Icon(
                    !passwordObscured
                        ? Icons.visibility_outlined
                        : Icons.visibility_off_outlined,
                  ),
                ),
              ),
              keyboardType: TextInputType.visiblePassword,
              obscureText: passwordObscured,
              onChanged: (s) => password = s.trim(),
              validator: (s) {
                if (s == null || s.trim().isEmpty) {
                  return "Passwort erforderlich";
                }
                return null;
              },
            ),
          ],
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context, null),
          child: const Text("Abbrechen"),
        ),
        TextButton(
          onPressed: () {
            if (!(formKey.currentState?.validate() ?? false)) return;

            Navigator.pop(context, (ssid, password));
          },
          child: const Text("Speichern"),
        ),
      ],
    );
  }
}
