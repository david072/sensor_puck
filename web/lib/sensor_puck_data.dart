import 'dart:math';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:fixnum/fixnum.dart';

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

int decodeInt16(List<int> bytes) {
  var value = Int32.ZERO | (bytes.removeAt(0) << 8) | bytes.removeAt(0);
  // if the top bit is set (i.e. the number is negative), extend the sign bit all the way to the left
  if (value >> 15 > 0) value |= 0xFFFF << 16;
  return value.toInt();
}

enum SensorPuckValueType {
  co2(0x0),
  temperature(0x1),
  humidity(0x2);

  const SensorPuckValueType(this.value);

  final int value;
}

sealed class SensorPuckValue {
  final String unit;
  final SensorPuckValueType type;

  const SensorPuckValue(this.unit, this.type);

  static SensorPuckValue? decode(List<int> bytes) {
    var type = bytes.removeAt(0);
    if (type == SensorPuckValueType.co2.value) return Co2PpmValue.decode(bytes);
    if (type == SensorPuckValueType.temperature.value) {
      return TemperatureValue.decode(bytes);
    }
    if (type == SensorPuckValueType.humidity.value) {
      return HumidityValue.decode(bytes);
    }

    return null;
  }

  int get encodedLength => 4;

  Iaq? iaq() => null;

  void encodeInto(List<int> bytes) {
    bytes.add(type.value);
    _encodeContentInto(bytes);
  }

  void _encodeContentInto(List<int> bytes);
}

class Co2PpmValue extends SensorPuckValue {
  final int ppm;

  const Co2PpmValue(this.ppm) : super("ppm", SensorPuckValueType.co2);

  static Co2PpmValue decode(List<int> bytes) => Co2PpmValue(decodeInt16(bytes));

  @override
  Iaq? iaq() => switch (ppm) {
        _ when ppm <= 400 => Iaq.excellent,
        _ when ppm <= 1000 => Iaq.fine,
        _ when ppm <= 1500 => Iaq.moderate,
        _ when ppm <= 2000 => Iaq.poor,
        _ when ppm <= 5000 => Iaq.veryPoor,
        _ => Iaq.severe,
      };

  @override
  void _encodeContentInto(List<int> bytes) {
    encodeInt16Into(bytes, ppm);
  }
}

class TemperatureValue extends SensorPuckValue {
  final double temperature;

  const TemperatureValue(this.temperature)
      : super("°C", SensorPuckValueType.temperature);

  static TemperatureValue decode(List<int> bytes) =>
      TemperatureValue(decodeInt16(bytes) / 100);

  @override
  void _encodeContentInto(List<int> bytes) {
    encodeInt16Into(bytes, (temperature * 100).round());
  }
}

class HumidityValue extends SensorPuckValue {
  final double humidity;

  const HumidityValue(this.humidity) : super("%", SensorPuckValueType.humidity);

  static HumidityValue decode(List<int> bytes) =>
      HumidityValue(decodeInt16(bytes) / 100);

  @override
  void _encodeContentInto(List<int> bytes) {
    encodeInt16Into(bytes, (humidity * 100).round());
  }
}

class SensorPuck {
  final Co2PpmValue? co2;
  final TemperatureValue? temp;
  final HumidityValue? hum;
  final Iaq iaq;

  SensorPuck({this.co2, this.temp, this.hum})
      : iaq = Iaq.worst([co2?.iaq(), temp?.iaq(), hum?.iaq()]);

  static SensorPuck decode(String b64) {
    var bytes = base64.decode(b64).toList();
    Map<SensorPuckValueType, SensorPuckValue> values = {};
    while (bytes.isNotEmpty) {
      var v = SensorPuckValue.decode(bytes);
      if (v == null) continue;
      values[v.type] = v;
    }

    return SensorPuck(
      co2: values[SensorPuckValueType.co2] as Co2PpmValue?,
      temp: values[SensorPuckValueType.temperature] as TemperatureValue?,
      hum: values[SensorPuckValueType.humidity] as HumidityValue?,
    );
  }

  String encode() {
    List<int> bytes = [];
    co2?.encodeInto(bytes);
    temp?.encodeInto(bytes);
    hum?.encodeInto(bytes);
    return base64.encode(bytes);
  }
}
