// PC-Test fuer Kleine Welt "Cala": rendert die Bucht zu verschiedenen Tageszeiten,
// prueft die Tipp-Aktionen und speichert Bilder als PPM.
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

static void save(const char *name) {
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
static void run(float sec, const Touch *t = nullptr, int n = 0) {
  for (int f = 0; f < sec * 30; f++) { now += 33; tick(now, t, n); }
}
static void tap(int x, int y) { Touch t{(int16_t)x, (int16_t)y}; now += 33; tick(now, &t, 1); now += 33; tick(now, nullptr, 0); }

int main(int argc, char **argv) {
  if (argc > 1) outDir = std::string(argv[1]) + "/";
  begin(fbuf, work, 4242);

  struct { float h; int m; const char *name; } shots[] = {
    {7.9f, 10, "01_morgen"}, {12.5f, 10, "02_mittag"}, {18.6f, 10, "03_goldene_stunde"},
    {19.25f, 10, "04_sonnenuntergang"}, {20.0f, 10, "05_daemmerung"}, {23.0f, 10, "06_nacht"},
  };
  for (auto &s : shots) { testSetHours(s.h, s.m); run(9); save(s.name); }

  // Boot antippen -> legt am Steg an
  testSetHours(14.0f, 7); run(1);
  int target = -1;
  for (int tries = 0; tries < 200 && target < 0; tries++) {
    Debug d = debug();
    for (int k = 0; k < 3; k++)
      if (d.boatState[k] == 0 && d.boatX[k] > 40 && d.boatX[k] < 190 && d.boatY[k] > 120) target = k;
    if (target < 0) run(1);
  }
  if (target < 0) { printf("kein Boot zum Antippen gefunden\n"); fails++; }
  else {
    Debug d = debug();
    tap((int)(d.boatX[target] * 2), (int)(d.boatY[target] * 2 - 6));
    run(1);
    printf("Boot %d nach Tipp: Zustand %d (1 = legt an)\n", target, debug().boatState[target]);
    if (debug().boatState[target] != 1) fails++;
    run(30);
    printf("nach 30 s: angelegt %d\n", debug().boatsDocked);
    if (debug().boatsDocked != 1) fails++;
    save("07_boot_am_steg");
  }
  // Fisch, Moewe
  tap(220, 270); tap(300, 80); run(0.4f); save("08_fisch_moewe");

  // Uhr stellen: lange druecken, Stunde +1, OK
  testSetHours(-1, 10); setClock(10, 0, 0, 10); run(0.2f);
  Touch t{233, 300};
  run(1.7f, &t, 1); run(0.1f);
  printf("Uhr-Menue nach langem Druecken: %s\n", debug().clockUi ? "offen" : "ZU");
  if (!debug().clockUi) fails++;
  tap(150, 160); run(0.1f);                 // Stunde +1
  tap(300, 160); tap(300, 160); run(0.1f);  // Minute +1 +1
  Touch hold{300, 160}; run(2.0f, &hold, 1); run(0.1f);   // gedrueckt halten: zaehlt schnell weiter
  save("09_uhr_stellen");
  tap(300, 300); run(0.1f);                 // Minute -1
  tap(233, 380); run(0.1f);
  int h, m;
  bool ch = takeClockChange(h, m);
  printf("Uhr gestellt: %s %02d:%02d, Anzeige %.2f h\n", ch ? "ja" : "NEIN", h, m, debug().hours);
  if (!ch || h != 11 || m < 12 || m > 40 || debug().clockUi) fails++;

  printf(fails ? "FEHLER: %d\n" : "ALLES OK\n", fails);
  return fails ? 1 : 0;
}
