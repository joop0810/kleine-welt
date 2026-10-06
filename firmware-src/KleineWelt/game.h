#pragma once
// Kleine Welt - Spielkern. Kennt weder Arduino noch Display noch Touch-Chip:
// er bekommt einen Framebuffer (466x466 RGB565) und pro Frame den Fingerzustand.
// Dadurch laeuft derselbe Code auf dem Board und im PC-Testprogramm (host/).
#include <stdint.h>

namespace kw {

constexpr int SCR = 466;

// fb: Bild, das pro Frame ans Panel geht. bg: vorgerenderter Raum (gleiche Groesse).
void begin(uint16_t *fb, uint16_t *bg, uint32_t seed);

// Ein Spielschritt + Zeichnen nach fb. touchDown/tx/ty = Finger jetzt.
void tick(uint32_t nowMs, bool touchDown, int tx, int ty);

// Nur fuer Tests / Seriell-Log
struct Debug {
  float hx, hy;      // Held (Mitte der Fuesse)
  int hp;            // halbe Herzen, max 6
  bool slimeAlive;
  float sx, sy;      // Schleimling
  int bi, bj;        // Steinblock (Kachel)
  bool blockMoving;
  bool doorOpen;
  int mode;          // 0 spielen, 1 geschafft, 2 umgefallen
  float plateX, plateY, doorX, doorY, blockStartX, blockStartY;
};
Debug debug();

}  // namespace kw
