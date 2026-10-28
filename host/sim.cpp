// PC-Test fuer Kleine Welt: rendert alle Bilder (Mallorca zu Mittag, goldener Stunde und Nacht,
// die anderen Welten bei Tag und Nacht), prueft Wischen (Bild und Welt), Wechsel nach Uhr,
// Ueberblendung, Menue, Uhr stellen, Nachtmond, Mondphase (gegen Ephemeriden), Aquarium
// (Schwappen, Fuettern), Game of Life und die Tipp-Aktionen in Mallorca.
//   g++ -O2 -std=c++17 -Ifirmware-src/KleineWelt host/sim.cpp firmware-src/KleineWelt/game.cpp -o sim
//   ./sim out/
#include "game.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>

using namespace kw;
static uint16_t fbuf[SCR * SCR], work[SCR * SCR];
static std::string outDir = "out/";
static uint32_t now = 1000;
static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("  FEHLER: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static void save(const std::string &name) {
  std::string p = outDir + name + ".ppm";
  FILE *f = fopen(p.c_str(), "wb");
  fprintf(f, "P6 %d %d 255\n", SCR, SCR);
  for (int i = 0; i < SCR * SCR; i++) {
    uint16_t c = fbuf[i];
    unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)};
    int x = i % SCR, y = i / SCR; float dx = x - 232.5f, dy = y - 232.5f;
    if (dx * dx + dy * dy > 233 * 233) rgb[0] = rgb[1] = rgb[2] = 0;   // rundes Display
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}
static void run(float sec, const Touch *t = nullptr, int n = 0) { for (int f = 0; f < sec * 30; f++) { now += 33; tick(now, t, n); } }
static void tap(int x, int y) { Touch t{(int16_t)x, (int16_t)y}; now += 33; tick(now, &t, 1); now += 33; tick(now, nullptr, 0); }
static void swipe(int x0, int y0, int x1, int y1) {
  for (int k = 0; k <= 8; k++) { Touch t{(int16_t)(x0 + (x1 - x0) * k / 8), (int16_t)(y0 + (y1 - y0) * k / 8)}; now += 33; tick(now, &t, 1); }
  now += 33; tick(now, nullptr, 0);
}
static void longPress(int x, int y) { Touch t{(int16_t)x, (int16_t)y}; run(1.7f, &t, 1); run(0.1f); }
static int firstOf(int world) { for (int s = 0; s < sceneCount(); s++) if (sceneWorld(s) == world) return s; return -1; }

int main(int argc, char **argv) {
  if (argc > 1) outDir = std::string(argv[1]) + "/";
  begin(fbuf, work, 4242);
  setDate(2026, 10, 28);
  int N = sceneCount();
  printf("%d Bilder\n", N);
  const int MOND = firstOf(W_MOND), AQUA = firstOf(W_AQUARIUM), KUNST = firstOf(W_KUNST);

  // ---- jedes Bild rendern
  const float times[3] = {13.0f, 18.9f, 22.5f};
  for (int s = 0; s < N; s++) {
    testSetScene(s);
    int w = sceneWorld(s);
    for (int k = 0; k < 3; k++) {
      if (w != W_MALLORCA && k == 1) continue;
      testSetHours(times[k], k == 0 && (s == 3 || s == 6) ? 3 : 10);   // Mittag in Finca/Muehlen: Maerz (Mandelbluete)
      run(k == 0 ? (w == W_KUNST ? 8.0f : 4.0f) : 3.0f);
      char name[32]; snprintf(name, sizeof name, "s%02d_%d", s, k);
      save(name);
    }
    CHECK(debug().scene == s, "Bild %d nicht geladen", s);
    printf("Bild %2d %-16s ok\n", s, sceneName(s));
  }

  // ---- Rechenzeit je Welt (PC, nur zum Vergleich mit Mallorca)
  for (int s : {0, 2, KUNST, KUNST + 1, KUNST + 2, KUNST + 3, AQUA, MOND}) {
    testSetScene(s); testSetHours(13, 10); run(4);
    auto t0 = std::chrono::steady_clock::now();
    run(10);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 300;
    printf("Rechenzeit %-16s %.2f ms/Bild\n", sceneName(s), ms);
  }

  // ---- Mondphase gegen PyEphem (Weltzeit -> Ortszeit umgerechnet)
  struct { int y, m, d; float h, frac, toFull, toNew; } ref[] = {
    {2026, 10, 28, 19.0f, 0.9089f, 26.87f, 11.543f},
    {2027, 3, 15, 7.0f, 0.453f, 7.197f, 22.744f},
    {2028, 6, 15, 23.0f, 0.4365f, 20.882f, 6.894f},
    {2026, 7, 4, 2.0f, 0.8565f, 25.608f, 10.405f},
    {2026, 11, 24, 15.883f, 0.9982f, 29.5f, 14.416f},
  };
  for (auto &r : ref) {
    float f, tf, tn; testMoon(r.y, r.m, r.d, r.h, f, tf, tn);
    printf("Mond %04d-%02d-%02d %05.2f: %.4f (soll %.4f)  Voll in %.2f (soll %.2f)  Neu in %.2f (soll %.2f)\n",
           r.y, r.m, r.d, r.h, f, r.frac, tf, r.toFull, tn, r.toNew);
    CHECK(fabsf(f - r.frac) < 0.01f, "Beleuchtung weicht ab");
    CHECK(fabsf(tf - r.toFull) < 0.3f || (r.toFull > 29 && tf < 0.3f), "Vollmond-Zeitpunkt weicht ab");
    CHECK(fabsf(tn - r.toNew) < 0.3f, "Neumond-Zeitpunkt weicht ab");
  }
  testSetScene(MOND); testSetHours(21.0f, 10); run(1); tap(233, 220); run(0.5f); save("mond_info");

  // ---- Wechsel nach Uhr (alle Welten, 10 Minuten)
  testSetScene(-1); testSetHours(10.0f, 10); run(5);
  int a = debug().scene;
  testSetHours(10.0f + 10.5f / 60, 10); run(0.5f);
  int b = debug().scene; bool fading = debug().fading;
  run(4);
  printf("Wechsel nach 10 Min: %d -> %d (Ueberblendung %s)\n", a, b, fading ? "ja" : "NEIN");
  CHECK(b == (a + 1) % N && fading, "10-Minuten-Wechsel");

  // ---- Wischen waagerecht = naechstes Bild
  int before = debug().scene;
  swipe(380, 233, 120, 233); run(1.2f); save("wisch_mitte"); run(3);
  printf("Wischen: %d -> %d\n", before, debug().scene);
  CHECK(debug().scene == (before + 1) % N, "Wischen nach links");
  swipe(120, 233, 380, 233); run(4);
  CHECK(debug().scene == before, "Zurueckwischen");

  // ---- Wischen senkrecht = naechste/vorige Welt (landet jeweils auf dem ersten Bild der Welt)
  for (int k = 0; k < 5; k++) {
    int w0 = sceneWorld(debug().scene);
    swipe(233, 380, 233, 120); run(4);
    int s1 = debug().scene, w1 = (w0 + 1) % W_COUNT;
    printf("Welt hoch: %s -> %s\n", sceneName(firstOf(w0)), sceneName(s1));
    CHECK(s1 == firstOf(w1), "Welt hoch landet nicht auf %s", sceneName(firstOf(w1)));
  }
  for (int k = 0; k < 5; k++) {
    int w0 = sceneWorld(debug().scene);
    swipe(233, 120, 233, 380); run(4);
    int s1 = debug().scene, w1 = (w0 + W_COUNT - 1) % W_COUNT;
    CHECK(s1 == firstOf(w1), "Welt runter landet nicht auf %s (ist %s)", sceneName(firstOf(w1)), sceneName(s1));
  }
  // aus der Mitte einer Welt: runter = Anfang der vorigen Welt
  testSetScene(-1);
  while (debug().scene != 4) { swipe(380, 233, 120, 233); run(0.2f); }
  run(4); swipe(233, 120, 233, 380); run(4);
  CHECK(debug().scene == MOND, "aus Mallorca runter -> Mond (ist %s)", sceneName(debug().scene));

  // ---- Menue
  longPress(233, 233);
  CHECK(debug().ui == 1, "Menue nicht offen");
  save("menue");
  tap(233, 222 * 1);   // Wechsel 10 -> 30
  tap(233, 274);       // Welten
  CHECK(debug().ui == 2, "Welten-Seite nicht offen");
  tap(233, 170); tap(233, 222); tap(233, 326);   // Mallorca, Kunst, Mond aus -> nur Aquarium
  tap(233, 274);                                  // Aquarium aus? darf nicht (letzte Welt)
  save("menue_welten");
  tap(233, 383);       // Zurueck
  CHECK(debug().ui == 1, "Zurueck zum Menue");
  tap(233, 326);       // Nachtmond 21
  save("menue_geaendert");
  tap(233, 383);       // Fertig
  Settings st; bool changed = takeSettingsChange(st);
  printf("Einstellungen: geaendert %s, Welten %x, Wechsel %d, Nachtmond %d\n", changed ? "ja" : "NEIN", st.worlds, st.interval, st.nightFrom);
  CHECK(changed && st.worlds == (1 << W_AQUARIUM) && st.interval == 30 && st.nightFrom == 21, "Menue-Einstellungen");
  CHECK(debug().ui == 0, "Menue nicht zu");
  run(4);
  CHECK(debug().scene == AQUA, "nur Aquarium aktiv (ist %d)", debug().scene);

  // ---- Uhr ueber das Menue stellen
  testSetHours(-1, 10); setClock(10, 0, 0, 10); run(0.2f);
  longPress(233, 233);
  tap(233, 170);
  CHECK(debug().clockUi, "Uhr-Seite nicht offen");
  tap(150, 160); tap(300, 160); tap(300, 160); run(0.1f);
  save("uhr");
  tap(233, 380); run(0.1f);
  int h, m; bool ch = takeClockChange(h, m);
  printf("Uhr gestellt: %s %02d:%02d\n", ch ? "ja" : "NEIN", h, m);
  CHECK(ch && h == 11 && m == 2, "Uhr stellen");
  CHECK(debug().ui == 1, "nach OK zurueck ins Menue");
  tap(233, 383);
  CHECK(debug().ui == 0, "Menue zu");

  // ---- Nachtmond
  Settings s2; s2.worlds = 0x0F; s2.interval = 10; s2.nightFrom = 22;
  setSettings(s2);
  testSetHours(23.0f, 10); run(4);
  printf("Nachtmond 23 Uhr: Bild %d, aktiv %s, Helligkeit %d\n", debug().scene, debug().nightMoon ? "ja" : "NEIN", wantBrightness());
  CHECK(debug().scene == MOND && debug().nightMoon, "Nachtmond um 23 Uhr");
  save("nachtmond");
  swipe(380, 233, 120, 233); run(4);
  CHECK(debug().scene != MOND && !debug().nightMoon, "Wegwischen beendet Nachtmond");
  testSetHours(3.0f, 10); run(1);
  CHECK(debug().scene != MOND, "weggewischt bleibt bis morgens");
  testSetHours(9.0f, 10); run(1); testSetHours(23.5f, 10); run(4);
  CHECK(debug().scene == MOND, "naechste Nacht wieder Nachtmond");
  testSetHours(21.5f, 10); run(4);
  CHECK(debug().scene != MOND, "vor 22 Uhr kein Nachtmond");

  // ---- Aquarium: Schwappen und Fuettern
  testSetScene(AQUA); testSetHours(13.0f, 10); setGravity(0, 1); run(3);
  setGravity(sinf(0.5f), cosf(0.5f));
  float maxA = 0;
  for (int k = 0; k < 90; k++) { run(1.0f / 30); maxA = fmaxf(maxA, debug().waterAngle); }
  run(1); save("aqua_schwapp"); run(5);
  printf("Schwappen: Ziel 0.50, Spitze %.2f, Ruhe %.2f\n", maxA, debug().waterAngle);
  CHECK(maxA > 0.55f && fabsf(debug().waterAngle - 0.5f) < 0.05f, "Schwappen");
  save("aqua_schief");
  setGravity(0, 1); run(8);
  {   // Stelle nahe der Oberflaeche suchen, an der kein Fisch schwimmt
    int bx = 233, best = -1;
    for (int x = 120; x <= 346; x += 6) {
      int dmin = 1 << 30, fx, fy;
      for (int k = 0; testFishPos(k, fx, fy); k++) dmin = std::min(dmin, (fx - x) * (fx - x) + (fy - 140) * (fy - 140));
      if (dmin > best) { best = dmin; bx = x; }
    }
    tap(bx, 140); run(0.3f);
  }
  printf("Futter: %d Flocken\n", debug().nFood);
  CHECK(debug().nFood == 6, "Futter streuen");
  run(2); save("aqua_futter");
  run(25);
  printf("Futter gefressen: %d\n", debug().foodEaten);
  CHECK(debug().foodEaten >= 3, "Fische fressen");
  int fx, fy;
  if (testFishPos(0, fx, fy)) { tap(fx, fy); run(1); }

  // ---- Game of Life lebt
  testSetScene(KUNST + 3); run(20);
  printf("Game of Life: %d Zellen\n", debug().population);
  CHECK(debug().population > 30, "Game of Life ausgestorben");
  for (int k = 0; k < 3; k++) { tap(233, 233); run(0.2f); }
  testSetScene(KUNST); run(6); tap(233, 233); run(0.4f); save("kunst_tipp");
  testSetScene(KUNST + 2); run(4); tap(233, 150); run(0.2f); save("sterne_schnuppe");

  // ---- Mallorca allein: alter Ablauf
  Settings s3; s3.worlds = 1 << W_MALLORCA; setSettings(s3);
  testSetScene(-1); testSetHours(10.0f, 10); run(5);
  a = debug().scene;
  testSetHours(10.0f + 10.5f / 60, 10); run(4);
  CHECK(sceneWorld(a) == W_MALLORCA && debug().scene == (a + 1) % 10, "Mallorca allein: 10-Minuten-Wechsel");
  swipe(233, 380, 233, 120); run(4);
  CHECK(sceneWorld(debug().scene) == W_MALLORCA, "Mallorca allein: Welt-Wischen bleibt in Mallorca");

  // Boot in der Cala anlegen lassen
  testSetScene(0); testSetHours(14.0f, 7); run(4);
  int target = -1;
  for (int tries = 0; tries < 200 && target < 0; tries++) {
    Debug d = debug();
    for (int k = 0; k < d.nBoats; k++) if (d.boatState[k] == 0 && d.boatX[k] > 50 && d.boatX[k] < 180 && d.boatY[k] > 120) target = k;
    if (target < 0) run(1);
  }
  CHECK(target >= 0, "kein Boot gefunden");
  if (target >= 0) {
    Debug d = debug(); tap((int)(d.boatX[target] * 2), (int)(d.boatY[target] * 2 - 6)); run(30);
    printf("Boot angelegt: %d\n", debug().boatsDocked);
    CHECK(debug().boatsDocked == 1, "Boot anlegen");
  }

  printf(fails ? "FEHLER: %d\n" : "ALLES OK\n", fails);
  return fails ? 1 : 0;
}
