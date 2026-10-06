// PC-Test fuer Kleine Welt: rendert alle Szenen zu Mittag, goldener Stunde und Nacht,
// prueft Wischen, 10-Minuten-Wechsel, Ueberblendung, Tipp-Aktionen und das Uhr-Menue.
//   g++ -O2 -std=c++17 -I../firmware-src/KleineWelt sim.cpp ../firmware-src/KleineWelt/game.cpp -o sim
//   ./sim out/
#include "game.h"
#include <cstdio>
#include <cmath>
#include <string>

using namespace kw;
static uint16_t fbuf[SCR * SCR], work[SCR * SCR];
static std::string outDir = "out/";
static uint32_t now = 1000;
static int fails = 0;

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
static void swipe(int x0, int x1, int y) {
  for (int k = 0; k <= 8; k++) { Touch t{(int16_t)(x0 + (x1 - x0) * k / 8), (int16_t)y}; now += 33; tick(now, &t, 1); }
  now += 33; tick(now, nullptr, 0);
}

int main(int argc, char **argv) {
  if (argc > 1) outDir = std::string(argv[1]) + "/";
  begin(fbuf, work, 4242);
  int N = sceneCount();
  printf("%d Szenen\n", N);

  // jede Szene zu drei Tageszeiten (Oktober: Untergang 19:18)
  const float times[3] = {13.0f, 18.9f, 22.5f};
  for (int s = 0; s < N; s++) {
    testSetScene(s);
    for (int k = 0; k < 3; k++) {
      testSetHours(times[k], k == 0 && (s == 3 || s == 6) ? 3 : 10);   // Mittag in Finca/Muehlen: Maerz (Mandelbluete)
      run(k == 0 ? 4.0f : 1.5f);
      char name[32]; snprintf(name, sizeof name, "s%02d_%d", s, k);
      save(name);
    }
    if (debug().scene != s) { printf("Szene %d nicht geladen\n", s); fails++; }
  }

  // 10-Minuten-Wechsel ueber die Uhr
  testSetScene(-1); testSetHours(10.0f, 10); run(5);
  int a = debug().scene;
  testSetHours(10.0f + 10.5f / 60, 10); run(0.5f);
  int b = debug().scene; bool fading = debug().fading;
  run(4);
  printf("Wechsel nach 10 Min: %d -> %d (Ueberblendung %s)\n", a, b, fading ? "ja" : "NEIN");
  if (b != (a + 1) % N || !fading) fails++;

  // Wischen: nach links = naechste Szene
  int before = debug().scene;
  swipe(380, 120, 233); run(1.2f); save("wisch_mitte");
  run(3);
  printf("Wischen: %d -> %d\n", before, debug().scene);
  if (debug().scene != (before + 1) % N) fails++;
  swipe(120, 380, 233); run(4);
  if (debug().scene != before) { printf("Zurueckwischen fehlgeschlagen\n"); fails++; }

  // Boot in der Cala anlegen lassen
  testSetScene(0); testSetHours(14.0f, 7); run(4);
  int target = -1;
  for (int tries = 0; tries < 200 && target < 0; tries++) {
    Debug d = debug();
    for (int k = 0; k < d.nBoats; k++) if (d.boatState[k] == 0 && d.boatX[k] > 50 && d.boatX[k] < 180 && d.boatY[k] > 120) target = k;
    if (target < 0) run(1);
  }
  if (target < 0) { printf("kein Boot gefunden\n"); fails++; }
  else {
    Debug d = debug(); tap((int)(d.boatX[target] * 2), (int)(d.boatY[target] * 2 - 6)); run(30);
    printf("Boot angelegt: %d\n", debug().boatsDocked);
    if (debug().boatsDocked != 1) fails++;
  }

  // Uhr stellen
  testSetHours(-1, 10); setClock(10, 0, 0, 10); run(0.2f);
  Touch t{233, 300}; run(1.7f, &t, 1); run(0.1f);
  if (!debug().clockUi) { printf("Uhr-Menue nicht offen\n"); fails++; }
  tap(150, 160); tap(300, 160); tap(300, 160); run(0.1f);
  save("uhr");
  tap(233, 380); run(0.1f);
  int h, m; bool ch = takeClockChange(h, m);
  printf("Uhr gestellt: %s %02d:%02d\n", ch ? "ja" : "NEIN", h, m);
  if (!ch || h != 11 || m != 2) fails++;

  printf(fails ? "FEHLER: %d\n" : "ALLES OK\n", fails);
  return fails ? 1 : 0;
}
