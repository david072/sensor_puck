import 'dart:math';

import 'package:flutter/material.dart';
import 'package:intl/intl.dart' hide TextDirection;
import 'package:sensor_puck_web/sensor_puck_data.dart';

String toStringAsFixedWithoutTrailingZeroes(double d, int fractionDigits) => d
    .toStringAsFixed(fractionDigits)
    .replaceAll(RegExp(r"([.]*0)(?!.*\d)"), "");

class HomePage extends StatefulWidget {
  const HomePage({super.key, required this.b64Data});

  final String b64Data;

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  late final SensorPuck sp;

  @override
  void initState() {
    super.initState();
    sp = SensorPuck.decode(widget.b64Data);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Sensor Puck")),
      body: SingleChildScrollView(
        child: Column(
          children: [
            Text(
                "Updated: ${DateFormat("HH:mm, dd.MM.yyyy").format(sp.lastUpdate)}"),
            Text("History offset: ${sp.historyOffset.inMinutes}min"),
            SizedBox(
              width: double.infinity,
              child: Card(
                clipBehavior: Clip.hardEdge,
                color: sp.iaq.color,
                child: Padding(
                  padding: const EdgeInsets.all(10),
                  child: Text(
                    "Index of Air Quality: ${sp.iaq.name}",
                    style: Theme.of(context)
                        .textTheme
                        .bodySmall
                        ?.copyWith(color: Colors.black),
                  ),
                ),
              ),
            ),
            GridView.count(
              physics: const NeverScrollableScrollPhysics(),
              shrinkWrap: true,
              crossAxisCount: 2,
              children: [
                if (sp.co2 != null) _ValueCard(value: sp.co2!),
                if (sp.temp != null) _ValueCard(value: sp.temp!),
                if (sp.hum != null) _ValueCard(value: sp.hum!),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _ValueCard<T extends num> extends StatelessWidget {
  const _ValueCard({required this.value});

  final SensorPuckValue<T> value;

  @override
  Widget build(BuildContext context) {
    var iaq = value.iaq();
    return Card(
      clipBehavior: Clip.antiAlias,
      child: CustomPaint(
        painter: value.history.isNotEmpty
            ? HistoryPainter(
                value: value,
                graphColor: Color(0xFF353535),
                textColor:
                    Theme.of(context).colorScheme.onBackground.withAlpha(180),
              )
            : null,
        child: Stack(
          alignment: Alignment.bottomCenter,
          children: [
            if (iaq != null)
              Container(
                width: double.infinity,
                padding: const EdgeInsets.all(10),
                decoration: BoxDecoration(
                  color: iaq.color,
                  borderRadius: BorderRadius.circular(12.0),
                ),
                child: Text(
                  iaq.name,
                  style: Theme.of(context)
                      .textTheme
                      .bodySmall
                      ?.copyWith(color: Colors.black),
                ),
              ),
            Column(
              crossAxisAlignment: CrossAxisAlignment.center,
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Text(
                  "${value.value}",
                  style: Theme.of(context)
                      .textTheme
                      .displayLarge
                      ?.copyWith(fontWeight: FontWeight.bold),
                ),
                Text(
                  value.unit,
                  style: Theme.of(context)
                      .textTheme
                      .bodyLarge
                      ?.copyWith(color: Theme.of(context).hintColor),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class HistoryPainter<T extends num> extends CustomPainter {
  const HistoryPainter({
    required this.value,
    required this.graphColor,
    required this.textColor,
  });

  final SensorPuckValue<T> value;
  final Color graphColor;
  final Color textColor;

  @override
  void paint(Canvas canvas, Size size) {
    var history = value.history;

    var historyWithCurrent = [...history, value.value];
    var historyMax = historyWithCurrent.reduce(max);
    var historyMin = historyWithCurrent.reduce(min);
    // make sure 0 is in the range [bottomValue; topValue]
    var topValue = max(historyMax * 1.2, 0);
    var bottomValue = min(historyMin * 0.8, 0);

    double valueToY(T value) {
      return (1 - (value - bottomValue) / (topValue - bottomValue)) *
          size.height;
    }

    var graphPaint = Paint()..color = graphColor;
    var textPaint = Paint()
      ..color = textColor
      ..strokeWidth = 1;

    var points = [
      for (int i = 0; i < history.length; i++)
        Offset((size.width / history.length) * i, valueToY(history[i]))
    ];

    points.add(Offset(size.width, valueToY(value.value)));
    points.add(Offset(size.width, valueToY(0 as T)));
    points.add(Offset(0, valueToY(0 as T)));

    canvas.drawPath(Path()..addPolygon(points, true), graphPaint);

    var textStyle = TextStyle(color: textColor, fontSize: 12);

    void drawTextAnchorTopLeft(Offset offset, String text) {
      TextPainter(
        text: TextSpan(text: text, style: textStyle),
        textDirection: TextDirection.ltr,
      )
        ..layout()
        ..paint(canvas, offset);
    }

    var entireGraphDuration = timeBetweenHistoryEntries * history.length;

    String formatDuration(Duration dur) {
      if (dur.inHours > 0) {
        var hours = dur.inMinutes / 60;
        return "${toStringAsFixedWithoutTrailingZeroes(hours, 1)}h";
      } else if (dur.inMinutes > 0) {
        return "${dur.inMinutes}min";
      }
      return "${dur.inSeconds}s";
    }

    canvas.drawLine(
        Offset(size.width / 2, 0), Offset(size.width / 2, 20), textPaint);
    drawTextAnchorTopLeft(Offset(size.width / 2 + 4, 4),
        "${formatDuration(entireGraphDuration ~/ 2)} ago");
    drawTextAnchorTopLeft(
        Offset(4, 4), "${formatDuration(entireGraphDuration)} ago");
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => true;
}
