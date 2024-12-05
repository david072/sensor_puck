// Services and characteristic UUIDs used by the Sensor Puck.
// NOTE: UUIDs are defined using BLE_UUID128_INIT(). Their byte-order has to be flipped
//  here compared to the way they are defined in the code.
// TODO: Make this flipping nicer? (build_runner?)

import 'dart:typed_data';

import 'package:universal_ble/universal_ble.dart';

class SensorPuck {
  static const systemServiceUuid = "da0fbd11-9835-2fb7-5347-786d15494171";
  static const dateTimeCharacteristicUuid =
      "11a1bd33-b92d-37ae-8f40-206194592f9e";

  final String _bleDeviceId;

  SensorPuck(this._bleDeviceId);

  static Future<SensorPuck> connect(BleDevice device) async {
    await UniversalBle.connect(device.deviceId);
    await UniversalBle.discoverServices(device.deviceId);
    return SensorPuck(device.deviceId);
  }

  Future<void> disconnect() async {
    await UniversalBle.disconnect(_bleDeviceId);
  }

  int _littleEndianUint8ListToInt(Uint8List list) {
    int result = 0;
    for (int i = 0; i < list.length; i++) {
      result += list[i] << (i * 8);
    }
    return result;
  }

  Uint8List _intToLittleEndianUint8List(int i) {
    List<int> result = [];
    while (i > 0) {
      result.add(i & 0xFF);
      i >>= 8;
    }

    return Uint8List.fromList(result);
  }

  Future<DateTime> getSystemTime() async {
    var dateTimeCharacteristic = await UniversalBle.readValue(
        _bleDeviceId, systemServiceUuid, dateTimeCharacteristicUuid);

    var unixTimestamp = _littleEndianUint8ListToInt(dateTimeCharacteristic);
    return DateTime.fromMillisecondsSinceEpoch(unixTimestamp * 1000,
            isUtc: true)
        .toLocal();
  }

  Future<void> setSystemTime(DateTime dt) async {
    var unixTimestamp = dt.toUtc().millisecondsSinceEpoch ~/ 1000;
    var value = _intToLittleEndianUint8List(unixTimestamp);
    print(value);
    UniversalBle.writeValue(
      _bleDeviceId,
      systemServiceUuid,
      dateTimeCharacteristicUuid,
      value,
      BleOutputProperty.withoutResponse,
    );
  }
}
