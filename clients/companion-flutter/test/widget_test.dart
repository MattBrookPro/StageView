// Smoke test: the app builds and shows its header before any connection.

import 'package:flutter_test/flutter_test.dart';
import 'package:stage_companion/main.dart';

void main() {
  testWidgets('app builds and shows the title + connect prompt', (tester) async {
    await tester.pumpWidget(const CompanionApp());
    expect(find.text('StageView · Monitor Mix'), findsOneWidget);
    expect(find.text('enter the engine IP and tap Connect'), findsOneWidget);
  });
}
