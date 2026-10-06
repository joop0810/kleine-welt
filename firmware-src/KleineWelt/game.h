#pragma once
// Kleine Welt - zehn kleine Mallorca-Szenen als Wohlfuehl-Deko.
// Der Kern kennt weder Arduino noch Display noch Touch-Chip: er bekommt einen
// Framebuffer (466x466 RGB565), die Uhrzeit und pro Frame die Fingerpunkte.
// Derselbe Code laeuft auf dem Board und im PC-Testprogramm (host/).
#include <stdint.h>

namespace kw {

constexpr int SCR = 466;

struct Touch { int16_t x, y; };

// fb: Bild fuer das Panel. work: Arbeitsspeicher (mind. 466*466*2 Byte, PSRAM).
void begin(uint16_t *fb, uint16_t *work, uint32_t seed);

// Uhrzeit aus der RTC (Ortszeit). Wird regelmaessig gesetzt; dazwischen zaehlt der Kern selbst.
void setClock(int hour, int minute, int second, int month);

// Ein Schritt + Zeichnen nach fb. pts/n = Finger, die gerade aufliegen (0..2).
void tick(uint32_t nowMs, const Touch *pts, int n);

// Hat der Nutzer am Geraet die Uhr gestellt? Dann true und neue Zeit (einmalig).
bool takeClockChange(int &hour, int &minute);

// Gewuenschte Display-Helligkeit (Tag hell, Nacht gedimmt)
uint8_t wantBrightness();

// Nur fuer Tests / Seriell-Log
struct Debug {
  float hours;        // Uhrzeit 0..24
  float daylight;     // 0 Nacht .. 1 Tag
  int scene;          // aktuelle Szene
  bool fading;        // Ueberblendung laeuft
  int nBoats, boatsDocked;
  int boatState[5];   // 0 faehrt, 1 legt an, 2 liegt am Steg, 3 legt ab
  float boatX[5], boatY[5];
  bool clockUi;
  bool lightsOn;
};
Debug debug();
int sceneCount();
const char *sceneName(int s);
// Test: Uhrzeit fest vorgeben (Stunden, -1 = aus) bzw. Szene festhalten (-1 = aus)
void testSetHours(float h, int month);
void testSetScene(int s);

}  // namespace kw
