// Kleine Welt - Prototyp: ein runder Raum, ein Held, ein Schleimling,
// ein Schalter-Raetsel (Steinblock auf die Bodenplatte schieben -> Tuer auf).
//
// Steuerung (wie bei Handheld-Zeldas mit Stift):
//   Finger halten          -> Held laeuft zum Finger
//   kurz tippen (Boden)    -> Held laeuft zu dieser Stelle
//   tippen nah am Held     -> Schwerthieb in diese Richtung
//   tippen auf den Gegner  -> Held laeuft hin und schlaegt zu
//   gegen den Block laufen -> Block rutscht eine Kachel weiter
//
// Alle Grafik ist eigene Pixelkunst (16x16, doppelt skaliert) bzw. wird im Code
// erzeugt. Kein fremdes Material.
#include "game.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

namespace kw {

// ---------------------------------------------------------------- Grundlagen
static uint16_t *fb = nullptr, *bg = nullptr;
static uint32_t rng = 1;
static uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static float frand(float a, float b) { return a + (b - a) * (rnd() % 10000) / 10000.0f; }

static constexpr uint16_t C(uint32_t rgb) {
  return (uint16_t)((((rgb >> 16) & 0xF8) << 8) | (((rgb >> 8) & 0xFC) << 3) | ((rgb & 0xFF) >> 3));
}
static constexpr uint16_t TRANSP = 0xF81F;  // Magenta = durchsichtig in Kunst-Puffern
static constexpr int SC = 3;               // Pixelkunst wird 3-fach gezeichnet (16 px -> 48 px)

static inline void unpack(uint16_t c, int &r, int &g, int &b) {
  r = (c >> 11) << 3; g = ((c >> 5) & 63) << 2; b = (c & 31) << 3;
}
static inline uint16_t pack(int r, int g, int b) {
  if (r < 0) r = 0; if (g < 0) g = 0; if (b < 0) b = 0;
  if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
static inline uint16_t mix(uint16_t a, uint16_t b, float t) {
  int r1, g1, b1, r2, g2, b2; unpack(a, r1, g1, b1); unpack(b, r2, g2, b2);
  return pack(r1 + (int)((r2 - r1) * t), g1 + (int)((g2 - g1) * t), b1 + (int)((b2 - b1) * t));
}
static inline void px(int x, int y, uint16_t c) {
  if ((unsigned)x < (unsigned)SCR && (unsigned)y < (unsigned)SCR) fb[y * SCR + x] = c;
}
static void rect(int x, int y, int w, int h, uint16_t c) {
  if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; }
  if (x + w > SCR) w = SCR - x; if (y + h > SCR) h = SCR - y;
  for (int j = 0; j < h; j++) { uint16_t *p = fb + (y + j) * SCR + x; for (int i = 0; i < w; i++) p[i] = c; }
}
// "dicker Pixel": SCxSC, damit alles im selben Pixelraster bleibt
static inline void fat(int x, int y, uint16_t c) { rect(x - ((x % SC) + SC) % SC, y - ((y % SC) + SC) % SC, SC, SC, c); }
static void darkenRect(int x, int y, int w, int h, float k) {
  for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++)
    if ((unsigned)i < (unsigned)SCR && (unsigned)j < (unsigned)SCR) fb[j * SCR + i] = mix(fb[j * SCR + i], 0, k);
}

// ---------------------------------------------------------------- Farben
static const uint16_t K_OUT = C(0x1b1424);
static uint16_t colorOf(char ch) {
  switch (ch) {
    case 'k': return K_OUT;
    case 'h': return C(0x2fa39a);  // Kapuze
    case 'H': return C(0x1f6e6a);
    case 'i': return C(0x7fdccc);
    case 's': return C(0xf2c79b);  // Haut
    case 'S': return C(0xd39a70);
    case 'o': return C(0xf08a3c);  // Schal
    case 'O': return C(0xb85a24);
    case 't': return C(0xe8dcc0);  // Kittel
    case 'T': return C(0xb9a98a);
    case 'b': return C(0x6e4630);  // Stiefel
    case 'w': return C(0xffffff);
    case 'p': return C(0xe0507a);  // Schleimling
    case 'P': return C(0xff9ab8);
    case 'q': return C(0x9e2f55);
    case 'r': return C(0xe8434f);  // Herz
    case 'R': return C(0xffb0b5);
    case 'e': return C(0x3d3350);
    case 'y': return C(0xffe27a);  // Flamme
    case 'Y': return C(0xff8a2a);
    case 'n': return C(0x7a5236);  // Holz
    case 'N': return C(0x4a2e20);
    default: return TRANSP;
  }
}

// ---------------------------------------------------------------- Pixelkunst
// Held: unten A/B, oben A/B, rechts A/B (links = gespiegelt)
static const char *HERO_DOWN[16] = {
  "................", ".....kkkkkk.....", "...kkhhiihhkk...", "..khhhiihhhhhk..",
  "..khhhhhhhhhhk..", ".khhhssssssShhk.", ".khhsssssssSShk.", ".kHhskssssksShk.",
  ".kHhsssssssSShk.", "..kHooooooooHk..", "..kOoooooooOOk..", ".kSkttttttttkSk.",
  ".kkktttTTtttkkk.", "...kTtttttttTk..", "...kbbk..kbbk...", "...kkkk..kkkk..."};
static const char *HERO_UP[16] = {
  "................", ".....kkkkkk.....", "...kkhhiihhkk...", "..khhhiihhhhhk..",
  "..khhhhhhhhhhk..", ".khhhhhhhhhhhhk.", ".khhhhiihhhhhhk.", ".kHhhhhhhhhhhHk.",
  ".kHHhhhhhhhhHHk.", "..kHHHHHHHHHHk..", "..kOoooooooOOk..", ".kSkttttttttkSk.",
  ".kkktttTTtttkkk.", "...kTtttttttTk..", "...kbbk..kbbk...", "...kkkk..kkkk..."};
static const char *HERO_SIDE[16] = {
  "................", "....kkkkkk......", "..kkhhiihhkk....", ".khhhiihhhhhk...",
  ".khhhhhhhhhhhk..", "kHhhhhhhsssssk..", "kHhhhhhssssssk..", "kHHhhhhssskssk..",
  ".kHhhhhsssssk...", "..kOOooooooOk...", ".kOOk.kttttSk...", "..kk.ktttttSk...",
  ".....kttTTtk....", ".....kTtttTk....", ".....kbbkbbk....", ".....kkkkkkk...."};
static const char *FEET_B_FRONT[2] = {"....kbbkkbbk....", "....kkkkkkkk...."};
static const char *FEET_B_SIDE[2] = {"....kbbk.kbbk...", "....kkkk.kkkk..."};

static const char *SLIME_A[16] = {
  "................", "................", "................", "................",
  "................", "................", "......kkkk......", "....kkPPppkk....",
  "...kPPpppppqk...", "..kPppppppppqk..", "..kpwwppppwwpk..", ".kppwkppppwkpqk.",
  ".kpppppppppppqk.", ".kqppppppppppqk.", "..kqqqqqqqqqqk..", "...kkkkkkkkkk..."};
static const char *SLIME_B[16] = {
  "................", "................", "................", "................",
  "................", "................", "................", "................",
  "......kkkk......", "...kkkPPppkkk...", "..kPPpppppppqk..", ".kPpwwppppwwpqk.",
  "kppwkppppppwkpqk", "kqppppppppppppqk", ".kqqqqqqqqqqqqk.", "..kkkkkkkkkkkk.."};

static const char *TORCH_A[16] = {
  "................", "................", "........y.......", ".......yy.......",
  ".......yYy......", "......yYYYy.....", "......YYyYY.....", "......YyyyY.....",
  ".......YyY......", "......knnnnk....", ".......kNNk.....", "........nn......",
  "........nn......", ".......kNNk.....", "................", "................"};
static const char *TORCH_B[16] = {
  "................", "................", ".......y........", ".......yy.......",
  "......yYy.......", "......yYYYy.....", ".....YYyYY......", "......YyyyY.....",
  ".......YyY......", "......knnnnk....", ".......kNNk.....", "........nn......",
  "........nn......", ".......kNNk.....", "................", "................"};

static const char *HEART[8] = {".kk.kk..", "kRrkrrk.", "kRrrrrk.", "krrrrrk.",
                               ".krrrk..", "..krk...", "...k....", "........"};

struct Art { int w, h; uint16_t px[256]; };
static Art aHeroDown[2], aHeroUp[2], aHeroSide[2], aSlime[2], aTorch[2];
static Art aHeartFull, aHeartHalf, aHeartEmpty, aBlock, aPlateUp, aPlateDown, aPillar;

static void artFromAscii(Art &a, const char *const *rows, int w, int h) {
  a.w = w; a.h = h;
  for (int y = 0; y < h; y++) {
    const char *r = rows[y];
    for (int x = 0; x < w; x++) a.px[y * w + x] = colorOf(r[x]);
  }
}

// Kunst zeichnen, SC-fach skaliert. flash = alle Pixel weiss (Treffer).
static void drawArt(const Art &a, int x, int y, bool flip = false, bool flash = false) {
  for (int j = 0; j < a.h; j++)
    for (int i = 0; i < a.w; i++) {
      uint16_t c = a.px[j * a.w + (flip ? a.w - 1 - i : i)];
      if (c == TRANSP) continue;
      if (flash) c = 0xFFFF;
      rect(x + i * SC, y + j * SC, SC, SC, c);
    }
}

// Prozedurale Kunst: Block, Platte, Saeule
static void buildProcArt() {
  // Steinblock: helle Oberseite, dunkle Vorderseite, eingemeisseltes Zeichen
  const uint16_t top = C(0xa69cb5), topL = C(0xc4bcd0), topD = C(0x8a8199);
  const uint16_t front = C(0x6e6580), frontD = C(0x534b63);
  Art &b = aBlock; b.w = b.h = 16;
  for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
    uint16_t c;
    bool edge = x == 0 || x == 15 || y == 0 || y == 15;
    if (edge) c = K_OUT;
    else if (y >= 11) c = (y == 14 || x == 14) ? frontD : front;
    else if (y == 1 || x == 1) c = topL;
    else if (x == 14 || y == 10) c = topD;
    else c = top;
    // Zeichen: kleine Raute in der Mitte der Oberseite
    int dx = x - 8, dy = y - 6; if (dx < 0) dx = -dx - 1; if (dy < 0) dy = -dy - 1;
    if (y < 11 && !edge && dx + dy == 2) c = topD;
    if (y >= 11 && !edge && (x == 4 || x == 11) && y == 12) c = frontD;
    b.px[y * 16 + x] = c;
  }
  // Bodenplatte: goldene Platte, gedrueckt = tiefer und dunkler
  for (int pressed = 0; pressed < 2; pressed++) {
    Art &p = pressed ? aPlateDown : aPlateUp; p.w = p.h = 16;
    const uint16_t g = pressed ? C(0x8a6a2a) : C(0xd1a94e), gl = pressed ? C(0x9e7c36) : C(0xf0cf7c);
    const uint16_t gd = pressed ? C(0x5e481c) : C(0x8a6a2a), rim = C(0x2a2236);
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
      uint16_t c = TRANSP;
      if (x >= 1 && x <= 14 && y >= 1 && y <= 14) c = rim;
      int o = pressed ? 1 : 0;
      if (x >= 3 && x <= 12 && y >= 3 + o && y <= 12) {
        c = g;
        if (x == 3 || y == 3 + o) c = gl;
        if (x == 12 || y == 12) c = gd;
        int dx = 2 * x - 15, dy = 2 * y - 15 - o; int d2 = dx * dx + dy * dy;
        if (d2 >= 9 && d2 <= 25) c = gd;  // Ring in der Mitte
      }
      p.px[y * 16 + x] = c;
    }
  }
  // Saeule: runder Kopf, Schaft mit Licht von links oben
  Art &s = aPillar; s.w = s.h = 16;
  const uint16_t sl = C(0xc9c0d8), sm = C(0x9a90ab), sd = C(0x6b6280), sdd = C(0x4c455c);
  for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
    uint16_t c = TRANSP;
    float fx = x + 0.5f - 8, fy = y + 0.5f - 5.5f;
    bool cap = fx * fx / 42.0f + fy * fy / 20.0f <= 1.0f;
    bool capO = fx * fx / 52.0f + fy * fy / 27.0f <= 1.0f;
    bool shaft = x >= 2 && x <= 13 && y >= 5 && y <= 14;
    bool shaftO = x >= 1 && x <= 14 && y >= 5 && y <= 15;
    if (shaftO || capO) c = K_OUT;
    if (shaft) c = x <= 4 ? sm : (x >= 11 ? sdd : sd);
    if (shaft && (y == 9 || y == 10) ) c = (x <= 4) ? sd : sdd;  // Ring
    if (cap) c = (fx + fy < -2) ? sl : sm;
    if (cap && fx * fx / 12.0f + fy * fy / 5.0f <= 1.0f) c = sd;  // Mulde oben
    s.px[y * 16 + x] = c;
  }
}

static void buildArt() {
  for (int f = 0; f < 2; f++) {
    const char *rows[16];
    for (int y = 0; y < 16; y++) rows[y] = HERO_DOWN[y];
    if (f) { rows[14] = FEET_B_FRONT[0]; rows[15] = FEET_B_FRONT[1]; }
    artFromAscii(aHeroDown[f], rows, 16, 16);
    for (int y = 0; y < 16; y++) rows[y] = HERO_UP[y];
    if (f) { rows[14] = FEET_B_FRONT[0]; rows[15] = FEET_B_FRONT[1]; }
    artFromAscii(aHeroUp[f], rows, 16, 16);
    for (int y = 0; y < 16; y++) rows[y] = HERO_SIDE[y];
    if (f) { rows[14] = FEET_B_SIDE[0]; rows[15] = FEET_B_SIDE[1]; }
    artFromAscii(aHeroSide[f], rows, 16, 16);
  }
  artFromAscii(aSlime[0], SLIME_A, 16, 16);
  artFromAscii(aSlime[1], SLIME_B, 16, 16);
  artFromAscii(aTorch[0], TORCH_A, 16, 16);
  artFromAscii(aTorch[1], TORCH_B, 16, 16);
  artFromAscii(aHeartFull, HEART, 8, 8);
  aHeartHalf = aHeartFull; aHeartEmpty = aHeartFull;
  const uint16_t red = colorOf('r'), redL = colorOf('R'), em = colorOf('e');
  for (int i = 0; i < 64; i++) {
    uint16_t c = aHeartFull.px[i];
    if (c == red || c == redL) {
      aHeartEmpty.px[i] = em;
      if ((i % 8) >= 4) aHeartHalf.px[i] = em;
    }
  }
  buildProcArt();
}

// ---------------------------------------------------------------- Schrift 5x7
static const uint8_t FONT[][5] = {
  {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x50,0x30,0x00,0x00},  // ' ' ! ,
  {0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},{0x00,0x36,0x36,0x00,0x00},  // - . :
  {0x14,0x14,0x14,0x14,0x14},{0x02,0x01,0x51,0x09,0x06},                            // = ?
  {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},  // 0 1 2
  {0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},  // 3 4 5
  {0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},  // 6 7 8
  {0x06,0x49,0x49,0x29,0x1E},                                                        // 9
  {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},  // A B C
  {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},  // D E F
  {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},  // G H I
  {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},  // J K L
  {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},  // M N O
  {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},  // P Q R
  {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},  // S T U
  {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},  // V W X
  {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},                            // Y Z
  {0x3D,0x40,0x40,0x40,0x3D},{0x39,0x44,0x44,0x44,0x39},{0x7D,0x12,0x11,0x12,0x7D},  // Ü Ö Ä
};
static int glyphIndex(const char *&s) {
  unsigned char c = (unsigned char)*s++;
  if (c == 0xC3) {
    unsigned char d = (unsigned char)*s++;
    if (d == 0x9C || d == 0xBC) return 44;  // Ü ü
    if (d == 0x96 || d == 0xB6) return 45;  // Ö ö
    if (d == 0x84 || d == 0xA4) return 46;  // Ä ä
    return 0;
  }
  if (c >= 'a' && c <= 'z') c -= 32;
  switch (c) {
    case ' ': return 0; case '!': return 1; case ',': return 2; case '-': return 3;
    case '.': return 4; case ':': return 5; case '=': return 6; case '?': return 7;
  }
  if (c >= '0' && c <= '9') return 8 + (c - '0');
  if (c >= 'A' && c <= 'Z') return 18 + (c - 'A');
  return 0;
}
static int textWidth(const char *s, int sc) {
  int n = 0; while (*s) { glyphIndex(s); n++; }
  return n ? n * 6 * sc - sc : 0;
}
static void textRaw(const char *s, int x, int y, int sc, uint16_t c) {
  while (*s) {
    const uint8_t *g = FONT[glyphIndex(s)];
    for (int i = 0; i < 5; i++) for (int j = 0; j < 7; j++)
      if (g[i] >> j & 1) rect(x + i * sc, y + j * sc, sc, sc, c);
    x += 6 * sc;
  }
}
// zentriert mit dunklem Rand, damit es auf jedem Boden lesbar ist
static void text(const char *s, int cy, int sc, uint16_t c) {
  int w = textWidth(s, sc), x = (SCR - w) / 2, y = cy - 7 * sc / 2;
  for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++)
    if (dx || dy) textRaw(s, x + dx * sc / 2 * 2, y + dy * sc / 2 * 2, sc, K_OUT);
  textRaw(s, x, y, sc, c);
}

// ---------------------------------------------------------------- Raum
constexpr int TILE = 16 * SC, N = 11, OFF = (SCR - N * TILE) / 2;  // 11x11 Kacheln a 48 px, Mitte = (5,5)
constexpr float HB = 15;   // halbe Breite Held/Schleimling
constexpr float BH = 23;   // halbe Breite Block
enum Tile : uint8_t { T_WALL, T_FLOOR, T_PILLAR, T_DOOR, T_PLATE };
static uint8_t map_[N][N];
static inline int cellOf(float p) { return (int)floorf((p - OFF) / TILE); }
static inline float cellC(int i) { return OFF + i * TILE + TILE / 2.0f; }
static const int DOOR_I = 5, DOOR_J = 1, PLATE_I = 7, PLATE_J = 7;
static const int BLOCK_I0 = 3, BLOCK_J0 = 4, HERO_I0 = 5, HERO_J0 = 8, SLIME_I0 = 6, SLIME_J0 = 5;
static const int TORCHES[2][2] = {{3, 1}, {7, 1}};

static inline uint8_t tileAt(int i, int j) {
  if (i < 0 || j < 0 || i >= N || j >= N) return T_WALL;
  return map_[j][i];
}
static inline bool walkable(uint8_t t) { return t == T_FLOOR || t == T_PLATE; }

static void buildMap() {
  for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
    int di = i - 5, dj = j - 5;
    map_[j][i] = di * di + dj * dj <= 13 ? T_FLOOR : T_WALL;   // Radius ~3,6 Kacheln
  }
  map_[DOOR_J][DOOR_I] = T_DOOR;
  map_[PLATE_J][PLATE_I] = T_PLATE;
  map_[2][4] = T_PILLAR; map_[2][6] = T_PILLAR;     // rahmen den Tuergang ein
  map_[6][3] = T_PILLAR;
}

static uint32_t hash2(int a, int b, int c) {
  uint32_t h = (uint32_t)a * 374761393u + (uint32_t)b * 668265263u + (uint32_t)c * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16);
}

// Raum einmal vorrendern: Boden, Waende, Saeulen, Licht, runder Rand
static void buildBackground() {
  uint16_t *save = fb; fb = bg;
  const uint16_t fBase[2] = {C(0x5b4e6e), C(0x564a68)}, fL = C(0x6c5f80), fD = C(0x4a3f5b), fS = C(0x382e47);
  const uint16_t wTop = C(0x241b2f), wTop2 = C(0x2c2238);
  const uint16_t brick = C(0x4a3b5f), brickL = C(0x5d4b75), brickD = C(0x3a2e4b), mortar = C(0x1d1628);
  for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
    int x0 = OFF + i * TILE, y0 = OFF + j * TILE;
    uint8_t t = map_[j][i];
    bool face = t == T_WALL || t == T_DOOR ? walkable(tileAt(i, j + 1)) || tileAt(i, j + 1) == T_PILLAR : false;
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
      uint16_t c;
      uint32_t h = hash2(i * 16 + x, j * 16 + y, 7);
      if (t == T_WALL || t == T_DOOR) {
        if (face) {
          int row = y / 4, bx = (x + (row & 1) * 4) % 8;
          if (y % 4 == 3 || bx == 7) c = mortar;
          else if (y % 4 == 0) c = brickL;
          else if (y % 4 == 2 || bx == 6) c = brickD;
          else c = brick;
          if (y >= 14) c = mortar;
        } else {
          c = (h & 7) == 0 ? wTop2 : wTop;
        }
      } else {
        uint16_t base = fBase[hash2(i, j, 1) & 1];
        bool seam = (x == 15 || y == 15) || (x == 7 && y < 8 && (hash2(i, j, 2) & 1));
        if (seam) c = fS;
        else if (x == 0 || y == 0 || (x == 8 && y < 8 && (hash2(i, j, 2) & 1))) c = fL;
        else if ((h & 31) == 0) c = fD;
        else if ((h & 31) == 1) c = fL;
        else c = base;
        // Schatten unter der Wand
        uint8_t up = tileAt(i, j - 1);
        if ((up == T_WALL || up == T_DOOR) && y < 3) c = mix(c, 0, 0.45f - y * 0.12f);
      }
      rect(x0 + x * SC, y0 + y * SC, SC, SC, c);
    }
    if (t == T_PILLAR) {
      // Schlagschatten nach rechts unten, dann die Saeule
      darkenRect(x0 + 9, y0 + 33, 42, 15, 0.35f);
      drawArt(aPillar, x0, y0);
    }
  }
  // Licht: Mitte etwas heller, Fackeln warm, Rand faellt ins Schwarze (rundes Fenster)
  for (int y = 0; y < SCR; y++) for (int x = 0; x < SCR; x++) {
    float dx = x - 233.0f, dy = y - 233.0f, r = sqrtf(dx * dx + dy * dy);
    uint16_t c = bg[y * SCR + x];
    float k = 0.22f * (r / 233.0f) * (r / 233.0f);
    if (r > 212) k += (r - 212) / 21.0f;
    if (k > 1) k = 1;
    c = mix(c, 0, k);
    for (auto &tc : TORCHES) {
      float tx = cellC(tc[0]), ty = cellC(tc[1]) + 4;
      float d = sqrtf((x - tx) * (x - tx) + (y - ty) * (y - ty));
      if (d < 120) c = mix(c, C(0xff9a40), 0.16f * (1 - d / 120.0f) * (1 - d / 120.0f));
    }
    bg[y * SCR + x] = c;
  }
  fb = save;
}

// ---------------------------------------------------------------- Spielzustand
enum Mode { M_PLAY, M_WIN, M_DEAD };
enum Face { F_DOWN, F_UP, F_RIGHT, F_LEFT };
static Mode mode;
static uint32_t lastMs = 0, startMs = 0, endMs = 0;
static float endT = 0;      // Spielzeit beim Ende (fuer das Abdunkeln)
static float t_ = 0;        // Spielzeit in s (Animationen)
static float freeze = 0;    // Trefferstopp
static float shake = 0;

struct Hero {
  float x, y; int hp; Face face;
  bool moving; float walkT; int frame;
  bool hasTarget, following, chase; float tx, ty;
  float atkT; float atkAng; bool atkHit; float cool;
  float inv; float kbT, kbx, kby;
  float pushT; int pushDi, pushDj;
  float deadT;
} hero;

struct Slime {
  float x, y; int hp; int state;  // 0 warten, 1 huepfen, 2 getroffen, 3 zerplatzt, 4 weg
  float t, dur; float fx, fy, tx, ty; float hz; float flash; int anim;
} slime;

struct Block {
  int i, j, fi, fj; float t; bool moving;
  float stuckT, sinkT;  // festgefahren -> versinkt und taucht am Start wieder auf
} block;

struct Pickup { bool on; float x, y, t; } heartDrop;

static float doorOpen = 0;  // 0 zu .. 1 offen
static bool doorWanted = false;
static char msg[40]; static float msgT = 0;
static float hintT = 0;

struct Particle { float x, y, vx, vy, life, max; uint16_t c; };
static Particle parts[48];
static void spawn(float x, float y, int n, uint16_t c, float sp, float life) {
  for (int k = 0; k < n; k++) for (auto &p : parts) if (p.life <= 0) {
    float a = frand(0, 6.2832f), v = frand(sp * 0.3f, sp);
    p = {x, y, cosf(a) * v, sinf(a) * v, life, life, c}; break;
  }
}
static void say(const char *s, float sec) { strncpy(msg, s, sizeof msg - 1); msg[sizeof msg - 1] = 0; msgT = sec; }

static void blockPos(float &x, float &y) {
  float t = block.moving ? block.t : 1.0f;
  float e = t * t * (3 - 2 * t);
  x = cellC(block.fi) + (cellC(block.i) - cellC(block.fi)) * e;
  y = cellC(block.fj) + (cellC(block.j) - cellC(block.fj)) * e;
}
static bool plateWeighted() {
  if (!block.moving && block.sinkT <= 0 && block.i == PLATE_I && block.j == PLATE_J) return true;
  if (cellOf(hero.x) == PLATE_I && cellOf(hero.y) == PLATE_J && mode == M_PLAY) return true;
  return false;
}

// Kollision: Kachel fest? (Block extra)
static bool solidTile(int i, int j) {
  uint8_t t = tileAt(i, j);
  if (t == T_DOOR) return doorOpen < 0.95f;
  return !walkable(t);
}
static bool boxHits(float x, float y, float h, bool withBlock) {
  int i0 = cellOf(x - h), i1 = cellOf(x + h - 0.01f), j0 = cellOf(y - h), j1 = cellOf(y + h - 0.01f);
  for (int j = j0; j <= j1; j++) for (int i = i0; i <= i1; i++) if (solidTile(i, j)) return true;
  if (withBlock && block.sinkT <= 0) {
    float bx, by; blockPos(bx, by);
    if (fabsf(x - bx) < h + BH && fabsf(y - by) < h + BH) return true;
  }
  return false;
}
// bewegt eine Box, Achsen getrennt; meldet, auf welcher Achse es klemmt
static void moveBox(float &x, float &y, float dx, float dy, float h, bool withBlock, bool *hitX, bool *hitY) {
  // steckt die Box schon im Block (z.B. nach einem Schubser), darf sie sich frei herausbewegen
  if (withBlock && boxHits(x, y, h, true) && !boxHits(x, y, h, false)) withBlock = false;
  int steps = (int)(fmaxf(fabsf(dx), fabsf(dy)) / 4) + 1;
  float sx = dx / steps, sy = dy / steps;
  bool hx = false, hy = false;
  for (int s = 0; s < steps; s++) {
    if (!hx) { if (boxHits(x + sx, y, h, withBlock)) hx = true; else x += sx; }
    if (!hy) { if (boxHits(x, y + sy, h, withBlock)) hy = true; else y += sy; }
  }
  if (hitX) *hitX = hx; if (hitY) *hitY = hy;
}

// Kann der Block die Platte ueberhaupt noch erreichen? (Breitensuche ueber Blockfelder;
// fuer einen Schub muss das Feld dahinter begehbar und das Feld davor frei sein)
static bool blockCanReachPlate(int si, int sj) {
  static uint8_t seen[N][N]; memset(seen, 0, sizeof seen);
  int q[N * N][2], qh = 0, qt = 0;
  q[qt][0] = si; q[qt][1] = sj; qt++; seen[sj][si] = 1;
  const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  while (qh < qt) {
    int i = q[qh][0], j = q[qh][1]; qh++;
    if (i == PLATE_I && j == PLATE_J) return true;
    for (auto &d : D) {
      int ni = i + d[0], nj = j + d[1], bi = i - d[0], bj = j - d[1];
      if (!walkable(tileAt(ni, nj)) || !walkable(tileAt(bi, bj)) || seen[nj][ni]) continue;
      seen[nj][ni] = 1; q[qt][0] = ni; q[qt][1] = nj; qt++;
    }
  }
  return false;
}

static void tryPush(int di, int dj) {
  int ni = block.i + di, nj = block.j + dj;
  if (!walkable(tileAt(ni, nj))) return;
  if (slime.state < 3 && cellOf(slime.x) == ni && cellOf(slime.y) == nj) return;
  block.fi = block.i; block.fj = block.j; block.i = ni; block.j = nj;
  block.t = 0; block.moving = true;
  float bx, by; blockPos(bx, by);
  spawn(bx - di * 21, by + 18, 5, C(0x8a8199), 60, 0.35f);
}

// ---------------------------------------------------------------- Neustart
static void resetRoom(uint32_t now) {
  mode = M_PLAY; startMs = now; t_ = 0; freeze = 0; shake = 0;
  memset(&hero, 0, sizeof hero);
  hero.x = cellC(HERO_I0); hero.y = cellC(HERO_J0); hero.hp = 6; hero.face = F_UP; hero.atkT = -1;
  memset(&slime, 0, sizeof slime);
  slime.x = cellC(SLIME_I0); slime.y = cellC(SLIME_J0); slime.hp = 2; slime.t = 1.2f;
  memset(&block, 0, sizeof block);
  block.i = block.fi = BLOCK_I0; block.j = block.fj = BLOCK_J0;
  heartDrop.on = false;
  doorOpen = 0; doorWanted = false; msgT = 0;
  for (auto &p : parts) p.life = 0;
}

void begin(uint16_t *fbuf, uint16_t *bgbuf, uint32_t seed) {
  fb = fbuf; bg = bgbuf; rng = seed ? seed : 1;
  buildArt();
  buildMap();
  buildBackground();
  resetRoom(0);
  hintT = 6.0f;
}

// ---------------------------------------------------------------- Eingabe
static bool wasDown = false;
static uint32_t downMs = 0;
static int gesture = 0;   // 0 laufen, 1 Hieb, 2 Gegner anvisiert, 3 Neustart, 4 noch offen
static int downX = 0, downY = 0;

static void startAttack(float ang) {
  if (hero.cool > 0 || hero.kbT > 0) return;
  hero.atkT = 0; hero.atkAng = ang; hero.atkHit = false; hero.cool = 0.34f;
  float c = cosf(ang), s = sinf(ang);
  hero.face = fabsf(c) > fabsf(s) ? (c > 0 ? F_RIGHT : F_LEFT) : (s > 0 ? F_DOWN : F_UP);
  hero.hasTarget = false; hero.chase = false;
}

static void input(uint32_t now, bool down, int x, int y) {
  if (down && !wasDown) {
    downMs = now;
    if (mode != M_PLAY) { gesture = 3; }
    else {
      if (hintT > 1.0f) hintT = 1.0f;
      float dhx = x - hero.x, dhy = y - (hero.y - 12);
      float dsx = x - slime.x, dsy = y - (slime.y - 9);
      bool slimeHere = slime.state < 3 && dsx * dsx + dsy * dsy < 66 * 66;
      if (slimeHere) {
        // Gegner angetippt: in Reichweite sofort zuschlagen, sonst hinlaufen
        float dx = slime.x - hero.x, dy = slime.y - hero.y;
        if (dx * dx + dy * dy < 64 * 64) { gesture = 1; startAttack(atan2f(dy, dx)); }
        else { gesture = 2; hero.chase = true; hero.hasTarget = false; hero.following = false; }
      } else if (dhx * dhx + dhy * dhy < 80 * 80) {
        // nah am Held: kurzer Tipp = Hieb, liegen lassen = laufen (entscheidet sich gleich)
        gesture = 4; downX = x; downY = y;
        hero.hasTarget = false; hero.chase = false;
      } else {
        gesture = 0; hero.hasTarget = true; hero.following = true; hero.chase = false;
        hero.tx = x; hero.ty = y;
      }
    }
  } else if (down && wasDown) {
    if (gesture == 4) {
      int mx = x - downX, my = y - downY;
      if (now - downMs > 170 || mx * mx + my * my > 30 * 30) gesture = 0;
    }
    if (gesture == 0) { hero.tx = x; hero.ty = y; hero.hasTarget = true; hero.following = true; }
    else if (gesture == 2 && now - downMs > 300) {
      // Finger bleibt liegen und wandert weg -> normales Laufen
      float dsx = x - slime.x, dsy = y - slime.y;
      if (dsx * dsx + dsy * dsy > 100 * 100) { gesture = 0; hero.chase = false; hero.hasTarget = true; hero.following = true; hero.tx = x; hero.ty = y; }
    }
  } else if (!down && wasDown) {
    if (gesture == 3) resetRoom(now);
    else if (gesture == 4) {
      float dhx = downX - hero.x, dhy = downY - (hero.y - 12);
      float ang = (dhx * dhx + dhy * dhy < 14 * 14) ? (hero.face == F_UP ? -1.5708f : hero.face == F_DOWN ? 1.5708f
                   : hero.face == F_LEFT ? 3.1416f : 0.0f) : atan2f(dhy, dhx);
      startAttack(ang);
    }
    else if (gesture == 0) {
      hero.following = false;
      if (now - downMs > 260) hero.hasTarget = false;  // gehalten: loslassen = stehen bleiben
    }
  }
  wasDown = down;
}

// ---------------------------------------------------------------- Update
static void hurtHero(float fromX, float fromY) {
  if (hero.inv > 0 || mode != M_PLAY) return;
  hero.hp -= 1; hero.inv = 1.1f; hero.kbT = 0.16f;
  float dx = hero.x - fromX, dy = hero.y - fromY, d = sqrtf(dx * dx + dy * dy) + 0.001f;
  hero.kbx = dx / d * 420; hero.kby = dy / d * 420;
  hero.hasTarget = hero.chase = false; hero.atkT = -1;
  shake = 0.18f; freeze = 0.06f;
  if (hero.hp <= 0) { hero.hp = 0; mode = M_DEAD; hero.deadT = 0; endMs = lastMs; endT = t_; say("AUTSCH!", 99); }
}

static void updateHero(float dt) {
  if (hero.cool > 0) hero.cool -= dt;
  if (hero.inv > 0) hero.inv -= dt;
  float vx = 0, vy = 0;
  if (hero.kbT > 0) {
    hero.kbT -= dt;
    moveBox(hero.x, hero.y, hero.kbx * dt, hero.kby * dt, HB, true, nullptr, nullptr);
    hero.moving = false; return;
  }
  if (hero.atkT >= 0) {
    hero.atkT += dt;
    if (hero.atkT > 0.22f) hero.atkT = -1;
    hero.moving = false;
    return;
  }
  if (hero.chase) {
    if (slime.state >= 3) hero.chase = false;
    else {
      float dx = slime.x - hero.x, dy = slime.y - hero.y;
      if (dx * dx + dy * dy < 58 * 58) { startAttack(atan2f(dy, dx)); return; }
      hero.tx = slime.x; hero.ty = slime.y;
    }
  }
  if (hero.hasTarget || hero.chase) {
    float dx = hero.tx - hero.x, dy = hero.ty - (hero.y - 12), d = sqrtf(dx * dx + dy * dy);
    if (d < 6) { if (!hero.following) hero.hasTarget = false; }
    else {
      const float SPEED = 170;
      float sp = d < 36 ? SPEED * (0.4f + 0.6f * d / 36) : SPEED;
      vx = dx / d * sp; vy = dy / d * sp;
    }
  }
  hero.moving = vx != 0 || vy != 0;
  if (!hero.moving) { hero.pushT = 0; return; }
  if (fabsf(vx) > fabsf(vy)) hero.face = vx > 0 ? F_RIGHT : F_LEFT; else hero.face = vy > 0 ? F_DOWN : F_UP;

  bool hx, hy;
  moveBox(hero.x, hero.y, vx * dt, vy * dt, HB, true, &hx, &hy);

  // Schieben: klar in eine Richtung gegen den Block laufen
  int di = 0, dj = 0;
  if (fabsf(vx) > fabsf(vy) * 1.4f && hx) di = vx > 0 ? 1 : -1;
  else if (fabsf(vy) > fabsf(vx) * 1.4f && hy) dj = vy > 0 ? 1 : -1;
  bool pushing = false;
  if ((di || dj) && !block.moving && block.sinkT <= 0) {
    float bx, by; blockPos(bx, by);
    float gx = (bx - hero.x) * di, gy = (by - hero.y) * dj;   // Abstand in Schubrichtung
    if (di && fabsf(hero.y - by) < 24 && gx > HB + BH - 2 && gx < HB + BH + 5) pushing = true;
    if (dj && fabsf(hero.x - bx) < 24 && gy > HB + BH - 2 && gy < HB + BH + 5) pushing = true;
    if (pushing) {
      // sanft auf die Blockmitte ausrichten, damit das Schieben nicht fummelig ist
      float k = fminf(1, dt * 8);
      if (di) moveBox(hero.x, hero.y, 0, (by - hero.y) * k, HB, false, nullptr, nullptr);
      else moveBox(hero.x, hero.y, (bx - hero.x) * k, 0, HB, false, nullptr, nullptr);
    }
  }
  if (pushing && di == hero.pushDi && dj == hero.pushDj) {
    hero.pushT += dt;
    if (hero.pushT > 0.28f) { tryPush(di, dj); hero.pushT = 0; }
  } else { hero.pushT = pushing ? dt : 0; hero.pushDi = di; hero.pushDj = dj; }

  hero.walkT += dt;
  if (hero.walkT > 0.14f) { hero.walkT = 0; hero.frame ^= 1; }
}

static void updateAttackHit() {
  if (hero.atkT < 0.03f || hero.atkT > 0.17f || hero.atkHit) return;
  if (slime.state >= 2) return;
  float dx = slime.x - hero.x, dy = slime.y - hero.y, d = sqrtf(dx * dx + dy * dy);
  if (d > 75) return;
  float a = atan2f(dy, dx) - hero.atkAng;
  while (a > 3.1416f) a -= 6.2832f; while (a < -3.1416f) a += 6.2832f;
  if (fabsf(a) > 1.45f && d > 27) return;
  hero.atkHit = true;
  slime.hp--; slime.flash = 0.18f; freeze = 0.07f; shake = 0.1f;
  float kd = d + 0.001f;
  slime.fx = slime.x; slime.fy = slime.y;
  slime.tx = slime.x + dx / kd * 80; slime.ty = slime.y + dy / kd * 80;
  slime.state = 2; slime.t = 0; slime.dur = 0.2f; slime.hz = 0;
  spawn(slime.x, slime.y - 12, 6, colorOf('P'), 100, 0.3f);
}

static void updateSlime(float dt) {
  if (slime.flash > 0) slime.flash -= dt;
  switch (slime.state) {
    case 0: {  // warten und wabbeln
      slime.t -= dt;
      slime.anim = ((int)(t_ * 3.2f)) & 1;
      if (slime.t <= 0) {
        float dx = hero.x - slime.x, dy = hero.y - slime.y, d = sqrtf(dx * dx + dy * dy);
        float ang = (d < 300 && mode == M_PLAY) ? atan2f(dy, dx) + frand(-0.35f, 0.35f) : frand(0, 6.2832f);
        float len = fminf(70, d > 45 ? d - 18 : 70);
        slime.fx = slime.x; slime.fy = slime.y;
        slime.tx = slime.x + cosf(ang) * len; slime.ty = slime.y + sinf(ang) * len;
        slime.state = 1; slime.t = 0; slime.dur = 0.42f;
      }
    } break;
    case 1:    // Hopser
    case 2: {  // vom Schwert weggeschleudert
      float prevX = slime.x, prevY = slime.y;
      slime.t += dt;
      float p = fminf(1, slime.t / slime.dur);
      float e = slime.state == 2 ? 1 - (1 - p) * (1 - p) : p;
      float wx = slime.fx + (slime.tx - slime.fx) * e, wy = slime.fy + (slime.ty - slime.fy) * e;
      slime.x = prevX; slime.y = prevY;
      moveBox(slime.x, slime.y, wx - prevX, wy - prevY, HB - 1, true, nullptr, nullptr);
      slime.hz = slime.state == 1 ? sinf(p * 3.1416f) * 24 : 0;
      slime.anim = slime.state == 1 ? (p < 0.15f || p > 0.85f ? 1 : 0) : 1;
      if (p >= 1) {
        if (slime.hp <= 0) {
          slime.state = 3; slime.t = 0;
          spawn(slime.x, slime.y - 12, 14, colorOf('p'), 160, 0.45f);
          spawn(slime.x, slime.y - 12, 6, 0xFFFF, 90, 0.3f);
          if (hero.hp < 6) heartDrop = {true, slime.x, slime.y, 0};
        } else {
          slime.state = 0; slime.t = frand(0.5f, 1.1f);
          if (slime.flash <= 0) spawn(slime.x, slime.y + 9, 3, C(0x9e2f55), 45, 0.25f);
        }
      }
    } break;
    case 3:
      slime.t += dt;
      if (slime.t > 0.45f) slime.state = 4;
      break;
    default: break;
  }
  // Beruehrung tut weh (nicht in der Luft, nicht wenn er gerade getroffen wurde)
  if (slime.state <= 1 && slime.hz < 9) {
    float dx = hero.x - slime.x, dy = hero.y - slime.y;
    if (dx * dx + dy * dy < 28 * 28) hurtHero(slime.x, slime.y);
  }
}

static void updateBlockAndDoor(float dt) {
  if (block.moving) {
    block.t += dt / 0.2f;
    if (block.t >= 1) {
      block.t = 1; block.moving = false;
      if (block.i == PLATE_I && block.j == PLATE_J) {}
      else if (!blockCanReachPlate(block.i, block.j)) block.stuckT = 1.4f;
    }
  }
  if (block.stuckT > 0) {
    block.stuckT -= dt;
    if (block.stuckT <= 0) { block.sinkT = 0.6f; say("DER BLOCK SITZT FEST...", 1.8f); }
  }
  if (block.sinkT > 0) {
    float bx, by; blockPos(bx, by);
    block.sinkT -= dt;
    if (((int)(block.sinkT * 30)) % 3 == 0) spawn(bx, by + 15, 1, C(0x8a8199), 45, 0.3f);
    if (block.sinkT <= 0) {
      // erst zurueck, wenn der Startplatz frei ist
      float sx = cellC(BLOCK_I0), sy = cellC(BLOCK_J0);
      if (fabsf(hero.x - sx) < HB + BH && fabsf(hero.y - sy) < HB + BH) block.sinkT = 0.05f;
      else {
        block.i = block.fi = BLOCK_I0; block.j = block.fj = BLOCK_J0; block.moving = false;
        spawn(sx, sy, 10, C(0xc4bcd0), 90, 0.4f);
      }
    }
  }
  bool want = plateWeighted();
  if (want && !doorWanted) { shake = 0.12f; say("KLACK! DIE TÜR IST OFFEN", 2.2f); }
  if (!want && doorWanted && doorOpen > 0.5f) say("DIE TÜR GEHT ZU...", 1.5f);
  doorWanted = want;
  // Held steht im Tuerfeld: Tuer bleibt offen, sonst wuerde sie ihn einklemmen
  bool heroInDoor = cellOf(hero.x) == DOOR_I && cellOf(hero.y - HB) <= DOOR_J;
  if (doorWanted || heroInDoor) doorOpen = fminf(1, doorOpen + dt / 0.45f);
  else doorOpen = fmaxf(0, doorOpen - dt / 0.3f);
}

static void updateMisc(float dt) {
  for (auto &p : parts) if (p.life > 0) {
    p.life -= dt; p.x += p.vx * dt; p.y += p.vy * dt; p.vx *= 0.9f; p.vy *= 0.9f;
  }
  if (heartDrop.on) {
    heartDrop.t += dt;
    float dx = hero.x - heartDrop.x, dy = hero.y - heartDrop.y;
    if (dx * dx + dy * dy < 32 * 32 && heartDrop.t > 0.3f) {
      hero.hp = hero.hp + 2 > 6 ? 6 : hero.hp + 2; heartDrop.on = false;
      spawn(hero.x, hero.y - 24, 8, colorOf('r'), 90, 0.4f);
    }
  }
  if (msgT > 0 && msgT < 90) msgT -= dt;
  if (hintT > 0) hintT -= dt;
  if (shake > 0) shake -= dt;
}

// ---------------------------------------------------------------- Zeichnen
static void drawShadow(float x, float y, int w) {
  int x0 = (int)x - w / 2, y0 = (int)y + 9;
  for (int j = 0; j < 9; j++) {
    float ry = (j - 4.0f) / 4.5f; int half = (int)(w / 2 * sqrtf(fmaxf(0, 1 - ry * ry)));
    for (int i = -half; i < half; i++) {
      int X = x0 + w / 2 + i, Y = y0 + j;
      if ((unsigned)X < (unsigned)SCR && (unsigned)Y < (unsigned)SCR) fb[Y * SCR + X] = mix(fb[Y * SCR + X], 0, 0.4f);
    }
  }
}

static void drawHero(int ox, int oy) {
  if (mode == M_DEAD) {
    // dreht sich und sinkt um
    static const Face spin[4] = {F_DOWN, F_LEFT, F_UP, F_RIGHT};
    Face f = hero.deadT < 0.8f ? spin[((int)(hero.deadT * 10)) & 3] : F_DOWN;
    const Art &a = f == F_UP ? aHeroUp[0] : (f == F_DOWN ? aHeroDown[0] : aHeroSide[0]);
    drawShadow(hero.x + ox, hero.y + oy, 33);
    if (hero.deadT < 1.2f) drawArt(a, (int)hero.x - 24 + ox, (int)hero.y - 33 + oy, f == F_LEFT);
    return;
  }
  if (hero.inv > 0 && ((int)(hero.inv * 16)) & 1) { drawShadow(hero.x + ox, hero.y + oy, 33); return; }
  int fr = hero.moving ? hero.frame : 0;
  const Art *a = &aHeroDown[fr]; bool flip = false;
  if (hero.face == F_UP) a = &aHeroUp[fr];
  else if (hero.face == F_RIGHT) a = &aHeroSide[fr];
  else if (hero.face == F_LEFT) { a = &aHeroSide[fr]; flip = true; }
  int bob = (hero.moving && fr) ? -SC : 0;
  drawShadow(hero.x + ox, hero.y + oy, 33);
  drawArt(*a, (int)hero.x - 24 + ox, (int)hero.y - 33 + bob + oy, flip);
}

// Schwert: Klinge schwingt ueber 140 Grad, dahinter ein heller Bogen
static void drawSword(int ox, int oy) {
  if (hero.atkT < 0) return;
  float p = fminf(1, hero.atkT / 0.13f);
  float a0 = hero.atkAng - 1.22f, a1 = hero.atkAng + 1.22f;
  float cur = a0 + (a1 - a0) * p;
  float cx = hero.x + ox, cy = hero.y - 12 + oy;
  const uint16_t trail = C(0xfff3d6), steel = C(0xe6edf5), steelD = C(0x8c97a8), hilt = C(0x7a5236);
  if (hero.atkT < 0.2f) {
    float fade = hero.atkT < 0.13f ? 0 : (hero.atkT - 0.13f) / 0.07f;
    // gegen den Hintergrund mischen (nicht gegen fb), sonst wird der Bogen fleckig
    for (float a = a0; a <= cur; a += 0.04f)
      for (float r = 42; r <= 57; r += 2) {
        int X = (int)(cx + cosf(a) * r), Y = (int)(cy + sinf(a) * r);
        if ((unsigned)X >= (unsigned)SCR || (unsigned)Y >= (unsigned)SCR) continue;
        float edge = (r < 45 || r > 54) ? 0.35f : 0.0f;
        fat(X, Y, mix(trail, bg[Y * SCR + X], fminf(1, 0.2f + edge + fade * 0.8f)));
      }
  }
  if (hero.atkT < 0.17f) {
    float c = cosf(cur), s = sinf(cur);
    for (float r = 9; r <= 60; r += 2) {
      int X = (int)(cx + c * r), Y = (int)(cy + s * r);
      if (r < 18) fat(X, Y, hilt);
      else { fat(X + 2, Y + 2, steelD); fat(X, Y, steel); }
    }
    fat((int)(cx + c * 18 - s * 6), (int)(cy + s * 18 + c * 6), C(0xd1a94e));  // Parierstange
    fat((int)(cx + c * 18 + s * 6), (int)(cy + s * 18 - c * 6), C(0xd1a94e));
  }
}

static void drawSlime(int ox, int oy) {
  if (slime.state >= 3) return;
  float sh = slime.hz;
  drawShadow(slime.x + ox, slime.y + oy, (int)(36 - sh * 0.5f));
  drawArt(aSlime[slime.anim], (int)slime.x - 24 + ox, (int)(slime.y - 33 - sh) + oy, false, slime.flash > 0);
}

static void drawBlock(int ox, int oy) {
  float bx, by; blockPos(bx, by);
  if (block.sinkT > 0) {
    // versinken: nur der obere Teil bleibt sichtbar, faellt nach unten weg
    float k = 1 - block.sinkT / 0.6f; int cut = (int)(k * 48);
    const Art &a = aBlock;
    for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++) {
      int Y = (int)by - 24 + j * SC + cut;
      if (Y + SC > (int)by + 24) continue;
      uint16_t c = a.px[j * 16 + i]; if (c == TRANSP) continue;
      rect((int)bx - 24 + i * SC + ox, Y + oy, SC, SC, c);
    }
    return;
  }
  int jig = block.stuckT > 0 ? (((int)(block.stuckT * 30)) & 1) * SC : 0;
  darkenRect((int)bx - 21 + ox, (int)by + 21 + oy, 48, 6, 0.35f);
  drawArt(aBlock, (int)bx - 24 + ox + jig, (int)by - 24 + oy);
}

static void drawDoor(int ox, int oy) {
  int x0 = OFF + DOOR_I * TILE + ox, y0 = OFF + DOOR_J * TILE + oy;
  auto ar = [&](int ax, int ay, int aw, int ah, uint16_t c) { rect(x0 + ax * SC, y0 + ay * SC, aw * SC, ah * SC, c); };
  const uint16_t dark = C(0x07050b), arch = C(0x6f5f86), archD = C(0x3e3150), iron = C(0x8a90a0), ironD = C(0x4b4f5c);
  ar(0, 0, 16, 16, archD);
  ar(2, 3, 12, 13, dark);
  ar(1, 0, 14, 3, arch); ar(1, 2, 14, 1, archD);
  int bars = (int)(13 * (1 - doorOpen) + 0.5f);      // Gitter faehrt nach oben weg
  for (int b = 0; b < 4; b++) { ar(3 + b * 3, 3, 1, bars, iron); }
  if (bars > 1) { ar(2, 3 + bars - 1, 12, 1, ironD); ar(2, 6, 12, 1, ironD); }
  if (doorOpen > 0.9f) {  // Licht von draussen
    for (int y = 0; y < 13; y++) for (int x = 0; x < 12; x++)
      if ((x + y + (int)(t_ * 4)) % 5 == 0) ar(2 + x, 3 + y, 1, 1, C(0x26203a));
  }
}

static void drawTorches(int ox, int oy) {
  for (auto &tc : TORCHES) {
    int x0 = OFF + tc[0] * TILE + ox, y0 = OFF + tc[1] * TILE + oy;
    drawArt(aTorch[((int)(t_ * 7) + tc[0]) & 1], x0 - 1, y0);
  }
}

static void drawHUD() {
  for (int k = 0; k < 3; k++) {
    int v = hero.hp - k * 2;
    const Art &a = v >= 2 ? aHeartFull : (v == 1 ? aHeartHalf : aHeartEmpty);
    drawArt(a, 233 - 40 + k * 28, 408);
  }
}

static void render() {
  int ox = 0, oy = 0;
  if (shake > 0) { ox = (int)(rnd() % 5) - 2; oy = (int)(rnd() % 5) - 2; ox &= ~1; oy &= ~1; }
  // Hintergrund (mit Wackeln verschoben)
  for (int y = 0; y < SCR; y++) {
    int sy = y - oy; if (sy < 0) sy = 0; if (sy >= SCR) sy = SCR - 1;
    uint16_t *d = fb + y * SCR; const uint16_t *s = bg + sy * SCR;
    if (ox >= 0) { memcpy(d + ox, s, (SCR - ox) * 2); for (int x = 0; x < ox; x++) d[x] = 0; }
    else { memcpy(d, s - ox, (SCR + ox) * 2); for (int x = SCR + ox; x < SCR; x++) d[x] = 0; }
  }
  drawArt(plateWeighted() ? aPlateDown : aPlateUp, OFF + PLATE_I * TILE + ox, OFF + PLATE_J * TILE + oy);
  drawDoor(ox, oy);
  drawTorches(ox, oy);

  // Figuren nach y sortiert (wer weiter unten steht, wird spaeter gezeichnet)
  float bx, by; blockPos(bx, by);
  struct Ent { float y; int k; } ents[4]; int ne = 0;
  ents[ne++] = {by + 8, 0};
  ents[ne++] = {slime.y, 1};
  ents[ne++] = {hero.y, 2};
  if (heartDrop.on) ents[ne++] = {heartDrop.y, 3};
  for (int a = 1; a < ne; a++) for (int b = a; b > 0 && ents[b].y < ents[b - 1].y; b--) { Ent t = ents[b]; ents[b] = ents[b - 1]; ents[b - 1] = t; }
  for (int e = 0; e < ne; e++) {
    switch (ents[e].k) {
      case 0: drawBlock(ox, oy); break;
      case 1: drawSlime(ox, oy); break;
      case 2: drawHero(ox, oy); drawSword(ox, oy); break;
      case 3: {
        int hop = (int)(fabsf(sinf(heartDrop.t * 5)) * 9 * fmaxf(0, 1 - heartDrop.t));
        bool blink = heartDrop.t > 6 && ((int)(heartDrop.t * 8) & 1);
        if (!blink) drawArt(aHeartFull, (int)heartDrop.x - 12 + ox, (int)heartDrop.y - 21 - hop + oy);
      } break;
    }
  }
  for (auto &p : parts) if (p.life > 0) fat((int)p.x + ox, (int)p.y + oy, p.c);

  drawHUD();

  if (hintT > 0 && mode == M_PLAY) {
    uint16_t c = hintT < 1 ? mix(C(0xf4efe6), C(0x5b4e6e), 1 - hintT) : C(0xf4efe6);
    text("HALTEN: LAUFEN", 178, 2, c);
    text("TIPPEN AM HELD: SCHLAG", 202, 2, c);
    text("BRING DEN BLOCK AUF DIE PLATTE", 258, 2, c);
  }
  if (mode != M_PLAY) {
    float k = fminf(0.6f, (t_ - endT) * 1.5f);
    if (mode == M_DEAD) k = fminf(0.6f, (hero.deadT - 0.8f) * 1.2f);
    if (k > 0) {
      for (int i = 0; i < SCR * SCR; i++) fb[i] = mix(fb[i], 0, k);
      char buf[32]; uint32_t s = (endMs - startMs) / 1000;
      if (mode == M_WIN) {
        text("GESCHAFFT!", 190, 5, C(0xffe27a));
        snprintf(buf, sizeof buf, "ZEIT %u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
        text(buf, 250, 3, C(0xf4efe6));
      } else {
        text("AUTSCH!", 200, 5, C(0xff8fa0));
      }
      text("TIPPEN = NOCHMAL", 310, 2, C(0xb9b0c8));
    }
  } else if (msgT > 0) {
    text(msg, 118, 2, C(0xf4efe6));
  }
}

// ---------------------------------------------------------------- Takt
void tick(uint32_t now, bool down, int x, int y) {
  if (!lastMs) { lastMs = now; startMs = now; }
  float dt = (now - lastMs) / 1000.0f; lastMs = now;
  if (dt > 0.05f) dt = 0.05f;
  input(now, down, x, y);
  t_ += dt;
  if (freeze > 0) { freeze -= dt; if (shake > 0) shake -= dt; }
  else if (mode == M_PLAY) {
    updateHero(dt);
    updateAttackHit();
    updateSlime(dt);
    updateBlockAndDoor(dt);
    updateMisc(dt);
    // durch die offene Tuer gegangen?
    if (doorOpen >= 1 && cellOf(hero.x) == DOOR_I && hero.y < cellC(DOOR_J) + 15) {
      mode = M_WIN; endMs = now; endT = t_; hero.atkT = -1;
      spawn(hero.x, hero.y - 15, 16, C(0xffe27a), 180, 0.6f);
    }
  } else {
    if (mode == M_DEAD) hero.deadT += dt;
    updateMisc(dt);
    if (mode == M_WIN) { hero.moving = false; }
  }
  render();
}

Debug debug() {
  Debug d;
  d.hx = hero.x; d.hy = hero.y; d.hp = hero.hp; d.slimeAlive = slime.state < 3;
  d.sx = slime.x; d.sy = slime.y; d.bi = block.i; d.bj = block.j; d.blockMoving = block.moving || block.sinkT > 0;
  d.doorOpen = doorOpen >= 1; d.mode = mode;
  d.plateX = cellC(PLATE_I); d.plateY = cellC(PLATE_J); d.doorX = cellC(DOOR_I); d.doorY = cellC(DOOR_J);
  d.blockStartX = cellC(BLOCK_I0); d.blockStartY = cellC(BLOCK_J0);
  return d;
}

}  // namespace kw
