import 'dart:math';

import 'package:flutter/material.dart';
import 'package:intl/intl.dart' hide TextDirection;
import 'package:sensor_puck_web/sensor_puck_data.dart';

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
        painter: HistoryPainter(
          value: value,
          graphColor: Color(0xFF353535),
          textColor: Theme.of(context).colorScheme.onBackground.withAlpha(180),
        ),
        child: Stack(
          alignment: Alignment.bottomCenter,
          children: [
            if (iaq != null)
              Container(
                width: double.infinity,
                // margin: const EdgeInsets.all(5),
                padding: const EdgeInsets.all(5),
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

    // make sure 0 is in the range [bottomValue; topValue]
    var topValue = max(history.reduce((a, b) => max(a, b)) * 1.2, 0);
    var bottomValue = min(history.reduce((a, b) => min(a, b)) * 0.8, 0);

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

    canvas.drawLine(
        Offset(size.width / 2, 0), Offset(size.width / 2, 20), textPaint);

    void drawTextAnchorTopLeft(Offset offset, String text) {
      TextPainter(
        text: TextSpan(text: text, style: textStyle),
        textDirection: TextDirection.ltr,
      )
        ..layout()
        ..paint(canvas, offset);
    }

    drawTextAnchorTopLeft(Offset(size.width / 2 + 4, 4), "6h ago");
    drawTextAnchorTopLeft(Offset(4, 4), "12h ago");
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => true;
}
