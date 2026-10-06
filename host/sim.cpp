// PC-Test fuer Kleine Welt: laesst den Spielkern ohne Board laufen.
// Ein kleiner "Bot" bedient den Touchscreen so wie ein Mensch (halten/tippen),
// spielt den Raum durch und speichert Bilder als PPM.
//   g++ -O2 -std=c++17 -I../firmware-src/KleineWelt sim.cpp ../firmware-src/KleineWelt/game.cpp -o sim
//   ./sim out/
#include "game.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>

using namespace kw;
static uint16_t fbuf[SCR * SCR], bgbuf[SCR * SCR];
static std::string outDir = "out/";

static void save(const char *name) {
  std::string p = outDir + name + ".ppm";
  FILE *f = fopen(p.c_str(), "wb");
  fprintf(f, "P6 %d %d 255\n", SCR, SCR);
  for (int i = 0; i < SCR * SCR; i++) {
    uint16_t c = fbuf[i];
    unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)};
    // ausserhalb des runden Displays schwarz, wie auf dem Geraet
    int x = i % SCR, y = i / SCR; float dx = x - 232.5f, dy = y - 232.5f;
    if (dx * dx + dy * dy > 233 * 233) rgb[0] = rgb[1] = rgb[2] = 0;
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}

static float cc(int i) { return -31 + i * 48 + 24; }
static uint32_t now = 1000;
static const int FRAME = 33;
static void step(bool d, int x, int y) { now += FRAME; tick(now, d, x, y); }

// Finger halten, bis der Held am Ziel ist (Ziel = Kachel). Finger zielt auf den Kopf (y-8).
static bool walkTo(int i, int j, float maxSec = 6) {
  for (int f = 0; f < maxSec * 1000 / FRAME; f++) {
    Debug d = debug();
    if (fabsf(d.hx - cc(i)) < 7 && fabsf(d.hy - cc(j)) < 7) { step(false, 0, 0); return true; }
    step(true, (int)cc(i), (int)cc(j) - 12);
  }
  step(false, 0, 0);
  Debug d = debug();
  printf("  walkTo(%d,%d) nicht erreicht, Held bei %.0f,%.0f\n", i, j, d.hx, d.hy);
  return false;
}
// gegen den Block druecken (Finger weit in Schubrichtung halten), bis er auf (ti,tj) steht
static bool pushUntil(int di, int dj, int ti, int tj, float maxSec = 8) {
  for (int f = 0; f < maxSec * 1000 / FRAME; f++) {
    Debug d = debug();
    if (d.bi == ti && d.bj == tj && !d.blockMoving) { step(false, 0, 0); return true; }
    step(true, (int)(d.hx + di * 90), (int)(d.hy - 12 + dj * 90));
  }
  step(false, 0, 0);
  Debug d = debug();
  printf("  pushUntil: Block bei %d,%d statt %d,%d\n", d.bi, d.bj, ti, tj);
  return false;
}
static void idle(float sec) { for (int f = 0; f < sec * 1000 / FRAME; f++) step(false, 0, 0); }

int main(int argc, char **argv) {
  if (argc > 1) outDir = std::string(argv[1]) + "/";
  begin(fbuf, bgbuf, 12345);
  int fails = 0;

  step(false, 0, 0); save("01_start");
  idle(2.0); save("02_slime_kommt");

  // 1) Schleimling erledigen: auf ihn tippen (Held laeuft hin und schlaegt)
  int taps = 0; bool shot = false;
  for (int f = 0; f < 600 && debug().slimeAlive; f++) {
    Debug d = debug();
    bool down = (f % 12) == 0;
    if (down) taps++;
    step(down, (int)d.sx, (int)d.sy - 9);
    if (!shot && f > 20 && (f % 12) == 3) { save("03_schlag"); shot = true; }
  }
  printf("Schleimling %s nach %d Tipps, Held HP %d\n", debug().slimeAlive ? "LEBT NOCH" : "besiegt", taps, debug().hp);
  if (debug().slimeAlive) fails++;
  idle(0.5); save("04_nach_kampf");

  // 2) Block (3,4) -> (7,4) -> (7,7)
  if (!walkTo(2, 5) || !walkTo(2, 4)) fails++;
  save("05_vor_dem_block");
  if (!pushUntil(1, 0, 7, 4)) fails++;
  save("06_block_rechts");
  if (!walkTo(6, 3) || !walkTo(7, 3)) fails++;
  if (!pushUntil(0, 1, 7, 7)) fails++;
  idle(0.6);
  printf("Tuer %s\n", debug().doorOpen ? "offen" : "ZU");
  if (!debug().doorOpen) fails++;
  save("07_tuer_offen");

  // 3) Durch die Tuer
  if (!walkTo(6, 5) || !walkTo(5, 4) || !walkTo(5, 2)) fails++;
  for (int f = 0; f < 90 && debug().mode == 0; f++) step(true, (int)cc(5), (int)cc(0));
  step(false, 0, 0);
  idle(0.8);
  printf("Modus am Ende: %d (1 = geschafft)\n", debug().mode);
  if (debug().mode != 1) fails++;
  save("08_geschafft");

  // 4) Neustart per Tipp, dann festgefahrener Block: in die Ecke oben links schieben
  step(true, 233, 233); step(false, 0, 0);
  idle(0.3);
  if (!walkTo(4, 7) || !walkTo(4, 5) || !walkTo(3, 5)) fails++;
  pushUntil(0, -1, 3, 2, 5);       // nach oben an die Wand (Sackgasse)
  idle(1.0); save("09_block_steckt");
  idle(2.5);
  Debug d = debug();
  printf("Block nach Festsitzen bei %d,%d (Start 3,4)\n", d.bi, d.bj);
  if (d.bi != 3 || d.bj != 4) fails++;
  save("10_block_zurueck");

  // 5) Schaden: Held stellt sich neben den Schleimling und wartet
  begin(fbuf, bgbuf, 777);
  idle(7.0);
  printf("HP nach 7 s Nichtstun: %d\n", debug().hp);
  save("11_getroffen");

  // 6) Hieb im Detail: kurz rechts neben den Held tippen
  begin(fbuf, bgbuf, 5);
  idle(6.5);   // Hinweistext ist weg
  { Debug h = debug(); step(true, (int)h.hx + 50, (int)h.hy - 12); step(false, 0, 0); }
  step(false, 0, 0); save("12_hieb_a");
  step(false, 0, 0); save("12_hieb_b");

  printf(fails ? "FEHLER: %d\n" : "ALLES OK\n", fails);
  return fails ? 1 : 0;
}
