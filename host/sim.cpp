// PC-Test fuer Kleine Welt: laesst den Spielkern ohne Board laufen.
// Ein kleiner "Bot" bedient Steuerkreuz und Schwert-Knopf wie ein Mensch mit
// zwei Daumen, spielt alle Raeume durch und speichert Bilder als PPM.
//   g++ -O2 -std=c++17 -I../firmware-src/KleineWelt sim.cpp ../firmware-src/KleineWelt/game.cpp -o sim
//   ./sim out/
#include "game.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>

using namespace kw;
static uint16_t fbuf[SCR * SCR], bgbuf[SCR * SCR];
static std::string outDir = "out/";
static float PX, PY, BX, BY;

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

static float cc(int i) { return -31 + i * 48 + 24; }
static uint32_t now = 1000;
static const int FRAME = 33;
static int fails = 0;

// ein Frame mit Steuerkreuz-Auslenkung (sx,sy in -1..1) und optional Knopf
static void step(float sx = 0, float sy = 0, bool btn = false) {
  Touch t[2]; int n = 0;
  if (sx != 0 || sy != 0) { t[n].x = (int16_t)(PX + sx * 34); t[n].y = (int16_t)(PY + sy * 34); n++; }
  if (btn) { t[n].x = (int16_t)BX; t[n].y = (int16_t)BY; n++; }
  now += FRAME; tick(now, t, n);
}
static void idle(float sec) { for (int f = 0; f < sec * 1000 / FRAME; f++) step(); }
static void tapAt(int x, int y) { Touch t{(int16_t)x, (int16_t)y}; now += FRAME; tick(now, &t, 1); step(); }

static void steerTo(float tx, float ty, bool btn = false) {
  Debug d = debug();
  float dx = tx - d.hx, dy = ty - d.hy, len = sqrtf(dx * dx + dy * dy);
  if (len < 0.5f) { step(0, 0, btn); return; }
  float m = fminf(1, 0.25f + len / 30);
  step(dx / len * m, dy / len * m, btn);
}
static bool walkTo(int i, int j, float maxSec = 6) {
  for (int f = 0; f < maxSec * 1000 / FRAME; f++) {
    Debug d = debug();
    if (fabsf(d.hx - cc(i)) < 6 && fabsf(d.hy - cc(j)) < 6) { step(); return true; }
    steerTo(cc(i), cc(j));
  }
  Debug d = debug();
  printf("  walkTo(%d,%d) nicht erreicht (Raum %d), Held bei %.0f,%.0f\n", i, j, d.room, d.hx, d.hy);
  fails++; return false;
}
static bool pushUntil(int k, int di, int dj, int ti, int tj, float maxSec = 8) {
  for (int f = 0; f < maxSec * 1000 / FRAME; f++) {
    Debug d = debug();
    if (d.bi[k] == ti && d.bj[k] == tj && !d.blockMoving) { step(); return true; }
    step((float)di, (float)dj);
  }
  Debug d = debug();
  printf("  pushUntil: Block %d bei %d,%d statt %d,%d\n", k, d.bi[k], d.bj[k], ti, tj);
  fails++; return false;
}
// alle Schleimlinge besiegen: hinlaufen, mit dem Knopf zuschlagen
static bool fightAll(float maxSec = 40, const char *shot = nullptr) {
  bool shotDone = false;
  for (int f = 0; f < maxSec * 1000 / FRAME; f++) {
    Debug d = debug();
    if (d.nSlimes == 0) { idle(0.3f); return true; }
    int best = 0; float bd = 1e9f;
    for (int k = 0; k < d.nSlimes; k++) {
      float dx = d.sx[k] - d.hx, dy = d.sy[k] - d.hy, dd = dx * dx + dy * dy;
      if (dd < bd) { bd = dd; best = k; }
    }
    float dist = sqrtf(bd);
    bool press = dist < 70 && (f % 8) == 0;
    if (dist < 50) step((d.sx[best] - d.hx) / dist * 0.2f, (d.sy[best] - d.hy) / dist * 0.2f, press);
    else steerTo(d.sx[best], d.sy[best], press);
    if (shot && !shotDone && press) { step(); save(shot); shotDone = true; }
  }
  printf("  Kampf nicht gewonnen (Raum %d, %d uebrig)\n", debug().room, debug().nSlimes);
  fails++; return false;
}
static bool goNorth(int expectRoom) {
  walkTo(5, 3); walkTo(5, 2);
  for (int f = 0; f < 120 && (debug().room != expectRoom || debug().fading); f++) step(0, -1);
  idle(0.2f);
  if (debug().room != expectRoom) { printf("  Raumwechsel nach %d fehlgeschlagen\n", expectRoom); fails++; return false; }
  printf("Raum %d betreten, Held %.0f,%.0f, HP %d\n", expectRoom, debug().hx, debug().hy, debug().hp);
  return true;
}

int main(int argc, char **argv) {
  if (argc > 1) outDir = std::string(argv[1]) + "/";
  controlCenters(PX, PY, BX, BY);
  begin(fbuf, bgbuf, 12345);

  step(); save("01_start");
  idle(6.5); save("02_ohne_hinweis");

  // Raum 0: Gegner besiegen
  fightAll(40, "03_schlag");
  idle(1.0);
  printf("Raum 0: Tuer %s\n", debug().doorOpen ? "offen" : "ZU");
  if (!debug().doorOpen) fails++;
  save("04_raum0_offen");
  goNorth(1);
  save("05_raum1");

  // Raum 1: Gegner, dann Block (3,4) -> (6,4) -> (6,7)
  fightAll();
  walkTo(4, 7); walkTo(4, 5); walkTo(2, 5); walkTo(2, 4);
  pushUntil(0, 1, 0, 6, 4);
  walkTo(5, 3); walkTo(6, 3);
  pushUntil(0, 0, 1, 6, 7);
  idle(0.6);
  printf("Raum 1: Tuer %s\n", debug().doorOpen ? "offen" : "ZU");
  if (!debug().doorOpen) fails++;
  save("06_raum1_geloest");
  walkTo(7, 5); walkTo(5, 4);
  goNorth(2);

  // Raum 2: zwei Gegner, zwei Bloecke
  fightAll();
  save("07_raum2");
  walkTo(6, 7); walkTo(6, 5); walkTo(5, 5); walkTo(5, 4);
  pushUntil(1, 1, 0, 8, 4);            // Block B (6,4) -> (8,4)
  walkTo(7, 3); walkTo(8, 3);
  pushUntil(1, 0, 1, 8, 5);            // -> Platte (8,5)
  walkTo(7, 4); walkTo(7, 6); walkTo(6, 5); walkTo(5, 5);
  pushUntil(0, -1, 0, 2, 5);           // Block A (4,5) -> (2,5)
  walkTo(3, 6); walkTo(2, 6);
  pushUntil(0, 0, -1, 2, 4);           // -> Platte (2,4)
  idle(0.6);
  printf("Raum 2: Tuer %s\n", debug().doorOpen ? "offen" : "ZU");
  if (!debug().doorOpen) fails++;
  save("08_raum2_geloest");
  walkTo(3, 4); walkTo(4, 4);
  goNorth(3);
  save("09_schatzkammer");

  // Raum 3: Truhe oeffnen
  walkTo(5, 6); walkTo(5, 5);
  step(0, -0.2f); step(0, 0, true); step();
  idle(0.5); save("10_truhe");
  idle(2.0);
  printf("Ende: Modus %d (1 = geschafft), Truhe %s\n", debug().mode, debug().chestOpen ? "offen" : "zu");
  if (debug().mode != 1) fails++;
  save("11_geschafft");

  // Zurueck durch die Suedtuer testen: neues Spiel, Raum 0 -> 1 -> zurueck
  tapAt(233, 233); idle(0.5);
  if (debug().room != 0 || debug().mode != 0) { printf("  Neustart fehlgeschlagen\n"); fails++; }
  fightAll(); idle(1.0); goNorth(1);
  walkTo(5, 7);
  for (int f = 0; f < 90 && (debug().room != 0 || debug().fading); f++) step(0, 1);
  idle(0.3);
  printf("Zurueck nach Sueden: Raum %d, Tuer Raum 0 %s\n", debug().room, debug().doorOpen ? "bleibt offen" : "ZU");
  if (debug().room != 0 || !debug().doorOpen) fails++;
  save("12_zurueck");

  // Umfallen: neues Spiel, in Raum 0 nichts tun
  begin(fbuf, bgbuf, 777);
  for (int f = 0; f < 60 * 30 && debug().mode != 2; f++) step();
  idle(2.0);
  printf("Nichtstun: Modus %d (2 = umgefallen)\n", debug().mode);
  if (debug().mode != 2) fails++;
  save("13_umgefallen");
  tapAt(233, 233); idle(0.3);
  printf("Nach Tipp: Modus %d, HP %d\n", debug().mode, debug().hp);
  if (debug().mode != 0 || debug().hp != 6) fails++;

  printf(fails ? "FEHLER: %d\n" : "ALLES OK\n", fails);
  return fails ? 1 : 0;
}
