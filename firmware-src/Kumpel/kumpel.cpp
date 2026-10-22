// Kumpel - Kern: Stimmungen, Figuren, Seiten-Baukasten, Gesten.
//
// Startseite: der Kumpel (Leuchtaugen, Troepfchen oder Kiesel) mit Uhrzeit und
// Sprechblase fuer Hinweise. Waagerecht wischen blaettert durch die Seiten, die
// man auf der Einstellungsseite selbst zusammenstellt (Klima, Licht, Schalter,
// Anzeige, Szene/Skript, Sauger). Lange auf den Kumpel druecken = andere Figur.
//
// Gegen Einbrennen: schwarzer Hintergrund, der Kumpel wandert langsam ein paar
// Pixel, nach einer Weile ohne Beruehrung geht es zurueck zur Startseite, nachts
// wird gedimmt.
#include "kumpel.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdarg.h>

namespace kb {

static const float PI_F = 3.14159265f;
static const int CX = SCR / 2;

// Farben
static const uint16_t K_BG = 0x0000;
static const uint16_t K_TRACK = C(0x24272d);
static const uint16_t K_DIM = C(0x9aa1ad);
static const uint16_t K_DIMMER = C(0x5c626c);
static const uint16_t K_TEXT = C(0xf2efe9);
static const uint16_t K_WARN = C(0xff8577);
static const uint16_t K_HEAT = C(0xff8a3d);
static const uint16_t K_COOL = C(0x5aa9ff);
static const uint16_t K_ON = C(0x2fc2aa);
static const uint16_t K_BUBBLE = C(0xf6f1e7);
static const uint16_t K_BUBBLE_T = C(0x2a2622);

// ------------------------------------------------------------------ Zustand
static Config cfg;
static Entity ents[MAX_PAGES + 2 + MAX_HINTS];
static int nEnts = 0;
static int pageIdx[MAX_PAGES];   // gueltige Seiten (Index in cfg.pages)
static int nShown = 0;

static Conn conn = CN_NO_WIFI;
static char info[3][64];

static uint32_t now = 0, lastTouch = 0, happyUntil = 0, surprisedUntil = 0;
static bool clockValid = false;
static float clockH = 12;          // Stunden 0..24
static uint32_t clockAt = 0;
static float testHours = -1;

static int page = 0;               // 0 = Kumpel
static int slideFrom = 0, slideTo = 0;
static uint32_t slideStart = 0;
static bool sliding = false;
static const uint32_t SLIDE_MS = 260;

static uint32_t rng = 1;
static uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static float frand(float a, float b) { return a + (b - a) * (rnd() % 10000) / 10000.0f; }

static uint32_t nextBlink = 0, blinkStart = 0;
static float gazeX = 0, gazeY = 0, gazeTX = 0, gazeTY = 0;
static uint32_t nextGaze = 0;

// Touch
enum Target { T_NONE, T_CHAR, T_ARC, T_MINUS, T_PLUS, T_POWER, T_MAIN, T_PRESET, T_VAC };
static bool down = false, moved = false, longFired = false, dragging = false;
static int sx, sy, lx, ly, tgt = T_NONE, tgtSub = 0;
static uint32_t downMs = 0;

// Ausstehende Aenderungen (sofort anzeigen, verzoegert senden)
static float pendTemp = NAN;
static uint32_t pendTempSend = 0;
static int pendBright = -1;
static uint32_t pendUntil = 0;     // bis dahin Anzeige statt HA-Wert
static uint32_t doneUntil = 0;
static int hintShown = 0;
static uint32_t hintSince = 0;

static Action queue[8];
static int qHead = 0, qLen = 0;
static bool skinChanged = false;

// ------------------------------------------------------------------ Texte
static const char *MOODS_N[] = {"neutral", "froehlich", "schlaeft", "friert", "ueberrascht", "muede"};
static const char *SKINS_N[] = {"Leuchtaugen", "Troepfchen", "Kiesel", "Marshmallow"};
static const char *PT_KEYS[] = {"", "climate", "light", "switch", "sensor", "button", "vacuum"};
static const char *COND_KEYS[] = {"eq", "ne", "contains", "lt", "gt"};
const char *moodName(int m) { return m >= 0 && m < MOODS ? MOODS_N[m] : "?"; }
const char *skinName(int s) { return s >= 0 && s < SKINS ? SKINS_N[s] : "?"; }
const char *pageTypeKey(int t) { return t > 0 && t < PAGE_TYPES ? PT_KEYS[t] : ""; }
int pageTypeFromKey(const char *k) {
  for (int i = 1; i < PAGE_TYPES; i++) if (!strcmp(k, PT_KEYS[i])) return i;
  return P_NONE;
}
const char *condKey(int c) { return c >= 0 && c < CONDS ? COND_KEYS[c] : "eq"; }
int condFromKey(const char *k) {
  for (int i = 0; i < CONDS; i++) if (!strcmp(k, COND_KEYS[i])) return i;
  return C_EQ;
}

void defaultConfig(Config &c) {
  memset(&c, 0, sizeof c);
  c.skin = SK_EYES;
  c.coldBelow = 18;
  c.sleepFrom = 23 * 60;
  c.sleepTo = 7 * 60;
  c.brightDay = 170;
  c.brightNight = 25;
  c.homeAfter = 45;
  c.hotAbove = 28;
  c.autoRotate = 1;
}

// ------------------------------------------------------------------ Entities
static void clearEntity(Entity &e, const char *id) {
  memset(&e, 0, sizeof e);
  strncpy(e.id, id, ID_LEN - 1);
  e.cur = e.target = NAN;
  e.tmin = 7; e.tmax = 30; e.step = 0.5f;
  e.brightness = -1;
  e.battery = -1;
}
static void addEntity(const char *id) {
  if (!id || !id[0] || findEntity(id)) return;
  if (nEnts >= (int)(sizeof ents / sizeof ents[0])) return;
  clearEntity(ents[nEnts++], id);
}
int entityCount() { return nEnts; }
Entity *entityAt(int i) { return i >= 0 && i < nEnts ? &ents[i] : nullptr; }
Entity *findEntity(const char *id) {
  for (int i = 0; i < nEnts; i++) if (!strcmp(ents[i].id, id)) return &ents[i];
  return nullptr;
}

void setConfig(const Config &c) {
  cfg = c;
  if (cfg.skin >= SKINS) cfg.skin = SK_EYES;
  if (cfg.nPages > MAX_PAGES) cfg.nPages = MAX_PAGES;
  if (cfg.nHints > MAX_HINTS) cfg.nHints = MAX_HINTS;
  if (cfg.homeAfter < 10) cfg.homeAfter = 10;
  cfg.rotate &= 3;
  cfg.autoRotate = cfg.autoRotate ? 1 : 0;
  // Entity-Tabelle neu aufbauen, bekannte Zustaende behalten
  Entity old[sizeof ents / sizeof ents[0]];
  int nOld = nEnts;
  memcpy(old, ents, sizeof ents);
  nEnts = 0;
  nShown = 0;
  for (int i = 0; i < cfg.nPages; i++) {
    PageCfg &p = cfg.pages[i];
    p.entity[ID_LEN - 1] = 0;
    if (p.type == P_NONE || p.type >= PAGE_TYPES || !p.entity[0]) continue;
    pageIdx[nShown++] = i;
    addEntity(p.entity);
  }
  addEntity(cfg.coldEntity);
  addEntity(cfg.weatherEntity);
  for (int i = 0; i < cfg.nHints; i++) addEntity(cfg.hints[i].entity);
  for (int i = 0; i < nEnts; i++)
    for (int j = 0; j < nOld; j++)
      if (!strcmp(ents[i].id, old[j].id)) ents[i] = old[j];
  if (page > nShown) page = 0;
  sliding = false;
}
const Config &config() { return cfg; }

void setConn(Conn c, const char *l1, const char *l2, const char *l3) {
  conn = c;
  snprintf(info[0], sizeof info[0], "%s", l1 ? l1 : "");
  snprintf(info[1], sizeof info[1], "%s", l2 ? l2 : "");
  snprintf(info[2], sizeof info[2], "%s", l3 ? l3 : "");
}

void setClock(int h, int m, int s, bool valid) {
  clockValid = valid;
  clockH = h + m / 60.0f + s / 3600.0f;
  clockAt = now;
}
static float hoursNow() {
  if (testHours >= 0) return testHours;
  float h = clockH + (now - clockAt) / 3600000.0f;
  return fmodf(h, 24.0f);
}
void testSetHours(float h) { testHours = h; clockValid = h >= 0 || clockValid; }

void shake() { surprisedUntil = now + 2200; lastTouch = now; }

static void pushAction(const char *domain, const char *service, const char *entity, const char *fmt = "", ...) {
  if (qLen >= 8) { qHead = (qHead + 1) % 8; qLen--; }   // aelteste verwerfen
  Action &a = queue[(qHead + qLen++) % 8];
  snprintf(a.domain, sizeof a.domain, "%s", domain);
  snprintf(a.service, sizeof a.service, "%s", service);
  snprintf(a.entity, sizeof a.entity, "%s", entity);
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(a.data, sizeof a.data, fmt, ap);
  va_end(ap);
}
bool takeAction(Action &a) {
  if (!qLen) return false;
  a = queue[qHead];
  qHead = (qHead + 1) % 8;
  qLen--;
  return true;
}
bool takeSkinChange(uint8_t &s) {
  if (!skinChanged) return false;
  skinChanged = false;
  s = cfg.skin;
  return true;
}

static void domainOf(const char *id, char *out, int n) {
  int i = 0;
  while (id[i] && id[i] != '.' && i < n - 1) { out[i] = id[i]; i++; }
  out[i] = 0;
}
static bool isOn(const Entity *e) {
  if (!e || !e->known) return false;
  const char *s = e->state;
  return strcmp(s, "off") && strcmp(s, "unavailable") && strcmp(s, "unknown") && strcmp(s, "closed") &&
         strcmp(s, "idle") && strcmp(s, "standby") && s[0];
}
static bool unavailable(const Entity *e) {
  return !e || !e->known || !strcmp(e->state, "unavailable");
}
static bool numeric(const char *s, float &v) {
  if (!s || !*s) return false;
  char *end;
  v = strtof(s, &end);
  while (*end == ' ') end++;
  return end != s && *end == 0;
}

// ------------------------------------------------------------------ Stimmung
static bool inSleepWindow() {
  if (cfg.sleepFrom < 0 || cfg.sleepTo < 0 || !clockValid) return false;
  int m = (int)(hoursNow() * 60);
  if (cfg.sleepFrom <= cfg.sleepTo) return m >= cfg.sleepFrom && m < cfg.sleepTo;
  return m >= cfg.sleepFrom || m < cfg.sleepTo;
}
static bool sleeping() {
  if (!inSleepWindow()) return false;
  return !(lastTouch && now - lastTouch < 25000);   // Antippen weckt ihn kurz
}
// In den 45 Minuten vor der Schlafenszeit wird er langsam muede (0 = wach .. 1 = gleich schlafen)
static float tiredness() {
  if (cfg.sleepFrom < 0 || !clockValid || inSleepWindow()) return 0;
  int m = (int)(hoursNow() * 60);
  int d = (cfg.sleepFrom - m + 1440) % 1440;
  return d > 0 && d <= 45 ? 1 - d / 45.0f : 0;
}

// Wetter aus Home Assistant (weather.*): Zustand und Aussentemperatur
static Entity *weather() {
  if (!cfg.weatherEntity[0]) return nullptr;
  Entity *e = findEntity(cfg.weatherEntity);
  return unavailable(e) ? nullptr : e;
}
static bool sunny() { Entity *e = weather(); return e && !strcmp(e->state, "sunny"); }
static bool rainy() {
  Entity *e = weather();
  if (!e) return false;
  static const char *R[] = {"rainy", "pouring", "lightning", "lightning-rainy", "hail"};
  for (const char *r : R) if (!strcmp(e->state, r)) return true;
  return false;
}
static bool hot() { Entity *e = weather(); return e && !isnan(e->target) && e->target >= cfg.hotAbove; }
static bool cold() {
  if (!cfg.coldEntity[0]) return false;
  Entity *e = findEntity(cfg.coldEntity);
  if (unavailable(e)) return false;
  float v = e->cur;                                   // Thermostat: Ist-Temperatur
  if (isnan(v) && !numeric(e->state, v)) v = e->target;  // Wetter: Aussentemperatur steckt in "temperature"
  if (isnan(v)) return false;
  return v < cfg.coldBelow;
}
static bool hintActive(int i) {
  const HintCfg &h = cfg.hints[i];
  if (!h.entity[0] || !h.text[0]) return false;
  Entity *e = findEntity(h.entity);
  if (unavailable(e)) return false;
  float a, b;
  switch (h.cond) {
    case C_EQ: return !strcasecmp(e->state, h.value);
    case C_NE: return strcasecmp(e->state, h.value) != 0;
    case C_CONTAINS: {
      char s[32], v[32];
      for (int k = 0; k < 32; k++) { s[k] = tolower((unsigned char)e->state[k]); if (!s[k]) break; }
      for (int k = 0; k < 32; k++) { v[k] = tolower((unsigned char)h.value[k]); if (!v[k]) break; }
      s[31] = v[31] = 0;
      return v[0] && strstr(s, v);
    }
    case C_LT: return numeric(e->state, a) && numeric(h.value, b) && a < b;
    case C_GT: return numeric(e->state, a) && numeric(h.value, b) && a > b;
  }
  return false;
}
static int activeHints(int *list) {
  int n = 0;
  for (int i = 0; i < cfg.nHints; i++) if (hintActive(i)) list[n++] = i;
  return n;
}
static int currentMood() {
  if (now < surprisedUntil) return M_SURPRISED;
  if (now < happyUntil) return M_HAPPY;
  if (sleeping()) return M_SLEEPY;
  if (cold()) return M_COLD;
  if (tiredness() > 0) return M_TIRED;
  return M_NEUTRAL;
}
uint8_t wantBrightness() { return sleeping() ? cfg.brightNight : cfg.brightDay; }

// ------------------------------------------------------------------ Figuren
// Alles, was das Aussehen gerade bestimmt: Stimmung plus kleine Zusaetze
struct Look {
  int mood;
  float bl;      // Blinzeln 0..1
  float cross;   // schielen 0..1
  float lid;     // muede Lider 0..1
  float yawn;    // gaehnen 0..1
  bool sad, shades, sweat;
  float talk;    // Mund bewegt sich beim Sprechen 0..1
  bool wink, tongue;
};
static uint32_t heartsUntil = 0;   // Herzaugen bis
static Look look = {M_NEUTRAL, 0, 0, 0, 0, false, false, false, 0, false, false};
static const uint16_t K_TONGUE = C(0xff7a93);   // pro Frame in tick() gesetzt

static float blinkAmount() {
  if (now < blinkStart || now > blinkStart + 170) return 0;
  return sinf((now - blinkStart) / 170.0f * PI_F);
}

static void drawZ(float cx, float cy, float s) {
  for (int k = 0; k < 3; k++) {
    float p = fmodf(now / 2600.0f + k / 3.0f, 1.0f);
    gAlpha(p < 0.15f ? p / 0.15f : (1 - p) / 0.85f);
    gText(F_MED, (int)(cx + (70 + 26 * p) * s), (int)(cy - (40 + 90 * p) * s), "z", K_DIM, CENTER);
  }
  gAlpha(1);
}

static void shiverLines(float cx, float cy, float w, float s, uint16_t c) {
  for (int side = -1; side <= 1; side += 2) {
    float x = cx + side * w, t = 3 * s;
    gCapsule(x, cy - 16 * s, x + side * 7 * s, cy - 5 * s, t, c);
    gCapsule(x + side * 7 * s, cy - 5 * s, x, cy + 6 * s, t, c);
    gCapsule(x, cy + 6 * s, x + side * 7 * s, cy + 17 * s, t, c);
  }
}

// Schweisstropfen, der langsam an der Seite herunterlaeuft
static void sweatDrop(float x, float y, float s) {
  float p = fmodf(now / 1800.0f, 1.0f);
  gAlpha(p < 0.8f ? 1 : (1 - p) * 5);
  gDrop(x, y + p * 26 * s, 0.14f * s, C(0x7cc7ff));
  gAlpha(1);
}

// ein Brillenglas mit Rahmen und kleinem Glanzstrich
static void lens(float x, float y, float w, float h, float s, uint16_t frame) {
  gRRect(x - w / 2 - 3 * s, y - h / 2 - 3 * s, w + 6 * s, h + 6 * s, 11 * s, frame);
  gRRect(x - w / 2, y - h / 2, w, h, 9 * s, C(0x24272e));
  gCapsule(x - w * 0.28f, y - h * 0.05f, x - w * 0.08f, y - h * 0.3f, 2.4f * s, C(0x7a8290));
}

static bool calm(int mood) { return mood == M_NEUTRAL || mood == M_TIRED; }

// Leuchtaugen: nur zwei Augen, Stimmung ueber Form und Farbe
static void skinEyes(float cx, float cy, float s, const Look &L) {
  s *= 1.2f;   // Augen duerfen etwas groesser sein als die Figuren
  int mood = L.mood;
  uint16_t col = mood == M_COLD ? C(0x8ab4ff) : C(0x5ee6d0);
  float gap = 62 * s, gx = gazeX * 9 * s, gy = gazeY * 8 * s;
  bool yawning = L.yawn > 0.05f && calm(mood);
  bool shades = L.shades && (mood == M_NEUTRAL || mood == M_HAPPY);
  if (mood == M_COLD) cx += sinf(now / 25.0f) * 2.2f * s;
  float mx = cx + gx * 0.6f, my = cy + gy * 0.6f + 60 * s;   // Mund
  cy -= 10 * s;                                            // Augen etwas hoeher, Platz fuer den Mund
  for (int side = -1; side <= 1; side += 2) {
    float x = cx + side * gap + gx - side * L.cross * gap * 0.42f, y = cy + gy;
    if (shades) break;   // hinter der Sonnenbrille sieht man die Augen nicht
    if (yawning) { gCapsule(x - 25 * s, y + 6 * s, x + 25 * s, y + 6 * s, 8 * s, col); continue; }
    if (L.wink && side == 1 && calm(mood)) { gArc(x, y + 16 * s, 28 * s, 17 * s, 200, 340, col); continue; }   // zwinkern
    switch (mood) {
      case M_HAPPY: gArc(x, y + 16 * s, 28 * s, 17 * s, 200, 340, col); break;
      case M_SLEEPY: gCapsule(x - 26 * s, y + 8 * s, x + 26 * s, y + 8 * s, 7 * s, col); break;
      case M_SURPRISED: gCircle(x, y, 33 * s, col); break;
      case M_COLD: {
        float hl = 15 * s * (1 - L.bl) + 0.5f;
        gCapsule(x, y - hl, x, y + hl, 17 * s, col);
        break;
      }
      default: {
        float hl = 30 * s * (1 - L.bl), r = 25 * s * (1 - 0.6f * L.bl);
        gCapsule(x, y - hl, x, y + hl, r, col);
        float top = y - hl - r - 3, h = 2 * (hl + r) + 6;
        // Lider sind einfach schwarze Flaechen - der Hintergrund ist schwarz
        if (L.lid > 0.01f) gRRect(x - r - 4, top - 2, 2 * r + 8, h * 0.55f * L.lid + 2, 0, K_BG);
        if (L.sad) gTriangle(x + side * (r + 5), top - 2, x - side * (r + 5), top - 2, x + side * (r + 5), top + h * 0.32f, K_BG);
      }
    }
  }
  if (shades) {
    for (int side = -1; side <= 1; side += 2) lens(cx + side * gap + gx, cy + gy, 72 * s, 56 * s, s, col);
    gCapsule(cx - gap + gx + 39 * s, cy + gy - 10 * s, cx + gap + gx - 39 * s, cy + gy - 10 * s, 3.2f * s, col);
  }
  // ---- Mund: zu jeder Stimmung ein eigener
  float t = 5 * s;   // Strichstaerke
  if (yawning) {
    float rx = (8 + 9 * L.yawn) * s, ry = (6 + 13 * L.yawn) * s;
    gEllipse(mx, my, rx, ry, col);
    gEllipse(mx, my, rx - t, ry - t, K_BG);
  } else if (L.talk > 0.05f && mood != M_SLEEPY) {           // sprechen: Mund klappt auf und zu
    float ry = (3 + 10 * L.talk) * s;
    gEllipse(mx, my, 12 * s, ry, col);
    if (ry > t + 1) gEllipse(mx, my, 12 * s - t, ry - t, K_BG);
  } else if (L.tongue && calm(mood)) {                       // Zunge rausstrecken
    gEllipse(mx + 5 * s, my + 9 * s, 8 * s, 10 * s, K_TONGUE);
    gArc(mx, my - 14 * s, 18 * s, t, 40, 140, col);
  } else if (shades) {
    gArc(mx + 6 * s, my - 14 * s, 20 * s, t, 25, 115, col);  // laessiges Grinsen
  } else switch (mood) {
    case M_HAPPY:                                            // offenes Lachen (halber Kreis) mit Zunge
      gCircle(mx, my - 6 * s, 20 * s, col);
      gRRect(mx - 22 * s, my - 30 * s, 44 * s, 24 * s, 0, K_BG);
      gEllipse(mx, my + 8 * s, 8 * s, 4 * s, K_TONGUE);
      break;
    case M_SLEEPY: gCircle(mx, my, (4 + 1.5f * sinf(now / 800.0f)) * s, mix565(0, col, 0.7f)); break;
    case M_SURPRISED: gRing(mx, my, 11 * s, t, col); break;
    case M_COLD:
      for (int k = 0; k < 4; k++) {
        float x0 = mx - 16 * s + k * 8 * s;
        gCapsule(x0, my + (k % 2 ? -3 : 3) * s, x0 + 8 * s, my + (k % 2 ? 3 : -3) * s, 2.4f * s, col);
      }
      break;
    case M_TIRED: gCapsule(mx - 12 * s, my, mx + 12 * s, my + 2 * s, 2.6f * s, col); break;
    default:
      if (L.sad) gArc(mx, my + 14 * s, 16 * s, t, 225, 315, col);   // bedrueckt
      else gArc(mx, my - 12 * s, 16 * s, t, 55, 125, col);         // kleines Laecheln
  }
  if (L.sweat && mood != M_SLEEPY) sweatDrop(cx + gap + gx + 44 * s, cy - 48 * s, s);
  if (mood == M_COLD) shiverLines(cx, cy, 118 * s, s, col);
  if (mood == M_SLEEPY && s > 0.6f) drawZ(cx, cy, s);
}

// Gesicht fuer Troepfchen und Kiesel (dunkle Augen, Baeckchen, Mund)
static void face(float cx, float cy, float s, const Look &L, uint16_t ink, uint16_t cheek, uint16_t body) {
  int mood = L.mood;
  float ex = 23 * s, gx = gazeX * 5 * s, gy = gazeY * 4 * s;
  bool yawning = L.yawn > 0.05f && calm(mood);
  for (int side = -1; side <= 1; side += 2) {
    float x = cx + side * ex + gx - side * L.cross * ex * 0.55f, y = cy + gy;
    if (mood == M_SLEEPY || yawning) gArc(x, y - 3 * s, 8 * s, 3.6f * s, 25, 155, ink);
    else if (mood == M_HAPPY || (L.wink && side == 1 && calm(mood))) gArc(x, y + 5 * s, 8 * s, 3.8f * s, 205, 335, ink);
    else if (L.bl > 0.5f) gCapsule(x - 7 * s, y, x + 7 * s, y, 2.6f * s, ink);
    else {
      float r = (mood == M_SURPRISED ? 9.5f : 7.5f) * s;
      gCircle(x, y, r, ink);
      if (L.lid > 0.01f) gRRect(x - r - 2, y - r - 2, 2 * r + 4, (2 * r + 3) * 0.55f * L.lid + 1, 0, body);
    }
    if (L.sad && calm(mood)) gCapsule(x - side * 7 * s, y - 16 * s, x + side * 6 * s, y - 11 * s, 2 * s, ink);
    gCircle(cx + side * 42 * s, cy + 15 * s, 8.5f * s, cheek);
  }
  float my = cy + 17 * s + gy;
  if (yawning) gEllipse(cx + gx, my + 3 * s, (4 + 5 * L.yawn) * s, (4 + 8 * L.yawn) * s, ink);
  else if (L.talk > 0.05f && mood != M_SLEEPY) gEllipse(cx + gx, my + 1 * s, 6 * s, (2 + 6 * L.talk) * s, ink);
  else if (L.tongue && calm(mood)) {
    gEllipse(cx + gx + 3 * s, my + 4 * s, 4.5f * s, 6 * s, K_TONGUE);
    gArc(cx + gx, my - 6 * s, 10 * s, 3.8f * s, 40, 140, ink);
  } else switch (mood) {
    case M_HAPPY: gArc(cx + gx, my - 4 * s, 13 * s, 4.5f * s, 20, 160, ink); break;
    case M_SLEEPY: gCircle(cx + gx, my + 2 * s, 4 * s, ink); break;
    case M_SURPRISED: gRing(cx + gx, my + 3 * s, 7 * s, 4 * s, ink); break;
    case M_COLD:
      for (int k = 0; k < 4; k++) {
        float x0 = cx - 12 * s + k * 6 * s;
        gCapsule(x0, my + (k % 2 ? -2 : 2) * s, x0 + 6 * s, my + (k % 2 ? 2 : -2) * s, 1.8f * s, ink);
      }
      break;
    default:
      if (L.sad) gArc(cx + gx, my + 9 * s, 9 * s, 3.6f * s, 220, 320, ink);
      else gArc(cx + gx, my - 6 * s, 10 * s, 3.8f * s, 40, 140, ink);
  }
  if (L.shades && (mood == M_NEUTRAL || mood == M_HAPPY)) {
    for (int side = -1; side <= 1; side += 2) lens(cx + side * ex + gx, cy + gy, 28 * s, 19 * s, s * 0.5f, ink);
    gCapsule(cx - ex + gx + 15 * s, cy + gy - 3 * s, cx + ex + gx - 15 * s, cy + gy - 3 * s, 1.8f * s, ink);
  }
}

static void skinDrop(float cx, float cy, float s, const Look &L) {
  int mood = L.mood;
  float bob = sinf(now / 900.0f) * 4 * s;
  if (mood == M_SLEEPY || mood == M_TIRED) bob *= 0.4f;
  if (mood == M_COLD) cx += sinf(now / 25.0f) * 2 * s;
  float y = cy + bob - 8 * s;
  uint16_t body = mood == M_COLD ? C(0xa8dcf2) : C(0x3fa9d6);
  gDrop(cx, y, s, body);
  gEllipse(cx - 30 * s, y - 5 * s, 9 * s, 17 * s, mix565(body, 0xFFFF, 0.45f));   // Glanzlicht
  face(cx, y + 28 * s, s, L, C(0x0b2a3a), C(0xff9fb0), body);
  if (L.sweat && mood != M_SLEEPY) sweatDrop(cx + 78 * s, y - 2 * s, s);
  if (mood == M_COLD) shiverLines(cx, y + 20 * s, 92 * s, s, body);
  if (mood == M_SLEEPY && s > 0.6f) drawZ(cx, cy, s);
}

static void skinPebble(float cx, float cy, float s, const Look &L) {
  int mood = L.mood;
  float br = mood == M_SLEEPY ? sinf(now / 1600.0f) * 0.035f : sinf(now / 1100.0f) * 0.018f;
  if (mood == M_COLD) cx += sinf(now / 25.0f) * 2 * s;
  uint16_t body = mood == M_COLD ? C(0xc9d6e0) : C(0xe8d3a8);
  float rx = 88 * s * (1 - br * 0.5f), ry = 64 * s * (1 + br);
  float y = cy + 10 * s - (ry - 64 * s);
  gEllipse(cx, y + 4 * s, rx, ry, mix565(body, 0, 0.25f));   // Schatten unten
  gEllipse(cx, y, rx, ry, body);
  gEllipse(cx - 34 * s, y - 30 * s, 20 * s, 9 * s, mix565(body, 0xFFFF, 0.5f));
  face(cx, y - 2 * s, s, L, C(0x4a3b22), C(0xf2a08c), body);
  if (L.sweat && mood != M_SLEEPY) sweatDrop(cx + 96 * s, y - 34 * s, s);
  if (mood == M_COLD) shiverLines(cx, y, 108 * s, s, body);
  if (mood == M_SLEEPY && s > 0.6f) drawZ(cx, cy, s);
}

// ---- Marshmallow: weiches, knautschiges Kissen mit Kulleraugen
static void heart(float x, float y, float r, uint16_t c) {
  gCircle(x - r * 0.48f, y - r * 0.2f, r * 0.56f, c);
  gCircle(x + r * 0.48f, y - r * 0.2f, r * 0.56f, c);
  gTriangle(x - r * 1.02f, y - r * 0.02f, x + r * 1.02f, y - r * 0.02f, x, y + r * 1.02f, c);
}
static void sparkle(float x, float y, float r, uint16_t c) {   // vierzackiger Glitzerstern
  float w = r * 0.24f;
  gTriangle(x - w, y, x + w, y, x, y - r, c);
  gTriangle(x - w, y, x + w, y, x, y + r, c);
  gTriangle(x, y - w, x, y + w, x - r, y, c);
  gTriangle(x, y - w, x, y + w, x + r, y, c);
}
static bool heartsNow() { return now < heartsUntil && now < happyUntil; }

static void skinSoft(float cx, float cy, float s, const Look &L) {
  s *= 1.32f;   // Marshmallow darf das Display gut ausfuellen
  int mood = L.mood;
  bool cold = mood == M_COLD;
  // atmen = leicht breiter/flacher und wieder zurueck (wie ein weiches Kissen)
  float br = sinf(now / (mood == M_SLEEPY ? 1500.0f : 1050.0f)) * (mood == M_SLEEPY ? 0.04f : 0.022f);
  if (cold) cx += sinf(now / 25.0f) * 2 * s;
  float W = 176 * s * (1 + br), H = 132 * s * (1 - br), R = 58 * s;
  float bottom = cy + 58 * s;
  float x0 = cx - W / 2, y0 = bottom - H;
  uint16_t body = cold ? C(0xd6e6f5) : C(0xe4f1ec);
  uint16_t glow = cold ? C(0x5a8fd0) : C(0x2fa58a);
  // weicher Schein dahinter und Schatten darunter
  for (int k = 8; k >= 1; k--) {   // viele flache Stufen = weicher Uebergang
    gAlpha(0.022f);
    gRRect(x0 - k * 6 * s, y0 - k * 6 * s, W + k * 12 * s, H + k * 12 * s, R + k * 6 * s, glow);
  }
  gAlpha(0.35f);
  gEllipse(cx, bottom + 14 * s, W * 0.36f, 7 * s, C(0x0e2a24));
  gAlpha(1);
  // Koerper mit sanfter Schattierung: dunklerer Rand, heller Kern, Glanz oben links
  gRRect(x0, y0, W, H, R, mix565(body, 0, 0.22f));
  gRRect(x0 + 3 * s, y0 + 2 * s, W - 6 * s, H - 7 * s, R - 3 * s, mix565(body, 0, 0.08f));
  gRRect(x0 + 10 * s, y0 + 6 * s, W - 20 * s, H - 22 * s, R - 10 * s, body);
  gAlpha(0.55f);
  gEllipse(x0 + W * 0.27f, y0 + H * 0.2f, 26 * s, 11 * s, 0xFFFF);
  gAlpha(1);

  uint16_t ink = C(0x1d1f22), blush = C(0xf7a6b3);
  float ex = 34 * s, ey = y0 + H * 0.5f, gx = gazeX * 6 * s, gy = gazeY * 5 * s;
  bool yawning = L.yawn > 0.05f && calm(mood);
  bool shades = L.shades && (mood == M_NEUTRAL || mood == M_HAPPY);
  bool hearts = mood == M_HAPPY && heartsNow();
  // Baeckchen
  gAlpha(hearts ? 0.85f : 0.6f);
  for (int side = -1; side <= 1; side += 2) gEllipse(cx + side * 50 * s + gx * 0.5f, ey + 16 * s, 14 * s, 7.5f * s, blush);
  gAlpha(1);
  // Augen
  for (int side = -1; side <= 1; side += 2) {
    float x = cx + side * ex + gx - side * L.cross * ex * 0.5f, y = ey + gy;
    if (shades) break;
    if (hearts) heart(x, y - 2 * s, 13 * s, C(0xff5a73));
    else if (mood == M_HAPPY || (L.wink && side == 1 && calm(mood))) gArc(x, y + 7 * s, 11 * s, 4.6f * s, 205, 335, ink);
    else if (mood == M_SLEEPY || yawning) gArc(x, y - 4 * s, 11 * s, 4.2f * s, 25, 155, ink);
    else if (L.bl > 0.5f) gCapsule(x - 9 * s, y, x + 9 * s, y, 3 * s, ink);
    else {
      float r = (mood == M_SURPRISED ? 11.5f : 8.5f) * s, hl = (mood == M_SURPRISED ? 2 : 4) * s;
      gCapsule(x, y - hl, x, y + hl, r, ink);
      gCircle(x - r * 0.35f, y - hl - r * 0.25f, r * 0.32f, 0xFFFF);   // Lichtpunkt im Auge
      if (L.lid > 0.01f) gRRect(x - r - 2, y - hl - r - 2, 2 * r + 4, (2 * (hl + r) + 3) * 0.55f * L.lid + 1, 0, body);
    }
    if (L.sad && calm(mood)) gCapsule(x - side * 9 * s, y - 19 * s, x + side * 7 * s, y - 14 * s, 2.2f * s, ink);
  }
  if (shades) {
    for (int side = -1; side <= 1; side += 2) lens(cx + side * ex + gx, ey + gy, 40 * s, 26 * s, s * 0.6f, ink);
    gCapsule(cx - ex + gx + 21 * s, ey + gy - 4 * s, cx + ex + gx - 21 * s, ey + gy - 4 * s, 2.2f * s, ink);
  }
  // Mund nur, wenn er etwas tut (sonst ganz schlicht, ohne Mund)
  float mx = cx + gx * 0.7f, my = ey + 22 * s + gy;
  if (yawning) gEllipse(mx, my + 2 * s, (5 + 6 * L.yawn) * s, (4 + 9 * L.yawn) * s, ink);
  else if (L.talk > 0.05f && mood != M_SLEEPY) gEllipse(mx, my, 7 * s, (2 + 6 * L.talk) * s, ink);
  else if (L.tongue && calm(mood)) {
    gEllipse(mx + 3 * s, my + 3 * s, 5 * s, 6.5f * s, K_TONGUE);
    gArc(mx, my - 8 * s, 10 * s, 3.4f * s, 40, 140, ink);
  } else if (mood == M_SURPRISED) gRing(mx, my + 2 * s, 6 * s, 3.4f * s, ink);
  else if (cold) {
    for (int k = 0; k < 4; k++) {
      float xk = mx - 12 * s + k * 6 * s;
      gCapsule(xk, my + (k % 2 ? -2 : 2) * s, xk + 6 * s, my + (k % 2 ? 2 : -2) * s, 1.8f * s, ink);
    }
  } else if (shades) gArc(mx + 4 * s, my - 9 * s, 12 * s, 3.4f * s, 25, 115, ink);
  else if (L.sad && calm(mood)) gArc(mx, my + 9 * s, 9 * s, 3.2f * s, 225, 315, ink);
  // Glitzer, wenn er sich freut
  if (mood == M_HAPPY && s > 0.6f) {
    uint16_t sp = C(0xd8e2e8);
    for (int k = 0; k < 3; k++) {
      float p = fmodf(now / 700.0f + k * 0.33f, 1.0f);
      gAlpha(sinf(p * PI_F));
      static const float SX[3] = {-36, -6, 20}, SY[3] = {-40, -66, -48}, SR[3] = {13, 17, 11};
      sparkle(cx + SX[k] * s + 30 * s, y0 + SY[k] * s, SR[k] * s * (0.7f + 0.3f * sinf(p * PI_F)), sp);
    }
    gAlpha(1);
  }
  if (L.sweat && mood != M_SLEEPY) sweatDrop(x0 + W - 22 * s, y0 + 10 * s, s);
  if (cold) shiverLines(cx, ey, W / 2 + 16 * s, s, body);
  if (mood == M_SLEEPY && s > 0.6f) drawZ(cx, y0 + 30 * s, s);
}

static void drawSkin(float cx, float cy, float s, int mood) {
  Look L = look;
  L.mood = mood;
  L.bl = (mood == M_NEUTRAL || mood == M_COLD || mood == M_TIRED) ? blinkAmount() : 0;
  switch (cfg.skin) {
    case SK_DROP: skinDrop(cx, cy, s, L); break;
    case SK_PEBBLE: skinPebble(cx, cy, s, L); break;
    case SK_SOFT: skinSoft(cx, cy, s, L); break;
    default: skinEyes(cx, cy, s, L);
  }
}

// kleines Regenwoelkchen ueber dem Kumpel
static void rainCloud(float x, float y) {
  for (int k = 0; k < 5; k++) {
    float p = fmodf(now / 900.0f + k * 0.37f, 1.0f);
    float dx = x - 26 + k * 13;
    gAlpha(p < 0.8f ? 1 : (1 - p) * 5);
    gCapsule(dx, y + 14 + p * 34, dx - 2, y + 22 + p * 34, 2.2f, C(0x6fb6ff));
  }
  gAlpha(1);
  uint16_t c = C(0x8a929e);
  gCircle(x - 20, y + 2, 16, c);
  gCircle(x + 2, y - 8, 21, c);
  gCircle(x + 24, y + 3, 15, c);
  gRRect(x - 36, y + 2, 76, 17, 8, c);
}

// ------------------------------------------------------------------ Bausteine
static void dots(int active) {
  int n = nShown + 1;
  if (n < 2) return;
  float w = 16, x0 = CX - (n - 1) * w / 2;
  for (int i = 0; i < n; i++) gCircle(x0 + i * w, 447, i == active ? 4.5f : 3.2f, i == active ? K_TEXT : K_DIMMER);
}

static const char *titleOf(const PageCfg &p, const Entity *e) {
  if (p.title[0]) return p.title;
  if (e && e->name[0]) return e->name;
  return p.entity;
}

static void fmtTemp(float v, char *out, int n, bool deg = true) {
  snprintf(out, n, "%.1f%s", v, deg ? "\xC2\xB0" : "");
  for (char *p = out; *p; p++) if (*p == '.') *p = ',';
}

static const char *translate(const char *s) {
  static const char *map[][2] = {
    {"on", "An"}, {"off", "Aus"}, {"unavailable", "Nicht verf\xC3\xBCgbar"}, {"unknown", "Unbekannt"},
    {"home", "Zuhause"}, {"not_home", "Unterwegs"}, {"open", "Offen"}, {"closed", "Geschlossen"},
    {"locked", "Verriegelt"}, {"unlocked", "Entriegelt"}, {"cleaning", "Saugt"}, {"docked", "In der Station"},
    {"returning", "F\xC3\xA4hrt zur\xC3\xBC" "ck"}, {"idle", "Bereit"}, {"paused", "Pausiert"}, {"error", "Fehler"},
    {"heat", "Heizen"}, {"cool", "K\xC3\xBChlen"}, {"auto", "Automatik"}, {"playing", "Spielt"},
  };
  for (auto &m : map) if (!strcmp(s, m[0])) return m[1];
  return s;
}

static void miniKumpel(int mood) { drawSkin(CX, 396, 0.3f, mood); }

static void pageHeader(const PageCfg &p, const Entity *e) {
  char t[64];
  gFit(F_SMALL, titleOf(p, e), t, sizeof t, 210);
  gText(F_SMALL, CX, 92, t, K_DIM, CENTER);
  if (conn != CN_OK) gCircle(CX, 80, 4, K_WARN);
}

// Bogen fuer Regler: 135 Grad (unten links) im Uhrzeigersinn bis 405 Grad (unten rechts)
static const float ARC_R = 178, ARC_T = 24, ARC_A0 = 135, ARC_SWEEP = 270;
static float arcFraction(int x, int y, bool clampGap) {
  float ang = atan2f((float)(y - CX), (float)(x - CX)) * 180 / PI_F;
  float rel = fmodf(ang - ARC_A0 + 720.0f, 360.0f);
  if (rel > ARC_SWEEP) {
    if (!clampGap) return -1;
    rel = rel - ARC_SWEEP < (360 - rel) ? ARC_SWEEP : 0;
  }
  return rel / ARC_SWEEP;
}
static bool onArc(int x, int y) {
  float dx = x - CX, dy = y - CX, d = sqrtf(dx * dx + dy * dy);
  return d > ARC_R - 28 && d < ARC_R + 32 && arcFraction(x, y, false) >= 0;
}
static void arc(float frac, uint16_t col, bool knob) {
  gArc(CX, CX, ARC_R, ARC_T, ARC_A0, ARC_A0 + ARC_SWEEP, K_TRACK);
  if (frac < 0) return;
  frac = frac > 1 ? 1 : frac;
  float a1 = ARC_A0 + ARC_SWEEP * frac;
  if (frac > 0.002f) gArc(CX, CX, ARC_R, ARC_T, ARC_A0, a1, col);
  if (knob) {
    float r = a1 * PI_F / 180;
    gCircle(CX + ARC_R * cosf(r), CX + ARC_R * sinf(r), 15, K_TEXT);
  }
}

static void minusPlus(float x, float y, bool plus) {
  gCircle(x, y, 31, K_TRACK);
  gCapsule(x - 11, y, x + 11, y, 3, K_TEXT);
  if (plus) gCapsule(x, y - 11, x, y + 11, 3, K_TEXT);
}
static void powerIcon(float x, float y, bool on) {
  gCircle(x, y, 23, on ? K_TRACK : C(0x3a3f47));
  uint16_t c = on ? K_TEXT : K_DIM;
  gArc(x, y + 1, 10, 3.4f, 305, 595, c);
  gCapsule(x, y - 12, x, y - 2, 1.8f, c);
}

// ------------------------------------------------------------------ Seiten
static const float BTN_Y = 322;

static float climateShown(const Entity *e) {
  if (!isnan(pendTemp)) return pendTemp;
  return e ? e->target : NAN;
}

static void pageClimate(const PageCfg &p, Entity *e) {
  bool off = e && e->known && !strcmp(e->state, "off");
  float shown = climateShown(e);
  float span = e ? e->tmax - e->tmin : 1;
  uint16_t col = K_ON;
  if (e && !strcmp(e->action, "heating")) col = K_HEAT;
  else if (e && !strcmp(e->action, "cooling")) col = K_COOL;
  arc(off || isnan(shown) || span <= 0 ? -1 : (shown - e->tmin) / span, col, !off && !isnan(shown));
  pageHeader(p, e);
  char buf[24];
  if (unavailable(e)) {
    gText(F_MED, CX, 196, e && e->known ? "Nicht verf\xC3\xBCgbar" : "\xE2\x80\xA6", K_DIM, CENTER);
  } else if (off) {
    gText(F_MED, CX, 186, "Aus", K_DIM, CENTER);
  } else if (!isnan(shown)) {
    fmtTemp(shown, buf, sizeof buf);
    gText(F_BIG, CX + 10, 132, buf, K_TEXT, CENTER);
  }
  if (e && !isnan(e->cur)) {
    char t[32];
    fmtTemp(e->cur, buf, sizeof buf);
    snprintf(t, sizeof t, "Ist %s", buf);
    gText(F_SMALL, CX, 252, t, K_DIM, CENTER);
  }
  if (!off) { minusPlus(CX - 82, BTN_Y, false); minusPlus(CX + 82, BTN_Y, true); }
  powerIcon(CX, BTN_Y + 4, !off);
}

static uint16_t lightColor(const Entity *e) {
  int r = 255, g = 196, b = 120;
  if (e && e->hasRgb) { r = e->rgb[0]; g = e->rgb[1]; b = e->rgb[2]; }
  return rgb565(r, g, b);
}
static const struct { bool kelvin; int k, r, g, b; } PRESETS[] = {
  {true, 2700, 255, 183, 107}, {true, 4600, 255, 236, 214}, {false, 0, 255, 120, 40}, {false, 0, 255, 80, 160}, {false, 0, 70, 120, 255},
};
static int presetList(const Entity *e, int *out) {
  int n = 0;
  for (int i = 0; i < 5; i++)
    if ((PRESETS[i].kelvin && e->hasKelvin) || (!PRESETS[i].kelvin && e->hasRgb)) out[n++] = i;
  return n;
}
static float presetX(int i, int n) { return CX + (i - (n - 1) / 2.0f) * 40; }

static void pageLight(const PageCfg &p, Entity *e, bool isLight) {
  bool on = isOn(e);
  int bright = (pendBright >= 0 && now < pendUntil) ? pendBright : (e ? e->brightness : -1);
  uint16_t col = isLight ? lightColor(e) : K_ON;
  bool arcShown = isLight && e && e->hasBright;
  if (arcShown) arc(on && bright >= 0 ? bright / 255.0f : -1, col, on && bright >= 0);
  pageHeader(p, e);
  float cy = isLight ? 214 : 232, r = isLight ? 86 : 96;
  if (unavailable(e)) {
    gRing(CX, cy, r, 4, K_TRACK);
    gText(F_SMALL, CX, (int)cy - 14, e && e->known ? "Nicht verf\xC3\xBCgbar" : "\xE2\x80\xA6", K_DIM, CENTER);
  } else if (on) {
    float k = bright >= 0 ? 0.45f + 0.55f * bright / 255.0f : 1;
    uint16_t fill = mix565(0, col, k);
    gCircle(CX, cy, r + 10, mix565(0, col, 0.18f));   // weicher Schein
    gCircle(CX, cy, r, fill);
    char t[16];
    if (isLight && bright >= 0) snprintf(t, sizeof t, "%d %%", (bright * 100 + 127) / 255);
    else snprintf(t, sizeof t, "An");
    gText(F_MED, CX, (int)cy - 21, t, C(0x15161a), CENTER);
  } else {
    gRing(CX, cy, r, 5, K_TRACK);
    gText(F_MED, CX, (int)cy - 21, "Aus", K_DIM, CENTER);
  }
  if (isLight && e && e->known) {
    int list[5], n = presetList(e, list);
    for (int i = 0; i < n; i++) {
      auto &pr = PRESETS[list[i]];
      gCircle(presetX(i, n), 338, 14, rgb565(pr.r, pr.g, pr.b));
    }
  }
}

static void pageSensor(const PageCfg &p, Entity *e) {
  pageHeader(p, e);
  if (unavailable(e)) {
    gText(F_MED, CX, 196, e && e->known ? "Nicht verf\xC3\xBCgbar" : "\xE2\x80\xA6", K_DIM, CENTER);
    return;
  }
  float v;
  char buf[40];
  if (numeric(e->state, v)) {
    if (fabsf(v) >= 100 || fabsf(v - roundf(v)) < 0.05f) snprintf(buf, sizeof buf, "%.0f", v);
    else snprintf(buf, sizeof buf, "%.1f", v);
    for (char *q = buf; *q; q++) if (*q == '.') *q = ',';
    bool deg = !strcmp(e->unit, "\xC2\xB0" "C") || !strcmp(e->unit, "\xC2\xB0" "F");
    bool pct = !strcmp(e->unit, "%");
    if (deg) strcat(buf, "\xC2\xB0");
    if (pct) strcat(buf, "%");
    if (gTextW(F_BIG, buf) < 330) gText(F_BIG, CX, 136, buf, K_TEXT, CENTER);
    else gText(F_MED, CX, 186, buf, K_TEXT, CENTER);
    if (!pct && !deg && e->unit[0]) gText(F_MED, CX, 270, e->unit, K_DIM, CENTER);
  } else {
    char t[64];
    gFit(F_MED, translate(e->state), t, sizeof t, 320);
    gText(F_MED, CX, 196, t, K_TEXT, CENTER);
  }
}

static void pageButton(const PageCfg &p, Entity *e) {
  pageHeader(p, e);
  bool done = now < doneUntil;
  float pulse = done ? 1 - (doneUntil - now) / 1500.0f : 0;
  if (done) gRing(CX, 222, 92 + 40 * pulse, 6 * (1 - pulse) + 0.5f, mix565(0, K_ON, 1 - pulse));
  gCircle(CX, 222, 86, done ? K_ON : K_TRACK);
  if (done) {
    gCapsule(CX - 30, 224, CX - 8, 246, 7, C(0x0c1a17));
    gCapsule(CX - 8, 246, CX + 34, 200, 7, C(0x0c1a17));
  } else {
    gTriangle(CX - 22, 186, CX - 22, 258, CX + 38, 222, unavailable(e) ? K_DIMMER : K_TEXT);
  }
  if (unavailable(e) && !(e && !e->known && conn == CN_OK))
    gText(F_SMALL, CX, 322, e && e->known ? "Nicht verf\xC3\xBCgbar" : "", K_DIM, CENTER);
}

static const float VAC_X[3] = {CX - 92, CX, CX + 92};
static void pageVacuum(const PageCfg &p, Entity *e) {
  pageHeader(p, e);
  char t[48];
  if (unavailable(e)) {
    gText(F_MED, CX, 170, e && e->known ? "Nicht verf\xC3\xBCgbar" : "\xE2\x80\xA6", K_DIM, CENTER);
  } else {
    gFit(F_MED, translate(e->state), t, sizeof t, 320);
    gText(F_MED, CX, 150, t, K_TEXT, CENTER);
    if (e->battery >= 0) {
      snprintf(t, sizeof t, "Akku %d %%", e->battery);
      gText(F_SMALL, CX, 200, t, K_DIM, CENTER);
    }
  }
  bool cleaning = e && !strcmp(e->state, "cleaning");
  for (int i = 0; i < 3; i++) {
    float x = VAC_X[i], y = 290;
    gCircle(x, y, 36, i == 0 && cleaning ? K_ON : K_TRACK);
    if (i == 0) gTriangle(x - 10, y - 15, x - 10, y + 15, x + 16, y, K_TEXT);
    if (i == 1) { gCapsule(x - 7, y - 13, x - 7, y + 13, 3.6f, K_TEXT); gCapsule(x + 7, y - 13, x + 7, y + 13, 3.6f, K_TEXT); }
    if (i == 2) {   // Haus = Station
      gTriangle(x - 17, y - 2, x + 17, y - 2, x, y - 17, K_TEXT);
      gRRect(x - 12, y - 4, 24, 19, 2, K_TEXT);
    }
  }
  const char *lab[3] = {"Start", "Pause", "Station"};
  for (int i = 0; i < 3; i++) gText(F_SMALL, (int)VAC_X[i], 332, lab[i], K_DIM, CENTER);
}

// ------------------------------------------------------------------ Startseite
// Hinweistext mit Platzhalter: "{}" wird durch den aktuellen Zustand der Entity ersetzt
void hintText(int i, char *out, int n) {
  const HintCfg &h = cfg.hints[i];
  Entity *e = findEntity(h.entity);
  const char *val = !e || !e->known ? "" : (!strncmp(e->id, "calendar.", 9) && e->extra[0]) ? e->extra : translate(e->state);
  const char *p = strstr(h.text, "{}");
  if (!p) { snprintf(out, n, "%s", h.text); return; }
  snprintf(out, n, "%.*s%s%s", (int)(p - h.text), h.text, val, p + 2);
}

static void drawBubble(const char *text) {
  char t[64];
  gFit(F_SMALL, text, t, sizeof t, 282);
  int w = gTextW(F_SMALL, t) + 40;
  if (w < 120) w = 120;
  float x = CX - w / 2.0f, y = 62, h = 50;
  gRRect(x, y, w, h, 22, K_BUBBLE);
  gTriangle(CX - 11, y + h - 2, CX + 11, y + h - 2, CX, y + h + 13, K_BUBBLE);
  gText(F_SMALL, CX, (int)(y + 11), t, K_BUBBLE_T, CENTER);
}

// Begruessung, kleine Ticks und Strecken
static char greet[24] = "";
static uint32_t greetUntil = 0, stretchStart = 0;
static bool morningPending = false, windowInit = false, wasInWindow = false;
static int tickType = 0;
static uint32_t tickStart = 0, nextTick = 0, yawnStart = 0, nextYawn = 0;
static const uint32_t TICK_MS[5] = {0, 1100, 1700, 800, 1500};

static void sayGreet(const char *t, uint32_t ms) { snprintf(greet, sizeof greet, "%s", t); greetUntil = now + ms; }
static float envelope(float p) { return p < 0.2f ? p / 0.2f : (p > 0.8f ? (1 - p) / 0.2f : 1); }
static float tickProgress(int type) {
  if (tickType != type || now - tickStart >= TICK_MS[type]) return -1;
  return (now - tickStart) / (float)TICK_MS[type];
}
void testTick(int type) { tickType = type; tickStart = now; }

static void pageHome(int mood) {
  float t = now / 1000.0f;
  float dx = sinf(t / 97.0f) * 7, dy = sinf(t / 131.0f) * 5;   // langsames Wandern gegen Einbrennen
  float hp = tickProgress(3);
  if (hp >= 0) dy -= fabsf(sinf(hp * 2 * PI_F)) * 20;          // zweimal huepfen
  if (now - stretchStart < 1500 && stretchStart) dy -= sinf((now - stretchStart) / 1500.0f * PI_F) * 26;   // strecken
  if (info[0][0] && conn != CN_OK) {
    drawSkin(CX + dx, 170 + dy, 0.75f, mood);
    gText(F_SMALL, CX, 290, info[0], C(0xffc27a), CENTER);
    gText(F_MED, CX, 322, info[1], K_TEXT, CENTER);
    gText(F_SMALL, CX, 368, info[2], K_DIM, CENTER);
    return;
  }
  int list[MAX_HINTS], n = mood == M_SLEEPY ? 0 : activeHints(list);
  bool greeting = now < greetUntil;
  char said[96] = "";
  if (greeting) {
    snprintf(said, sizeof said, "%s", greet);
  } else if (n) {
    if (now - hintSince > 4000) { hintShown++; hintSince = now; }
    hintText(list[hintShown % n], said, sizeof said);
  }
  // neue Sprechblase -> 1,6 s lang "sprechen"
  static char lastSaid[96] = "";
  static uint32_t talkStart = 0;
  if (said[0] && strcmp(said, lastSaid)) talkStart = now;
  snprintf(lastSaid, sizeof lastSaid, "%s", said);
  if (said[0]) drawBubble(said);
  look.talk = said[0] && now - talkStart < 1600 ? fabsf(sinf((now - talkStart) / 1000.0f * 11)) : 0;
  if (!said[0] && look.sad && calm(mood)) {
    rainCloud(CX + 78 + dx, (cfg.skin == SK_SOFT ? 82 : 112) + dy);
  }
  drawSkin(CX + dx, (said[0] ? 252 : 240) + dy, 1.15f, mood);
  look.talk = 0;
  if (clockValid || testHours >= 0) {
    float h = hoursNow();
    char tm[8];
    snprintf(tm, sizeof tm, "%02d:%02d", (int)h, (int)(h * 60) % 60);
    gText(F_MED, CX, 368, tm, mood == M_SLEEPY ? K_DIMMER : K_DIM, CENTER);
  }
  if (conn != CN_OK) {
    const char *s = conn == CN_AUTH ? "Token ung\xC3\xBCltig" : conn == CN_HA ? "Verbinde mit HA\xE2\x80\xA6"
                  : conn == CN_WIFI ? "Verbinde mit WLAN\xE2\x80\xA6" : conn == CN_NO_HA ? "Home Assistant fehlt" : "Kein WLAN";
    char t[64];
    gFit(F_SMALL, s, t, sizeof t, 280);
    gText(F_SMALL, CX, 406, t, K_WARN, CENTER);
  }
}

static void drawPage(int pg, int mood) {
  if (pg == 0) {
    pageHome(mood);
    if (conn == CN_OK || !info[0][0]) dots(0);
    return;
  }
  const PageCfg &p = cfg.pages[pageIdx[pg - 1]];
  Entity *e = findEntity(p.entity);
  switch (p.type) {
    case P_CLIMATE: pageClimate(p, e); break;
    case P_LIGHT: pageLight(p, e, true); break;
    case P_SWITCH: pageLight(p, e, false); break;
    case P_SENSOR: pageSensor(p, e); break;
    case P_BUTTON: pageButton(p, e); break;
    case P_VACUUM: pageVacuum(p, e); break;
  }
  miniKumpel(mood);
  dots(pg);
}

// ------------------------------------------------------------------ Bedienung
static void goPage(int to) {
  if (to < 0 || to > nShown || to == page || sliding) return;
  slideFrom = page; slideTo = to; slideStart = now; sliding = true;
  page = to;
  pendTemp = NAN; pendBright = -1;
}

static int hitTest(int x, int y, int &sub) {
  sub = 0;
  auto near = [&](float cx, float cy, float r) { return (x - cx) * (x - cx) + (y - cy) * (y - cy) < r * r; };
  if (page == 0) return T_CHAR;
  if (near(CX, 396, 46)) return T_CHAR;
  const PageCfg &p = cfg.pages[pageIdx[page - 1]];
  Entity *e = findEntity(p.entity);
  switch (p.type) {
    case P_CLIMATE:
      if (near(CX - 82, BTN_Y, 44)) return T_MINUS;
      if (near(CX + 82, BTN_Y, 44)) return T_PLUS;
      if (near(CX, BTN_Y + 4, 36)) return T_POWER;
      if (onArc(x, y)) return T_ARC;
      break;
    case P_LIGHT:
      if (e && e->hasBright && onArc(x, y)) return T_ARC;
      if (e) {
        int list[5], n = presetList(e, list);
        for (int i = 0; i < n; i++) if (near(presetX(i, n), 338, 22)) { sub = list[i]; return T_PRESET; }
      }
      if (near(CX, 214, 100)) return T_MAIN;
      break;
    case P_SWITCH: if (near(CX, 232, 110)) return T_MAIN; break;
    case P_BUTTON: if (near(CX, 222, 100)) return T_MAIN; break;
    case P_VACUUM:
      for (int i = 0; i < 3; i++) if (near(VAC_X[i], 290, 46)) { sub = i; return T_VAC; }
      break;
  }
  return T_NONE;
}

static void dragArc(int x, int y) {
  const PageCfg &p = cfg.pages[pageIdx[page - 1]];
  Entity *e = findEntity(p.entity);
  if (!e || !e->known) return;
  float f = arcFraction(x, y, true);
  if (p.type == P_CLIMATE) {
    if (!strcmp(e->state, "off")) return;
    float st = e->step > 0 ? e->step : 0.5f;
    float v = e->tmin + f * (e->tmax - e->tmin);
    pendTemp = roundf(v / st) * st;
    pendTempSend = 0;   // erst beim Loslassen senden
  } else {
    pendBright = (int)(f * 255 + 0.5f);
    if (pendBright < 3) pendBright = 3;
    pendUntil = now + 4000;
  }
}

static void releaseArc() {
  const PageCfg &p = cfg.pages[pageIdx[page - 1]];
  Entity *e = findEntity(p.entity);
  if (!e) return;
  if (p.type == P_CLIMATE && !isnan(pendTemp)) {
    pendTempSend = now + 150;
  } else if (p.type == P_LIGHT && pendBright >= 0) {
    pushAction("light", "turn_on", e->id, "\"brightness\":%d", pendBright);
    e->brightness = pendBright;
    snprintf(e->state, sizeof e->state, "on");
    pendUntil = now + 2500;
  }
}

static void onTap(int target, int sub) {
  // nochmal antippen, solange er sich noch freut -> Herzaugen
  if (target == T_CHAR && now < happyUntil) heartsUntil = now + 2500;
  happyUntil = target == T_CHAR ? now + 2500 : happyUntil;
  if (page == 0) { happyUntil = now + 2500; return; }
  const PageCfg &p = cfg.pages[pageIdx[page - 1]];
  Entity *e = findEntity(p.entity);
  if (!e || !e->known || target == T_NONE || target == T_CHAR) return;
  char dom[16];
  domainOf(e->id, dom, sizeof dom);
  switch (target) {
    case T_MINUS: case T_PLUS: {
      float base = climateShown(e);
      if (isnan(base)) return;
      float st = e->step > 0 ? e->step : 0.5f;
      float v = base + (target == T_PLUS ? st : -st);
      if (v < e->tmin) v = e->tmin;
      if (v > e->tmax) v = e->tmax;
      pendTemp = v;
      pendTempSend = now + 900;   // mehrfach tippen sammeln
      break;
    }
    case T_POWER:
      pushAction("climate", !strcmp(e->state, "off") ? "turn_on" : "turn_off", e->id);
      snprintf(e->state, sizeof e->state, "%s", !strcmp(e->state, "off") ? "heat" : "off");
      break;
    case T_MAIN:
      if (p.type == P_BUTTON) {
        if (!strcmp(dom, "scene") || !strcmp(dom, "script")) pushAction(dom, "turn_on", e->id);
        else if (!strcmp(dom, "automation")) pushAction(dom, "trigger", e->id);
        else pushAction(dom, "press", e->id);
        doneUntil = now + 1500;
        happyUntil = now + 2000;
      } else {
        pushAction(!strcmp(dom, "light") ? "light" : "homeassistant", "toggle", e->id);
        snprintf(e->state, sizeof e->state, "%s", isOn(e) ? "off" : "on");
      }
      break;
    case T_PRESET: {
      auto &pr = PRESETS[sub];
      if (pr.kelvin) pushAction("light", "turn_on", e->id, "\"color_temp_kelvin\":%d", pr.k);
      else pushAction("light", "turn_on", e->id, "\"rgb_color\":[%d,%d,%d]", pr.r, pr.g, pr.b);
      e->rgb[0] = pr.r; e->rgb[1] = pr.g; e->rgb[2] = pr.b;
      e->hasRgb = e->hasRgb || !pr.kelvin;
      snprintf(e->state, sizeof e->state, "on");
      break;
    }
    case T_VAC: {
      static const char *svc[3] = {"start", "pause", "return_to_base"};
      pushAction("vacuum", svc[sub], e->id);
      break;
    }
  }
}

static void onLong(int target) {
  if (page == 0 && target == T_CHAR) {
    cfg.skin = (cfg.skin + 1) % SKINS;
    skinChanged = true;
    happyUntil = now + 2500;
  }
}

static void input(const Touch *pts, int n) {
  if (n > 0) lastTouch = now;
  if (sliding) { if (n == 0) down = false; return; }
  if (n > 0 && !down) {
    if (morningPending && !sliding) {
      morningPending = false;
      if (hoursNow() < 12) { sayGreet("Guten Morgen!", 4500); stretchStart = now; happyUntil = now + 3000; }
    }
    down = true; moved = longFired = dragging = false;
    sx = lx = pts[0].x; sy = ly = pts[0].y; downMs = now;
    tgt = hitTest(sx, sy, tgtSub);
    if (tgt == T_ARC) { dragging = true; dragArc(sx, sy); }
    return;
  }
  if (n > 0 && down) {
    lx = pts[0].x; ly = pts[0].y;
    if (dragging) { dragArc(lx, ly); return; }
    if (abs(lx - sx) > 18 || abs(ly - sy) > 18) moved = true;
    if (!moved && !longFired && now - downMs > 700) { longFired = true; onLong(tgt); }
    return;
  }
  if (n == 0 && down) {
    down = false;
    if (dragging) { releaseArc(); return; }
    int dx = lx - sx, dy = ly - sy;
    if (moved && abs(dx) > 60 && abs(dx) > abs(dy) * 1.3f) goPage(dx < 0 ? page + 1 : page - 1);
    else if (!moved && !longFired) onTap(tgt, tgtSub);
  }
}

// ------------------------------------------------------------------ Ablauf
void begin(uint16_t *fb, uint32_t seed) {
  gBegin(fb);
  rng = seed ? seed : 1;
  Config c;
  defaultConfig(c);
  setConfig(c);
  info[0][0] = info[1][0] = info[2][0] = 0;
}

void tick(uint32_t nowMs, const Touch *pts, int n) {
  now = nowMs;
  input(pts, n);

  // Klima: gesammelte Aenderung senden
  if (!isnan(pendTemp) && pendTempSend && now >= pendTempSend && page > 0) {
    const PageCfg &p = cfg.pages[pageIdx[page - 1]];
    Entity *e = findEntity(p.entity);
    if (e && p.type == P_CLIMATE) {
      pushAction("climate", "set_temperature", e->id, "\"temperature\":%.1f", pendTemp);
      e->target = pendTemp;
    }
    pendTemp = NAN; pendTempSend = 0;
  }
  // zurueck zum Kumpel nach einer Weile
  if (page != 0 && !down && !sliding && now - lastTouch > cfg.homeAfter * 1000u) goPage(0);

  // Blinzeln und Blick
  if (now >= nextBlink) { blinkStart = now; nextBlink = now + (uint32_t)frand(2500, 6500); }
  if (now >= nextGaze) {
    gazeTX = frand(-1, 1); gazeTY = frand(-0.6f, 0.6f);
    if (rnd() % 3 == 0) gazeTX = gazeTY = 0;
    nextGaze = now + (uint32_t)frand(1800, 5200);
  }
  // Tagesablauf: abends "Gute Nacht!", morgens wartet er auf das erste Antippen
  bool w = inSleepWindow();
  if (windowInit && clockValid) {
    if (w && !wasInWindow) { sayGreet("Gute Nacht!", 6000); morningPending = false; }
    if (!w && wasInWindow) morningPending = true;
  }
  if (clockValid) { wasInWindow = w; windowInit = true; }
  if (morningPending && hoursNow() >= 12) morningPending = false;

  int mood = currentMood();
  // kleine Ticks ab und zu, nur wenn er einfach so dasitzt
  if (now >= nextTick) {
    if (page == 0 && mood == M_NEUTRAL && !sliding && !down) { tickType = 1 + rnd() % 4; tickStart = now; }
    nextTick = now + (uint32_t)frand(14000, 40000);
  }
  if (tickType && now - tickStart >= TICK_MS[tickType]) tickType = 0;
  // Gaehnen, wenn er abends muede wird
  float tir = tiredness();
  if (mood == M_TIRED && now >= nextYawn) { yawnStart = now; nextYawn = now + (uint32_t)frand(22000, 50000); }
  float yp = (now - yawnStart) / 2200.0f;
  look.yawn = yawnStart && yp < 1 && mood == M_TIRED ? sinf(yp * PI_F) : 0;
  look.lid = mood == M_TIRED ? 0.3f + 0.45f * tir : 0;
  float cp = tickProgress(1);
  look.cross = cp >= 0 ? envelope(cp) : 0;
  look.wink = look.tongue = tickProgress(4) >= 0;
  look.talk = 0;   // wird auf der Startseite gesetzt, wenn eine Sprechblase neu erscheint
  look.shades = sunny() && !inSleepWindow();
  look.sad = rainy() && !look.shades;
  look.sweat = hot();

  int dummy[MAX_HINTS];
  float tx = gazeTX, ty = gazeTY;
  if (page == 0 && mood == M_NEUTRAL && activeHints(dummy)) { tx = 0; ty = -1; }
  float up = tickProgress(2);
  if (up >= 0) { tx = up < 0.5f ? -0.4f : 0.4f; ty = -1.2f; }
  if (look.cross > 0) tx = ty = 0;
  if (mood == M_SLEEPY || mood == M_HAPPY) tx = ty = 0;
  gazeX += (tx - gazeX) * 0.12f;
  gazeY += (ty - gazeY) * 0.12f;

  gClear(K_BG);
  if (sliding) {
    float p = (now - slideStart) / (float)SLIDE_MS;
    if (p >= 1) { sliding = false; p = 1; }
    float e = 1 - (1 - p) * (1 - p) * (1 - p);
    int dir = slideTo > slideFrom ? 1 : -1;
    if (sliding) {
      gOffset((int)(-dir * SCR * e), 0); drawPage(slideFrom, mood);
      gOffset((int)(dir * SCR * (1 - e)), 0); drawPage(slideTo, mood);
      gOffset(0, 0);
      return;
    }
  }
  drawPage(page, mood);
}

Debug debug() {
  Debug d;
  d.page = page;
  d.mood = currentMood();
  int l[MAX_HINTS];
  d.bubble = page == 0 && d.mood != M_SLEEPY && activeHints(l) > 0;
  d.sliding = sliding;
  Entity *e = page > 0 ? findEntity(cfg.pages[pageIdx[page - 1]].entity) : nullptr;
  d.shownTarget = e ? climateShown(e) : NAN;
  d.tick = tickType;
  d.shades = look.shades;
  d.sad = look.sad;
  d.sweat = look.sweat;
  d.lid = look.lid;
  d.yawn = look.yawn;
  d.greet = now < greetUntil ? greet : "";
  d.hearts = heartsNow();
  if (now < greetUntil) d.bubble = page == 0;
  return d;
}

}  // namespace kb
