import 'dart:math';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:fixnum/fixnum.dart';

const int maxHistoryEntries = 48;
const int timeBetweenHistoryEntriesSec = 10;

enum Iaq {
  excellent("Excellent", Colors.green),
  fine("Fine", Colors.lightGreen),
  moderate("Moderate", Colors.amber),
  poor("Poor", Colors.orange),
  veryPoor("Very Poor", Colors.deepOrange),
  severe("Severe", Colors.red);

  const Iaq(this.name, this.color);

  final String name;
  final Color color;

  static Iaq worst(Iterable<Iaq?> iaqs) => iaqs
      .where((el) => el != null)
      .fold(Iaq.excellent, (v, el) => Iaq.values[max(v.index, el!.index)]);
}

void encodeInt16Into(List<int> bytes, int i) {
  var value = i.toSigned(16);
  bytes.add((value >> 8) & 0xFF);
  bytes.add((value >> 0) & 0xFF);
}

void encodeInt64Into(List<int> bytes, int i) {
  var value = i.toSigned(64);
  for (int i = 56; i >= 0; i -= 8) {
    bytes.add((value >> i) & 0xFF);
  }
}

int decodeInt16(List<int> bytes) {
  var value = Int32.ZERO | (bytes.removeAt(0) << 8) | bytes.removeAt(0);
  // if the top bit is set (i.e. the number is negative), extend the sign bit all the way to the left
  if (value >> 15 > 0) value |= 0xFFFF << 16;
  return value.toInt();
}

int decodeInt64(List<int> bytes) {
  var value = Int64.ZERO;
  for (int i = 56; i >= 0; i -= 8) {
    value |= bytes.removeAt(0) << i;
  }
  return value.toInt();
}

enum SensorPuckValueType {
  co2(0x0),
  temperature(0x1),
  humidity(0x2);

  const SensorPuckValueType(this.value);

  final int value;
}

sealed class SensorPuckValue<T> {
  final T value;
  final List<T> history = [];

  final String unit;
  final SensorPuckValueType type;

  SensorPuckValue(this.value, this.unit, this.type);

  static SensorPuckValue? decode(List<int> bytes, int historyLength) {
    SensorPuckValue result;
    var type = bytes.removeAt(0);
    if (type == SensorPuckValueType.co2.value) {
      result = Co2PpmValue.decode(bytes);
    } else if (type == SensorPuckValueType.temperature.value) {
      result = TemperatureValue.decode(bytes);
    } else if (type == SensorPuckValueType.humidity.value) {
      result = HumidityValue.decode(bytes);
    } else {
      return null;
    }

    result._decodeHistory(bytes, historyLength);
    return result;
  }

  int get encodedLength => 4;

  Iaq? iaq() => null;

  void encodeInto(List<int> bytes) {
    bytes.add(type.value);
    _encodeContentInto(bytes);
    bytes.add(history.length & 0xFF);
    _encodeHistoryInto(bytes);
  }

  void _encodeContentInto(List<int> bytes);
  void _encodeHistoryInto(List<int> bytes);

  void _decodeHistory(List<int> bytes, int length);
}

class Co2PpmValue extends SensorPuckValue<int> {
  Co2PpmValue(int ppm) : super(ppm, "ppm", SensorPuckValueType.co2);

  static Co2PpmValue decode(List<int> bytes) => Co2PpmValue(decodeInt16(bytes));

  @override
  Iaq? iaq() => switch (value) {
        _ when value <= 400 => Iaq.excellent,
        _ when value <= 1000 => Iaq.fine,
        _ when value <= 1500 => Iaq.moderate,
        _ when value <= 2000 => Iaq.poor,
        _ when value <= 5000 => Iaq.veryPoor,
        _ => Iaq.severe,
      };

  @override
  void _encodeContentInto(List<int> bytes) {
    encodeInt16Into(bytes, value);
  }

  @override
  void _encodeHistoryInto(List<int> bytes) {
    for (int ppm in history) {
      encodeInt16Into(bytes, ppm);
    }
  }

  @override
  void _decodeHistory(List<int> bytes, int length) {
    for (int i = 0; i < length; ++i) {
      history.add(decodeInt16(bytes));
    }
  }
}

class TemperatureValue extends SensorPuckValue<double> {
  TemperatureValue(double temp)
      : super(temp, "°C", SensorPuckValueType.temperature);

  static void _encodeValueInto(List<int> bytes, double temp) =>
      encodeInt16Into(bytes, (temp * 100).round());
  static double _decodeValue(List<int> bytes) => decodeInt16(bytes) / 100;

  static TemperatureValue decode(List<int> bytes) =>
      TemperatureValue(_decodeValue(bytes));

  @override
  void _encodeContentInto(List<int> bytes) {
    _encodeValueInto(bytes, value);
  }

  @override
  void _encodeHistoryInto(List<int> bytes) {
    for (double temp in history) {
      _encodeValueInto(bytes, temp);
    }
  }

  @override
  void _decodeHistory(List<int> bytes, int length) {
    for (int i = 0; i < length; ++i) {
      history.add(_decodeValue(bytes));
    }
  }
}

class HumidityValue extends SensorPuckValue<double> {
  HumidityValue(double hum) : super(hum, "%", SensorPuckValueType.humidity);

  static void _encodeValueInto(List<int> bytes, double temp) =>
      encodeInt16Into(bytes, (temp * 100).round());
  static double _decodeValue(List<int> bytes) => decodeInt16(bytes) / 100;

  static HumidityValue decode(List<int> bytes) =>
      HumidityValue(_decodeValue(bytes));

  @override
  void _encodeContentInto(List<int> bytes) {
    encodeInt16Into(bytes, (value * 100).round());
  }

  @override
  void _encodeHistoryInto(List<int> bytes) {
    for (double hum in history) {
      _encodeValueInto(bytes, hum);
    }
  }

  @override
  void _decodeHistory(List<int> bytes, int length) {
    for (int i = 0; i < length; ++i) {
      history.add(_decodeValue(bytes));
    }
  }
}

class SensorPuck {
  final Co2PpmValue? co2;
  final TemperatureValue? temp;
  final HumidityValue? hum;
  final Iaq iaq;

  final DateTime lastUpdate;

  SensorPuck({this.co2, this.temp, this.hum, required this.lastUpdate})
      : iaq = Iaq.worst([co2?.iaq(), temp?.iaq(), hum?.iaq()]);

  static SensorPuck decode(String b64) {
    var bytes = base64.decode(b64).toList();

    var lastUpdate = DateTime.fromMillisecondsSinceEpoch(
            decodeInt64(bytes) * 1000,
            isUtc: true)
        .toLocal();

    var historyLength = bytes.removeAt(0);
    if (historyLength > 0) {
      var historyOffset = bytes.removeAt(0);
    }

    Map<SensorPuckValueType, SensorPuckValue> values = {};
    while (bytes.isNotEmpty) {
      var v = SensorPuckValue.decode(bytes, historyLength);
      if (v == null) continue;
      values[v.type] = v;
    }

    return SensorPuck(
      co2: values[SensorPuckValueType.co2] as Co2PpmValue?,
      temp: values[SensorPuckValueType.temperature] as TemperatureValue?,
      hum: values[SensorPuckValueType.humidity] as HumidityValue?,
      lastUpdate: lastUpdate,
    );
  }

  String encode() {
    List<int> bytes = [];
    encodeInt64Into(bytes, lastUpdate.millisecondsSinceEpoch ~/ 1000);
    co2?.encodeInto(bytes);
    temp?.encodeInto(bytes);
    hum?.encodeInto(bytes);
    return base64.encode(bytes);
  }
}
