import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:sensor_puck_web/pages/home_page.dart';
import 'package:flutter_web_plugins/url_strategy.dart';

void main() {
  usePathUrlStrategy();

  runApp(MaterialApp.router(
    debugShowCheckedModeBanner: false,
    themeMode: ThemeMode.dark,
    theme: ThemeData.dark(useMaterial3: true),
    routerConfig: GoRouter(
      routes: [
        GoRoute(
          path: '/',
          builder: (context, state) {
            // turn URL safe base64 into normal base64
            var data = (state.uri.queryParameters["d"] ?? "")
                .replaceAll("-", "+")
                .replaceAll("_", "/");
            return HomePage(b64Data: data);
          },
        )
      ],
    ),
  ));
}
