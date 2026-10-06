#pragma once
// Kleine Welt - Spielkern. Kennt weder Arduino noch Display noch Touch-Chip:
// er bekommt einen Framebuffer (466x466 RGB565) und pro Frame die Fingerpunkte.
// Dadurch laeuft derselbe Code auf dem Board und im PC-Testprogramm (host/).
#include <stdint.h>

namespace kw {

constexpr int SCR = 466;

struct Touch { int16_t x, y; };

// fb: Bild, das pro Frame ans Panel geht. bg: vorgerenderter Raum (gleiche Groesse).
void begin(uint16_t *fb, uint16_t *bg, uint32_t seed);

// Ein Spielschritt + Zeichnen nach fb. pts/n = Finger, die gerade aufliegen (0..2).
void tick(uint32_t nowMs, const Touch *pts, int n);

// Nur fuer Tests / Seriell-Log
struct Debug {
  float hx, hy;      // Held (Mitte der Fuesse)
  int hp;            // halbe Herzen, max 6
  int room;          // 0..3
  bool fading;
  int nSlimes;       // lebende Schleimlinge
  float sx[4], sy[4];
  int nBlocks, bi[2], bj[2];
  bool blockMoving;
  bool doorOpen;     // Nordtuer
  bool chestOpen;
  int mode;          // 0 spielen, 1 geschafft, 2 umgefallen
};
Debug debug();
void controlCenters(float &padX, float &padY, float &btnX, float &btnY);

}  // namespace kw
