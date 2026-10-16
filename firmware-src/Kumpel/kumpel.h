#pragma once
// Kumpel - interaktiver Schreibtisch-/Wandkumpel mit Home-Assistant-Seiten.
// Der Kern kennt weder Arduino noch WLAN noch Display: er bekommt Framebuffer,
// Uhrzeit, Fingerpunkte und die Zustaende der Home-Assistant-Entities und
// liefert Bild + Aktionen ("schalte Licht X"). Derselbe Code laeuft im PC-Test.
#include <stdint.h>
#include "gfx.h"

namespace kb {

struct Touch { int16_t x, y; };

constexpr int MAX_PAGES = 8, MAX_HINTS = 4, ID_LEN = 64;

enum PageType : uint8_t { P_NONE = 0, P_CLIMATE, P_LIGHT, P_SWITCH, P_SENSOR, P_BUTTON, P_VACUUM, PAGE_TYPES };
enum Cond : uint8_t { C_EQ = 0, C_NE, C_CONTAINS, C_LT, C_GT, CONDS };
enum Skin : uint8_t { SK_EYES = 0, SK_DROP, SK_PEBBLE, SKINS };
enum Mood : uint8_t { M_NEUTRAL = 0, M_HAPPY, M_SLEEPY, M_COLD, M_SURPRISED, MOODS };

struct PageCfg {
  uint8_t type;
  char entity[ID_LEN];
  char title[32];   // leer = Name aus Home Assistant
};
struct HintCfg {
  char entity[ID_LEN];
  uint8_t cond;
  char value[32];
  char text[40];    // was der Kumpel in der Sprechblase sagt
};
struct Config {
  uint8_t skin;
  uint8_t nPages;
  PageCfg pages[MAX_PAGES];
  char coldEntity[ID_LEN];   // Temperatur-Sensor oder Thermostat, leer = aus
  float coldBelow;
  int16_t sleepFrom, sleepTo;   // Minuten seit Mitternacht, -1 = nie schlafen
  uint8_t nHints;
  HintCfg hints[MAX_HINTS];
  uint8_t brightDay, brightNight;
  uint8_t homeAfter;            // Sekunden ohne Beruehrung -> zurueck zum Kumpel
};
void defaultConfig(Config &c);

// Zustand einer Entity, wie ihn die Home-Assistant-Verbindung liefert
struct Entity {
  char id[ID_LEN];
  bool known;
  char state[32];
  char name[48];
  char unit[12];
  float cur, target, tmin, tmax, step;   // Thermostat (NAN = unbekannt)
  char action[16];                       // hvac_action
  int brightness;                        // 0..255, -1 = unbekannt
  uint8_t rgb[3];
  bool hasRgb, hasKelvin, hasBright;
  int battery;                           // -1 = unbekannt
};

// Verbindungszustand (fuer Hinweise auf dem Startbildschirm)
enum Conn : uint8_t { CN_NO_WIFI = 0, CN_AP, CN_WIFI, CN_NO_HA, CN_HA, CN_AUTH, CN_OK };

struct Action {
  char domain[16];
  char service[24];
  char entity[ID_LEN];
  char data[96];   // JSON-Inhalt ohne Klammern, z. B.  "temperature":21.5
};

void begin(uint16_t *fb, uint32_t seed);
void setConfig(const Config &c);
const Config &config();

// Entities, die wegen der Konfiguration beobachtet werden (Seiten, Kaelte, Hinweise)
int entityCount();
Entity *entityAt(int i);
Entity *findEntity(const char *id);

// Zeilen werden auf dem Startbildschirm unter dem Kumpel gezeigt, solange conn != CN_OK
void setConn(Conn c, const char *l1 = "", const char *l2 = "", const char *l3 = "");
void setClock(int hour, int minute, int second, bool valid);
void shake();   // Lagesensor meldet Schuetteln

void tick(uint32_t nowMs, const Touch *pts, int n);

bool takeAction(Action &a);
bool takeSkinChange(uint8_t &skin);   // am Geraet umgeschaltet -> speichern
uint8_t wantBrightness();

// Hilfen fuer Tests und den Seriell-Log
struct Debug {
  int page;          // 0 = Kumpel, 1.. = Seiten
  int mood;
  bool bubble;
  bool sliding;
  float shownTarget; // angezeigte Solltemperatur (Klima-Seite)
};
Debug debug();
void hintText(int i, char *out, int n);   // Text der Sprechblase, {} = Wert der Entity
void testSetHours(float h);   // < 0 = aus
const char *moodName(int m);
const char *skinName(int s);
const char *pageTypeKey(int t);   // "climate", "light", ... (fuer JSON)
int pageTypeFromKey(const char *k);
const char *condKey(int c);
int condFromKey(const char *k);

}  // namespace kb
