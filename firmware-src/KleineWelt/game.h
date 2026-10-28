#pragma once
// Kleine Welt - Wohlfuehl-Deko mit vier Welten: Mallorca (zehn Szenen), Kunst (vier
// generative Bilder), Aquarium und Mond.
// Der Kern kennt weder Arduino noch Display noch Touch-Chip: er bekommt einen
// Framebuffer (466x466 RGB565), die Uhrzeit, die Lage und pro Frame die Fingerpunkte.
// Derselbe Code laeuft auf dem Board und im PC-Testprogramm (host/).
#include <stdint.h>

namespace kw {

constexpr int SCR = 466;

struct Touch { int16_t x, y; };

// Welten (Bits in Settings::worlds)
enum World : uint8_t { W_MALLORCA = 0, W_KUNST = 1, W_AQUARIUM = 2, W_MOND = 3, W_COUNT = 4 };

// Einstellungen, die am Geraet im Menue geaendert und vom Board gespeichert werden
struct Settings {
  uint8_t worlds = 0x0F;    // welche Welten im Wechsel laufen (mind. eine)
  uint8_t interval = 10;    // Minuten bis zum naechsten Bild (0 = kein Wechsel)
  int8_t nightFrom = -1;    // Nachtmond ab dieser Stunde bis Sonnenaufgang (-1 = aus)
};

// fb: Bild fuer das Panel. work: Arbeitsspeicher (mind. 466*466*2 Byte, PSRAM).
void begin(uint16_t *fb, uint16_t *work, uint32_t seed);

// Uhrzeit aus der RTC (Ortszeit). Wird regelmaessig gesetzt; dazwischen zaehlt der Kern selbst.
void setClock(int hour, int minute, int second, int month);
// Datum aus der RTC (fuer die Mondphase)
void setDate(int year, int month, int day);
// Schwerkraft in Bildkoordinaten (x nach rechts, y nach unten), in g. Ohne Aufruf: senkrecht.
void setGravity(float gx, float gy);

// Gespeicherte Einstellungen beim Start uebernehmen
void setSettings(const Settings &s);
const Settings &settings();
// Wurden die Einstellungen im Menue geaendert? Dann true und neue Werte (einmalig).
bool takeSettingsChange(Settings &s);

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
  int scene;          // aktuelles Bild (0..sceneCount()-1)
  int world;          // Welt des aktuellen Bildes
  bool fading;        // Ueberblendung laeuft
  int nBoats, boatsDocked;
  int boatState[5];   // 0 faehrt, 1 legt an, 2 liegt am Steg, 3 legt ab
  float boatX[5], boatY[5];
  bool clockUi;       // Uhr-stellen-Seite offen
  int ui;             // 0 nichts, 1 Menue, 2 Welten, 3 Uhr
  bool lightsOn;
  bool nightMoon;     // Nachtmond aktiv
  float moonFrac;     // beleuchteter Anteil 0..1
  float moonToFull;   // Tage bis Vollmond
  float moonToNew;    // Tage bis Neumond
  int nFood, foodEaten, nFish;
  float waterAngle;   // Neigung der Wasserlinie (rad, 0 = waagerecht)
  int population;     // lebende Zellen (Game of Life)
};
Debug debug();
int sceneCount();
const char *sceneName(int s);
int sceneWorld(int s);
// Test: Uhrzeit fest vorgeben (Stunden, -1 = aus) bzw. Bild festhalten (-1 = aus)
void testSetHours(float h, int month);
void testSetScene(int s);
// Test: Mondphase fuer Datum/Uhrzeit (Ortszeit) berechnen
void testMoon(int y, int mo, int d, float h, float &frac, float &toFull, float &toNew);
// Test: Fisch-Position (Bildschirmkoordinaten) fuer gezieltes Antippen
bool testFishPos(int k, int &x, int &y);

}  // namespace kw
