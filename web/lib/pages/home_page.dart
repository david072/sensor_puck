import 'package:flutter/material.dart';
import 'package:intl/intl.dart';
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
                if (sp.co2 != null)
                  _ValueCard(
                    value: sp.co2!.ppm.toStringAsFixed(0),
                    unit: sp.co2!.unit,
                    iaq: sp.co2!.iaq(),
                  ),
                if (sp.temp != null)
                  _ValueCard(
                    value: sp.temp!.temperature.toStringAsFixed(2),
                    unit: sp.temp!.unit,
                    iaq: sp.temp!.iaq(),
                  ),
                if (sp.hum != null)
                  _ValueCard(
                    value: sp.hum!.humidity.toStringAsFixed(2),
                    unit: sp.hum!.unit,
                    iaq: sp.hum!.iaq(),
                  ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _ValueCard extends StatelessWidget {
  const _ValueCard({
    required this.value,
    required this.unit,
    this.iaq,
  });

  final String value;
  final String unit;
  final Iaq? iaq;

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Stack(
        alignment: Alignment.bottomCenter,
        children: [
          if (iaq != null)
            Container(
              width: double.infinity,
              margin: const EdgeInsets.all(5),
              padding: const EdgeInsets.all(5),
              decoration: BoxDecoration(
                color: iaq!.color,
                borderRadius: BorderRadius.circular(7.0),
              ),
              child: Text(
                iaq!.name,
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
                value,
                style: Theme.of(context)
                    .textTheme
                    .displayLarge
                    ?.copyWith(fontWeight: FontWeight.bold),
              ),
              Text(
                unit,
                style: Theme.of(context)
                    .textTheme
                    .bodyLarge
                    ?.copyWith(color: Theme.of(context).hintColor),
              ),
            ],
          ),
        ],
      ),
    );
  }
}
