// Kleine Welt - Prototyp: vier verbundene runde Raeume.
//   1 Eingang     - Schleimlinge besiegen -> Tuer auf
//   2 Block       - Steinblock auf die Platte schieben
//   3 Zwei Platten- zwei Bloecke, zwei Platten, Schleimlinge
//   4 Schatz      - Truhe oeffnen = Ende des Prototyps
//
// Steuerung mit festen Zonen am Rand (Daumen verdecken den Held nicht):
//   links unten  : Steuerkreuz (analog, Finger aufsetzen und schieben)
//   rechts unten : Schwert-Knopf (Hieb in Blickrichtung, zielt leicht auf Gegner)
//   gegen einen Block laufen -> Block rutscht eine Kachel weiter
//
// Alle Grafik ist eigene Pixelkunst (16x16, 3-fach skaliert) bzw. wird im Code
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

// ---------------------------------------------------------------- Raeume
constexpr int TILE = 16 * SC, N = 11, OFF = (SCR - N * TILE) / 2;  // 11x11 Kacheln a 48 px, Mitte = (5,5)
constexpr float HB = 15;   // halbe Breite Held/Schleimling
constexpr float BH = 23;   // halbe Breite Block
enum Tile : uint8_t { T_WALL, T_FLOOR, T_PILLAR, T_DOOR, T_PLATE, T_CHEST };
static inline int cellOf(float p) { return (int)floorf((p - OFF) / TILE); }
static inline float cellC(int i) { return OFF + i * TILE + TILE / 2.0f; }

// Tueren sitzen immer in der Mitte oben (Norden) bzw. unten (Sueden)
static const int DN_I = 5, DN_J = 1, DS_I = 5, DS_J = 9;
enum Cond : uint8_t { C_NONE, C_ENEMIES, C_PLATES };
struct Cell { int8_t i, j; };
struct RoomDef {
  const char *name;
  int8_t north, south;          // Zielraum oder -1
  Cond cond;                    // was die Nordtuer oeffnet
  Cell pillars[6]; uint8_t nPillars;
  Cell plates[2]; uint8_t nPlates;
  Cell blocks[2]; uint8_t nBlocks;
  Cell slimes[4]; uint8_t nSlimes;
  bool chest; Cell chestAt;
  uint32_t floorA, floorB;      // Bodenfarben (jeder Raum etwas anders)
};
static const RoomDef ROOMS[] = {
  {"EINGANG", 1, -1, C_ENEMIES, {{4, 2}, {6, 2}}, 2, {}, 0, {}, 0, {{3, 4}, {7, 4}}, 2, false, {0, 0},
   0x5b4e6e, 0x564a68},
  {"DER BLOCK", 2, 0, C_PLATES, {{4, 2}, {6, 2}, {3, 6}}, 3, {{6, 7}}, 1, {{3, 4}}, 1, {{6, 5}}, 1, false, {0, 0},
   0x4f5a6e, 0x4a5468},
  {"ZWEI PLATTEN", 3, 1, C_PLATES, {{4, 2}, {6, 2}, {5, 6}}, 3, {{2, 4}, {8, 5}}, 2, {{4, 5}, {6, 4}}, 2,
   {{3, 7}, {7, 7}}, 2, false, {0, 0}, 0x5e4d5c, 0x584857},
  {"SCHATZKAMMER", -1, 2, C_NONE, {{3, 4}, {7, 4}, {3, 6}, {7, 6}}, 4, {}, 0, {}, 0, {}, 0, true, {5, 4},
   0x625a44, 0x5c543f},
};
constexpr int NROOMS = sizeof(ROOMS) / sizeof(ROOMS[0]);
static const int TORCHES[2][2] = {{3, 1}, {7, 1}};

static int room = 0;
static uint8_t map_[N][N];
static inline uint8_t tileAt(int i, int j) {
  if (i < 0 || j < 0 || i >= N || j >= N) return T_WALL;
  return map_[j][i];
}
static inline bool walkable(uint8_t t) { return t == T_FLOOR || t == T_PLATE; }

static void buildMap(const RoomDef &r) {
  for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
    int di = i - 5, dj = j - 5;
    map_[j][i] = di * di + dj * dj <= 13 ? T_FLOOR : T_WALL;   // Radius ~3,6 Kacheln
  }
  if (r.north >= 0) map_[DN_J][DN_I] = T_DOOR;
  if (r.south >= 0) map_[DS_J][DS_I] = T_DOOR;
  for (int k = 0; k < r.nPlates; k++) map_[r.plates[k].j][r.plates[k].i] = T_PLATE;
  for (int k = 0; k < r.nPillars; k++) map_[r.pillars[k].j][r.pillars[k].i] = T_PILLAR;
  if (r.chest) map_[r.chestAt.j][r.chestAt.i] = T_CHEST;
}

static uint32_t hash2(int a, int b, int c) {
  uint32_t h = (uint32_t)a * 374761393u + (uint32_t)b * 668265263u + (uint32_t)c * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16);
}

// Raum einmal vorrendern: Boden, Waende, Saeulen, Licht, runder Rand
static void buildBackground(const RoomDef &r) {
  uint16_t *save = fb; fb = bg;
  const uint16_t fBase[2] = {C(r.floorA), C(r.floorB)};
  const uint16_t fL = mix(fBase[0], 0xFFFF, 0.12f), fD = mix(fBase[0], 0, 0.18f), fS = mix(fBase[0], 0, 0.38f);
  const uint16_t wTop = C(0x241b2f), wTop2 = C(0x2c2238);
  const uint16_t brick = C(0x4a3b5f), brickL = C(0x5d4b75), brickD = C(0x3a2e4b), mortar = C(0x1d1628);
  for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
    int x0 = OFF + i * TILE, y0 = OFF + j * TILE;
    uint8_t t = map_[j][i];
    bool wall = t == T_WALL || t == T_DOOR;
    uint8_t below = tileAt(i, j + 1);
    bool face = wall && below != T_WALL && below != T_DOOR;
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
      uint16_t c;
      uint32_t h = hash2(i * 16 + x, j * 16 + y, 7 + room);
      if (wall) {
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
        uint16_t base = fBase[hash2(i, j, 1 + room) & 1];
        bool split = hash2(i, j, 2 + room) & 1;
        bool seam = (x == 15 || y == 15) || (x == 7 && y < 8 && split);
        if (seam) c = fS;
        else if (x == 0 || y == 0 || (x == 8 && y < 8 && split)) c = fL;
        else if ((h & 31) == 0) c = fD;
        else if ((h & 31) == 1) c = fL;
        else c = base;
        uint8_t up = tileAt(i, j - 1);
        if ((up == T_WALL || up == T_DOOR) && y < 3) c = mix(c, 0, 0.45f - y * 0.12f);
      }
      rect(x0 + x * SC, y0 + y * SC, SC, SC, c);
    }
    if (t == T_PILLAR) {
      darkenRect(x0 + 9, y0 + 33, 42, 15, 0.35f);
      drawArt(aPillar, x0, y0);
    }
  }
  for (int y = 0; y < SCR; y++) for (int x = 0; x < SCR; x++) {
    float dx = x - 233.0f, dy = y - 233.0f, rr = sqrtf(dx * dx + dy * dy);
    uint16_t c = bg[y * SCR + x];
    float k = 0.22f * (rr / 233.0f) * (rr / 233.0f);
    if (rr > 212) k += (rr - 212) / 21.0f;
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

// Truhe (geschlossen / offen)
static Art aChest[2];
static void buildChestArt() {
  const uint16_t wood = C(0x9a6236), woodL = C(0xc4874e), woodD = C(0x5e3a20), gold = C(0xf0cf7c), goldD = C(0xb08a3a);
  for (int o = 0; o < 2; o++) {
    Art &a = aChest[o]; a.w = a.h = 16;
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
      uint16_t c = TRANSP;
      int top = o ? 6 : 3;
      if (x >= 1 && x <= 14 && y >= top && y <= 14) {
        bool edge = x == 1 || x == 14 || y == top || y == 14;
        c = edge ? K_OUT : wood;
        if (!edge && (y == top + 1)) c = woodL;
        if (!edge && (x == 13 || y == 13)) c = woodD;
        if (!edge && (x == 4 || x == 11)) c = goldD;        // Beschlaege
        if (!o && y == 7) c = K_OUT;                          // Deckelfuge
        if (!o && !edge && x >= 7 && x <= 8 && y >= 7 && y <= 9) c = gold;  // Schloss
      }
      if (o && x >= 2 && x <= 13 && y >= 7 && y <= 8) c = K_OUT;          // offen: dunkles Inneres
      if (o && x >= 2 && x <= 13 && y >= 2 && y <= 5) {                   // aufgeklappter Deckel
        c = (y == 2 || x == 2 || x == 13) ? K_OUT : woodD;
      }
      a.px[y * 16 + x] = c;
    }
  }
}

// ---------------------------------------------------------------- Zustand
enum Mode { M_PLAY, M_WIN, M_DEAD };
enum Face { F_DOWN, F_UP, F_RIGHT, F_LEFT };
static Mode mode;
static uint32_t lastMs = 0, startMs = 0, endMs = 0;
static float endT = 0;
static float t_ = 0, freeze = 0, shake = 0;
static float fade = 0;           // >0: Raumwechsel laeuft
static int fadeTo = -1, fadeFrom = -1;

struct Hero {
  float x, y; int hp; Face face;
  bool moving; float walkT; int frame;
  float atkT; float atkAng; bool atkHit[4]; float cool;
  float inv; float kbT, kbx, kby;
  float pushT; int pushDi, pushDj, pushBlock;
  float deadT;
} hero;

struct Slime {
  float x, y; int hp; int state;  // 0 warten, 1 huepfen, 2 getroffen, 3 zerplatzt, 4 weg
  float t, dur; float fx, fy, tx, ty; float hz; float flash; int anim;
};
struct Block {
  int i, j, fi, fj, si, sj; float t; bool moving;
  float stuckT, sinkT;
};
struct Pickup { bool on; float x, y, t; };

// Was ein Raum sich merkt, wenn man ihn verlaesst
struct RoomState {
  bool visited, northOpen, chestOpen;
  Slime slimes[4]; int nSlimes;
  Block blocks[2]; int nBlocks;
};
static RoomState rs[NROOMS];
static RoomState *cur = nullptr;
static Pickup hearts[4];
static float doorN = 0, doorS = 1;   // 0 zu .. 1 offen
static bool northWanted = false;
static char msg[40]; static float msgT = 0;
static float hintT = 0;
static float chestT = -1;            // Truhe wird geoeffnet

struct Particle { float x, y, vx, vy, life, max; uint16_t c; };
static Particle parts[64];
static void spawn(float x, float y, int n, uint16_t c, float sp, float life) {
  for (int k = 0; k < n; k++) for (auto &p : parts) if (p.life <= 0) {
    float a = frand(0, 6.2832f), v = frand(sp * 0.3f, sp);
    p = {x, y, cosf(a) * v, sinf(a) * v, life, life, c}; break;
  }
}
static void say(const char *s, float sec) { strncpy(msg, s, sizeof msg - 1); msg[sizeof msg - 1] = 0; msgT = sec; }

static void blockPos(const Block &b, float &x, float &y) {
  float t = b.moving ? b.t : 1.0f;
  float e = t * t * (3 - 2 * t);
  x = cellC(b.fi) + (cellC(b.i) - cellC(b.fi)) * e;
  y = cellC(b.fj) + (cellC(b.j) - cellC(b.fj)) * e;
}
static bool blockOnCell(int i, int j, int except = -1) {
  for (int k = 0; k < cur->nBlocks; k++) {
    if (k == except) continue;
    const Block &b = cur->blocks[k];
    if (b.sinkT > 0) continue;
    if ((b.i == i && b.j == j) || (b.moving && b.fi == i && b.fj == j)) return true;
  }
  return false;
}
static bool plateWeighted(const Cell &p) {
  for (int k = 0; k < cur->nBlocks; k++) {
    const Block &b = cur->blocks[k];
    if (!b.moving && b.sinkT <= 0 && b.i == p.i && b.j == p.j) return true;
  }
  return mode == M_PLAY && cellOf(hero.x) == p.i && cellOf(hero.y) == p.j;
}

// ---------------------------------------------------------------- Kollision
static bool solidTile(int i, int j) {
  uint8_t t = tileAt(i, j);
  if (t == T_DOOR) return (j == DN_J ? doorN : doorS) < 0.95f;
  return !walkable(t);
}
static bool boxHits(float x, float y, float h, bool withBlocks) {
  int i0 = cellOf(x - h), i1 = cellOf(x + h - 0.01f), j0 = cellOf(y - h), j1 = cellOf(y + h - 0.01f);
  for (int j = j0; j <= j1; j++) for (int i = i0; i <= i1; i++) if (solidTile(i, j)) return true;
  if (withBlocks) for (int k = 0; k < cur->nBlocks; k++) {
    const Block &b = cur->blocks[k];
    if (b.sinkT > 0) continue;
    float bx, by; blockPos(b, bx, by);
    if (fabsf(x - bx) < h + BH && fabsf(y - by) < h + BH) return true;
  }
  return false;
}
static void moveBox(float &x, float &y, float dx, float dy, float h, bool withBlocks, bool *hitX, bool *hitY) {
  // steckt die Box schon in einem Block (z.B. nach einem Schubser), darf sie sich frei herausbewegen
  if (withBlocks && boxHits(x, y, h, true) && !boxHits(x, y, h, false)) withBlocks = false;
  int steps = (int)(fmaxf(fabsf(dx), fabsf(dy)) / 4) + 1;
  float sx = dx / steps, sy = dy / steps;
  bool hx = false, hy = false;
  for (int s = 0; s < steps; s++) {
    if (!hx) { if (boxHits(x + sx, y, h, withBlocks)) hx = true; else x += sx; }
    if (!hy) { if (boxHits(x, y + sy, h, withBlocks)) hy = true; else y += sy; }
  }
  if (hitX) *hitX = hx;
  if (hitY) *hitY = hy;
}

// Kann der Block noch irgendeine Platte erreichen? (Breitensuche; fuer einen Schub
// muss das Feld dahinter begehbar und das Feld davor frei sein)
static bool blockCanReachPlate(int si, int sj) {
  const RoomDef &r = ROOMS[room];
  static uint8_t seen[N][N]; memset(seen, 0, sizeof seen);
  static int q[N * N][2]; int qh = 0, qt = 0;
  q[qt][0] = si; q[qt][1] = sj; qt++; seen[sj][si] = 1;
  const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  while (qh < qt) {
    int i = q[qh][0], j = q[qh][1]; qh++;
    for (int k = 0; k < r.nPlates; k++) if (r.plates[k].i == i && r.plates[k].j == j) return true;
    for (auto &d : D) {
      int ni = i + d[0], nj = j + d[1], bi = i - d[0], bj = j - d[1];
      if (!walkable(tileAt(ni, nj)) || !walkable(tileAt(bi, bj)) || seen[nj][ni]) continue;
      seen[nj][ni] = 1; q[qt][0] = ni; q[qt][1] = nj; qt++;
    }
  }
  return false;
}

static void tryPush(int k, int di, int dj) {
  Block &b = cur->blocks[k];
  int ni = b.i + di, nj = b.j + dj;
  if (!walkable(tileAt(ni, nj)) || blockOnCell(ni, nj, k)) return;
  for (int s = 0; s < cur->nSlimes; s++) {
    const Slime &sl = cur->slimes[s];
    if (sl.state < 3 && cellOf(sl.x) == ni && cellOf(sl.y) == nj) return;
  }
  b.fi = b.i; b.fj = b.j; b.i = ni; b.j = nj; b.t = 0; b.moving = true;
  float bx, by; blockPos(b, bx, by);
  spawn(bx - di * 21, by + 18, 5, C(0x8a8199), 60, 0.35f);
}

// ---------------------------------------------------------------- Raumwechsel
static void initRoomState(int r) {
  const RoomDef &d = ROOMS[r];
  RoomState &s = rs[r];
  memset(&s, 0, sizeof s);
  s.visited = true;
  s.nSlimes = d.nSlimes;
  for (int k = 0; k < d.nSlimes; k++) {
    Slime &sl = s.slimes[k];
    sl.x = cellC(d.slimes[k].i); sl.y = cellC(d.slimes[k].j); sl.hp = 2; sl.t = 1.0f + k * 0.4f;
  }
  s.nBlocks = d.nBlocks;
  for (int k = 0; k < d.nBlocks; k++) {
    Block &b = s.blocks[k];
    b.i = b.fi = b.si = d.blocks[k].i; b.j = b.fj = b.sj = d.blocks[k].j;
  }
  s.northOpen = d.cond == C_NONE;
}

// betritt Raum r; fromSouth = man kommt durch die Suedtuer herein (also von unten)
static void enterRoom(int r, bool fromSouth) {
  room = r;
  if (!rs[r].visited) initRoomState(r);
  cur = &rs[r];
  buildMap(ROOMS[r]);
  buildBackground(ROOMS[r]);
  if (fromSouth) { hero.x = cellC(DS_I); hero.y = cellC(DS_J - 1) + 4; hero.face = F_UP; }
  else { hero.x = cellC(DN_I); hero.y = cellC(DN_J + 1); hero.face = F_DOWN; }
  doorN = cur->northOpen ? 1 : 0; doorS = 1; northWanted = cur->northOpen;
  for (auto &h : hearts) h.on = false;
  for (auto &p : parts) p.life = 0;
  hero.atkT = -1; hero.kbT = 0; hero.pushT = 0;
  say(ROOMS[r].name, 1.6f);
}

static void newGame(uint32_t now) {
  mode = M_PLAY; startMs = now; t_ = 0; freeze = 0; shake = 0; fade = 0; chestT = -1;
  memset(&hero, 0, sizeof hero);
  hero.hp = 6; hero.atkT = -1;
  memset(rs, 0, sizeof rs);
  enterRoom(0, false);
  hero.x = cellC(5); hero.y = cellC(7); hero.face = F_UP;
  hintT = 6.0f;
}

// nach dem Umfallen: aktueller Raum von vorn, volle Herzen
static void retryRoom() {
  mode = M_PLAY; hero.hp = 6; hero.inv = 0; hero.deadT = 0;
  bool fromSouth = ROOMS[room].south >= 0;
  initRoomState(room);
  enterRoom(room, fromSouth);
  if (room == 0) { hero.x = cellC(5); hero.y = cellC(7); }
}

void begin(uint16_t *fbuf, uint16_t *bgbuf, uint32_t seed) {
  fb = fbuf; bg = bgbuf; rng = seed ? seed : 1;
  buildArt();
  buildChestArt();
  lastMs = 0;
  newGame(0);
}

// ---------------------------------------------------------------- Steuerung
// Feste Zonen am unteren Rand, damit die Daumen nichts vom Spiel verdecken.
// Wichtige Dinge (Platten, Truhe) liegen nie in den Kacheln unter diesen Zonen.
static const float PAD_X = 104, PAD_Y = 352, PAD_R = 54;     // Steuerkreuz
static const float BTN_X = 362, BTN_Y = 352, BTN_R = 42;     // Schwert
static float stickX = 0, stickY = 0;   // -1..1
static bool padActive = false, btnDown = false, btnWas = false, anyWas = false;
static float padFx = PAD_X, padFy = PAD_Y;  // Fingerposition fuer die Anzeige

static void readInput(const Touch *pts, int n) {
  bool pad = false, btn = false;
  for (int k = 0; k < n; k++) {
    float x = pts[k].x, y = pts[k].y;
    float dpx = x - PAD_X, dpy = y - PAD_Y, dbx = x - BTN_X, dby = y - BTN_Y;
    float dp = sqrtf(dpx * dpx + dpy * dpy), db = sqrtf(dbx * dbx + dby * dby);
    // links von der Mitte und nicht ueber dem Knopf: alles zaehlt als Steuerkreuz,
    // damit ein abgerutschter Daumen nicht ploetzlich stehen bleibt
    if (db < BTN_R + 26) btn = true;
    else if (dp < PAD_R + 60 || (padActive && x < 233)) {
      pad = true; padFx = x; padFy = y;
      float len = dp;
      const float DEAD = 8, FULL = 34;
      if (len < DEAD) { stickX = stickY = 0; }
      else {
        float m = fminf(1.0f, (len - DEAD) / (FULL - DEAD));
        stickX = dpx / len * m; stickY = dpy / len * m;
      }
    }
  }
  padActive = pad;
  if (!pad) { stickX = stickY = 0; padFx = PAD_X; padFy = PAD_Y; }
  btnWas = btnDown; btnDown = btn;
}

// ---------------------------------------------------------------- Kampf
static void startAttack() {
  if (hero.cool > 0 || hero.kbT > 0) return;
  float ang = hero.face == F_UP ? -1.5708f : hero.face == F_DOWN ? 1.5708f : hero.face == F_LEFT ? 3.1416f : 0.0f;
  // leichte Zielhilfe: naechster Gegner in Reichweite und grob in Blickrichtung
  float best = 1e9f;
  for (int k = 0; k < cur->nSlimes; k++) {
    const Slime &s = cur->slimes[k];
    if (s.state >= 3) continue;
    float dx = s.x - hero.x, dy = s.y - hero.y, d = sqrtf(dx * dx + dy * dy);
    if (d > 80) continue;
    float a = atan2f(dy, dx) - ang;
    while (a > 3.1416f) a -= 6.2832f;
    while (a < -3.1416f) a += 6.2832f;
    if (fabsf(a) < 1.0f && d < best) { best = d; ang = atan2f(dy, dx); }
  }
  hero.atkT = 0; hero.atkAng = ang; hero.cool = 0.30f;
  for (auto &h : hero.atkHit) h = false;
}

static void hurtHero(float fromX, float fromY) {
  if (hero.inv > 0 || mode != M_PLAY) return;
  hero.hp -= 1; hero.inv = 1.1f; hero.kbT = 0.16f;
  float dx = hero.x - fromX, dy = hero.y - fromY, d = sqrtf(dx * dx + dy * dy) + 0.001f;
  hero.kbx = dx / d * 420; hero.kby = dy / d * 420;
  hero.atkT = -1;
  shake = 0.18f; freeze = 0.06f;
  if (hero.hp <= 0) { hero.hp = 0; mode = M_DEAD; hero.deadT = 0; endMs = lastMs; endT = t_; }
}

static void updateAttackHit() {
  if (hero.atkT < 0.03f || hero.atkT > 0.17f) return;
  for (int k = 0; k < cur->nSlimes; k++) {
    Slime &s = cur->slimes[k];
    if (hero.atkHit[k] || s.state >= 2) continue;
    float dx = s.x - hero.x, dy = s.y - hero.y, d = sqrtf(dx * dx + dy * dy);
    if (d > 75) continue;
    float a = atan2f(dy, dx) - hero.atkAng;
    while (a > 3.1416f) a -= 6.2832f;
    while (a < -3.1416f) a += 6.2832f;
    if (fabsf(a) > 1.45f && d > 27) continue;
    hero.atkHit[k] = true;
    s.hp--; s.flash = 0.18f; freeze = 0.07f; shake = 0.1f;
    float kd = d + 0.001f;
    s.fx = s.x; s.fy = s.y;
    s.tx = s.x + dx / kd * 80; s.ty = s.y + dy / kd * 80;
    s.state = 2; s.t = 0; s.dur = 0.2f; s.hz = 0;
    spawn(s.x, s.y - 12, 6, colorOf('P'), 100, 0.3f);
  }
}

// ---------------------------------------------------------------- Update
static void updateHero(float dt) {
  if (hero.cool > 0) hero.cool -= dt;
  if (hero.inv > 0) hero.inv -= dt;
  if (btnDown && !btnWas) {
    // Truhe direkt vor dem Held? Dann oeffnen statt zuschlagen
    const RoomDef &r = ROOMS[room];
    if (r.chest && !cur->chestOpen) {
      float dx = cellC(r.chestAt.i) - hero.x, dy = cellC(r.chestAt.j) - hero.y;
      if (dx * dx + dy * dy < 62 * 62) { cur->chestOpen = true; chestT = 0; hero.face = F_UP; return; }
    }
    startAttack();
  }
  if (hero.kbT > 0) {
    hero.kbT -= dt;
    moveBox(hero.x, hero.y, hero.kbx * dt, hero.kby * dt, HB, true, nullptr, nullptr);
    hero.moving = false; return;
  }
  if (hero.atkT >= 0) {
    hero.atkT += dt;
    if (hero.atkT > 0.22f) hero.atkT = -1;
  }
  const float SPEED = 165;
  float slow = hero.atkT >= 0 ? 0.25f : 1.0f;   // waehrend des Hiebs kaum bewegen
  float vx = stickX * SPEED * slow, vy = stickY * SPEED * slow;
  hero.moving = fabsf(stickX) + fabsf(stickY) > 0.05f;
  if (!hero.moving) { hero.pushT = 0; return; }
  if (hero.atkT < 0) {
    if (fabsf(vx) > fabsf(vy)) hero.face = vx > 0 ? F_RIGHT : F_LEFT; else hero.face = vy > 0 ? F_DOWN : F_UP;
  }

  bool hx, hy;
  moveBox(hero.x, hero.y, vx * dt, vy * dt, HB, true, &hx, &hy);

  // Schieben: klar in eine Richtung gegen einen Block laufen
  int di = 0, dj = 0;
  if (fabsf(vx) > fabsf(vy) * 1.4f && hx) di = vx > 0 ? 1 : -1;
  else if (fabsf(vy) > fabsf(vx) * 1.4f && hy) dj = vy > 0 ? 1 : -1;
  int pk = -1;
  if (di || dj) for (int k = 0; k < cur->nBlocks; k++) {
    Block &b = cur->blocks[k];
    if (b.moving || b.sinkT > 0) continue;
    float bx, by; blockPos(b, bx, by);
    float gx = (bx - hero.x) * di, gy = (by - hero.y) * dj;
    bool p = (di && fabsf(hero.y - by) < 24 && gx > HB + BH - 2 && gx < HB + BH + 5) ||
             (dj && fabsf(hero.x - bx) < 24 && gy > HB + BH - 2 && gy < HB + BH + 5);
    if (p) {
      pk = k;
      float kk = fminf(1, dt * 8);   // sanft auf die Blockmitte ausrichten
      if (di) moveBox(hero.x, hero.y, 0, (by - hero.y) * kk, HB, false, nullptr, nullptr);
      else moveBox(hero.x, hero.y, (bx - hero.x) * kk, 0, HB, false, nullptr, nullptr);
      break;
    }
  }
  if (pk >= 0 && di == hero.pushDi && dj == hero.pushDj && pk == hero.pushBlock) {
    hero.pushT += dt;
    if (hero.pushT > 0.25f) { tryPush(pk, di, dj); hero.pushT = 0; }
  } else { hero.pushT = pk >= 0 ? dt : 0; hero.pushDi = di; hero.pushDj = dj; hero.pushBlock = pk; }

  hero.walkT += dt;
  if (hero.walkT > 0.14f) { hero.walkT = 0; hero.frame ^= 1; }
}

static void updateSlimes(float dt) {
  for (int k = 0; k < cur->nSlimes; k++) {
    Slime &s = cur->slimes[k];
    if (s.flash > 0) s.flash -= dt;
    switch (s.state) {
      case 0: {
        s.t -= dt;
        s.anim = ((int)(t_ * 3.2f + k)) & 1;
        if (s.t <= 0) {
          float dx = hero.x - s.x, dy = hero.y - s.y, d = sqrtf(dx * dx + dy * dy);
          float ang = (d < 300 && mode == M_PLAY) ? atan2f(dy, dx) + frand(-0.45f, 0.45f) : frand(0, 6.2832f);
          float len = fminf(70, d > 45 ? d - 18 : 70);
          s.fx = s.x; s.fy = s.y;
          s.tx = s.x + cosf(ang) * len; s.ty = s.y + sinf(ang) * len;
          s.state = 1; s.t = 0; s.dur = 0.42f;
        }
      } break;
      case 1:
      case 2: {
        float px0 = s.x, py0 = s.y;
        s.t += dt;
        float p = fminf(1, s.t / s.dur);
        float e = s.state == 2 ? 1 - (1 - p) * (1 - p) : p;
        float wx = s.fx + (s.tx - s.fx) * e, wy = s.fy + (s.ty - s.fy) * e;
        moveBox(s.x, s.y, wx - px0, wy - py0, HB - 1, true, nullptr, nullptr);
        s.hz = s.state == 1 ? sinf(p * 3.1416f) * 24 : 0;
        s.anim = s.state == 1 ? (p < 0.15f || p > 0.85f ? 1 : 0) : 1;
        if (p >= 1) {
          if (s.hp <= 0) {
            s.state = 3; s.t = 0;
            spawn(s.x, s.y - 12, 14, colorOf('p'), 160, 0.45f);
            spawn(s.x, s.y - 12, 6, 0xFFFF, 90, 0.3f);
            if (hero.hp < 6) for (auto &h : hearts) if (!h.on) { h = {true, s.x, s.y, 0}; break; }
          } else {
            s.state = 0; s.t = frand(0.6f, 1.3f);
          }
        }
      } break;
      case 3:
        s.t += dt;
        if (s.t > 0.45f) s.state = 4;
        break;
      default: break;
    }
    if (s.state <= 1 && s.hz < 9) {
      float dx = hero.x - s.x, dy = hero.y - s.y;
      if (dx * dx + dy * dy < 28 * 28) hurtHero(s.x, s.y);
    }
  }
}

static void updateBlocksAndDoors(float dt) {
  for (int k = 0; k < cur->nBlocks; k++) {
    Block &b = cur->blocks[k];
    if (b.moving) {
      b.t += dt / 0.2f;
      if (b.t >= 1) {
        b.t = 1; b.moving = false;
        if (!blockCanReachPlate(b.i, b.j)) b.stuckT = 1.4f;
      }
    }
    if (b.stuckT > 0) {
      b.stuckT -= dt;
      if (b.stuckT <= 0) { b.sinkT = 0.6f; say("DER BLOCK SITZT FEST...", 1.8f); }
    }
    if (b.sinkT > 0) {
      float bx, by; blockPos(b, bx, by);
      b.sinkT -= dt;
      if (((int)(b.sinkT * 30)) % 3 == 0) spawn(bx, by + 15, 1, C(0x8a8199), 45, 0.3f);
      if (b.sinkT <= 0) {
        float sx = cellC(b.si), sy = cellC(b.sj);
        if ((fabsf(hero.x - sx) < HB + BH && fabsf(hero.y - sy) < HB + BH) || blockOnCell(b.si, b.sj, k)) b.sinkT = 0.05f;
        else {
          b.i = b.fi = b.si; b.j = b.fj = b.sj; b.moving = false;
          spawn(sx, sy, 10, C(0xc4bcd0), 90, 0.4f);
        }
      }
    }
  }
  const RoomDef &r = ROOMS[room];
  if (r.north >= 0) {
    bool want = cur->northOpen;
    if (r.cond == C_ENEMIES && !want) {
      bool alive = false;
      for (int k = 0; k < cur->nSlimes; k++) if (cur->slimes[k].state < 3) alive = true;
      if (!alive) { cur->northOpen = want = true; }
    } else if (r.cond == C_PLATES) {
      want = true;
      for (int k = 0; k < r.nPlates; k++) if (!plateWeighted(r.plates[k])) want = false;
    }
    if (want && !northWanted) { shake = 0.12f; say("KLACK! DIE TÜR IST OFFEN", 2.2f); }
    if (!want && northWanted && doorN > 0.5f) say("DIE TÜR GEHT ZU...", 1.5f);
    northWanted = want;
    bool heroInDoor = cellOf(hero.x) == DN_I && cellOf(hero.y - HB) <= DN_J;
    if (want || heroInDoor) doorN = fminf(1, doorN + dt / 0.45f);
    else doorN = fmaxf(0, doorN - dt / 0.3f);
  }
}

static void updateMisc(float dt) {
  for (auto &p : parts) if (p.life > 0) {
    p.life -= dt; p.x += p.vx * dt; p.y += p.vy * dt; p.vx *= 0.9f; p.vy *= 0.9f;
  }
  for (auto &h : hearts) if (h.on) {
    h.t += dt;
    if (h.t > 8) { h.on = false; continue; }
    float dx = hero.x - h.x, dy = hero.y - h.y;
    if (dx * dx + dy * dy < 32 * 32 && h.t > 0.3f) {
      hero.hp = hero.hp + 2 > 6 ? 6 : hero.hp + 2; h.on = false;
      spawn(hero.x, hero.y - 24, 8, colorOf('r'), 90, 0.4f);
    }
  }
  if (msgT > 0) msgT -= dt;
  if (hintT > 0) hintT -= dt;
  if (shake > 0) shake -= dt;
}

// Ausgaenge pruefen und Raumwechsel mit Abblenden
static void checkExits() {
  const RoomDef &r = ROOMS[room];
  if (r.north >= 0 && doorN >= 1 && cellOf(hero.x) == DN_I && hero.y < cellC(DN_J) + 15) {
    fade = 0.5f; fadeFrom = room; fadeTo = r.north;
  } else if (r.south >= 0 && cellOf(hero.x) == DS_I && hero.y > cellC(DS_J) - 15) {
    fade = 0.5f; fadeFrom = room; fadeTo = r.south;
  }
}
static void updateFade(float dt) {
  float before = fade;
  fade -= dt;
  if (before > 0.25f && fade <= 0.25f) {
    // Mitte des Abblendens: Raum tauschen. Nach Norden raus = im neuen Raum unten rein.
    bool goingNorth = ROOMS[fadeFrom].north == fadeTo;
    enterRoom(fadeTo, goingNorth);
  }
  if (fade < 0) fade = 0;
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

static void drawSword(int ox, int oy) {
  if (hero.atkT < 0) return;
  float p = fminf(1, hero.atkT / 0.13f);
  float a0 = hero.atkAng - 1.22f, a1 = hero.atkAng + 1.22f;
  float curA = a0 + (a1 - a0) * p;
  float cx = hero.x + ox, cy = hero.y - 12 + oy;
  const uint16_t trail = C(0xfff3d6), steel = C(0xe6edf5), steelD = C(0x8c97a8), hilt = C(0x7a5236);
  if (hero.atkT < 0.2f) {
    float fd = hero.atkT < 0.13f ? 0 : (hero.atkT - 0.13f) / 0.07f;
    for (float a = a0; a <= curA; a += 0.04f)
      for (float r = 42; r <= 57; r += 2) {
        int X = (int)(cx + cosf(a) * r), Y = (int)(cy + sinf(a) * r);
        if ((unsigned)X >= (unsigned)SCR || (unsigned)Y >= (unsigned)SCR) continue;
        float edge = (r < 45 || r > 54) ? 0.35f : 0.0f;
        fat(X, Y, mix(trail, bg[Y * SCR + X], fminf(1, 0.2f + edge + fd * 0.8f)));
      }
  }
  if (hero.atkT < 0.17f) {
    float c = cosf(curA), s = sinf(curA);
    for (float r = 9; r <= 60; r += 2) {
      int X = (int)(cx + c * r), Y = (int)(cy + s * r);
      if (r < 18) fat(X, Y, hilt);
      else { fat(X + 2, Y + 2, steelD); fat(X, Y, steel); }
    }
    fat((int)(cx + c * 18 - s * 6), (int)(cy + s * 18 + c * 6), C(0xd1a94e));
    fat((int)(cx + c * 18 + s * 6), (int)(cy + s * 18 - c * 6), C(0xd1a94e));
  }
}

static void drawSlime(const Slime &s, int ox, int oy) {
  if (s.state >= 3) return;
  drawShadow(s.x + ox, s.y + oy, (int)(36 - s.hz * 0.5f));
  drawArt(aSlime[s.anim], (int)s.x - 24 + ox, (int)(s.y - 33 - s.hz) + oy, false, s.flash > 0);
}

static void drawBlock(const Block &b, int ox, int oy) {
  float bx, by; blockPos(b, bx, by);
  if (b.sinkT > 0) {
    float k = 1 - b.sinkT / 0.6f; int cut = (int)(k * 48);
    for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++) {
      int Y = (int)by - 24 + j * SC + cut;
      if (Y + SC > (int)by + 24) continue;
      uint16_t c = aBlock.px[j * 16 + i]; if (c == TRANSP) continue;
      rect((int)bx - 24 + i * SC + ox, Y + oy, SC, SC, c);
    }
    return;
  }
  int jig = b.stuckT > 0 ? (((int)(b.stuckT * 30)) & 1) * SC : 0;
  darkenRect((int)bx - 21 + ox, (int)by + 21 + oy, 48, 6, 0.35f);
  drawArt(aBlock, (int)bx - 24 + ox + jig, (int)by - 24 + oy);
}

// Tuer in Kunst-Pixeln; Sued-Tuer ist gespiegelt (Gitter faehrt nach unten weg)
static void drawDoor(int ci, int cj, float open, bool south, int ox, int oy) {
  int x0 = OFF + ci * TILE + ox, y0 = OFF + cj * TILE + oy;
  auto ar = [&](int ax, int ay, int aw, int ah, uint16_t c) {
    if (south) ay = 16 - ay - ah;
    rect(x0 + ax * SC, y0 + ay * SC, aw * SC, ah * SC, c);
  };
  const uint16_t dark = C(0x07050b), arch = C(0x6f5f86), archD = C(0x3e3150), iron = C(0x8a90a0), ironD = C(0x4b4f5c);
  ar(0, 0, 16, 16, archD);
  ar(2, 3, 12, 13, dark);
  ar(1, 0, 14, 3, arch); ar(1, 2, 14, 1, archD);
  int bars = (int)(13 * (1 - open) + 0.5f);
  for (int b = 0; b < 4; b++) ar(3 + b * 3, 3, 1, bars, iron);
  if (bars > 1) { ar(2, 3 + bars - 1, 12, 1, ironD); ar(2, 6, 12, 1, ironD); }
  if (open > 0.9f) {
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

// Herzen oben links auf der Wand, weg von Steuerzonen und Tueren
static void drawHUD() {
  for (int k = 0; k < 3; k++) {
    int v = hero.hp - k * 2;
    const Art &a = v >= 2 ? aHeartFull : (v == 1 ? aHeartHalf : aHeartEmpty);
    drawArt(a, 52 + k * 27, 112 - k * 12);
  }
}

// Kreis fuellen/abdunkeln (fuer die Steuerzonen)
static void tintCircle(float cx, float cy, float r, uint16_t c, float k) {
  int y0 = (int)(cy - r), y1 = (int)(cy + r);
  for (int y = y0; y <= y1; y++) {
    if ((unsigned)y >= (unsigned)SCR) continue;
    float dy = y - cy; float hw = sqrtf(fmaxf(0, r * r - dy * dy));
    int x0 = (int)(cx - hw), x1 = (int)(cx + hw);
    for (int x = x0; x <= x1; x++) if ((unsigned)x < (unsigned)SCR) fb[y * SCR + x] = mix(fb[y * SCR + x], c, k);
  }
}
static void ring(float cx, float cy, float r, uint16_t c) {
  for (float a = 0; a < 6.2832f; a += 0.02f) fat((int)(cx + cosf(a) * r), (int)(cy + sinf(a) * r), c);
}

static void drawControls() {
  const uint16_t ink = C(0xf4efe6), dark = C(0x0b0810);
  // Steuerkreuz: halbdurchsichtige Scheibe, Kreuz, Daumen-Knopf
  tintCircle(PAD_X, PAD_Y, PAD_R, dark, 0.45f);
  ring(PAD_X, PAD_Y, PAD_R, mix(ink, dark, 0.65f));
  const int arm = 14, len = 40;
  uint16_t cross = mix(ink, dark, padActive ? 0.55f : 0.7f);
  for (int y = -len; y <= len; y += SC) for (int x = -arm / 2; x <= arm / 2; x += SC) {
    fat((int)PAD_X + x, (int)PAD_Y + y, cross); fat((int)PAD_X + y, (int)PAD_Y + x, cross);
  }
  float kx = PAD_X + stickX * 30, ky = PAD_Y + stickY * 30;
  tintCircle(kx, ky, 17, padActive ? C(0x2fa39a) : ink, padActive ? 0.9f : 0.35f);
  // Schwert-Knopf
  bool lit = btnDown || hero.atkT >= 0;
  tintCircle(BTN_X, BTN_Y, BTN_R, lit ? C(0xf08a3c) : dark, lit ? 0.75f : 0.45f);
  ring(BTN_X, BTN_Y, BTN_R, mix(ink, dark, lit ? 0.2f : 0.65f));
  // kleines Schwert-Symbol (diagonal)
  uint16_t sw = lit ? ink : mix(ink, dark, 0.35f);
  for (int k = -15; k <= 12; k += 2) fat((int)BTN_X + k, (int)BTN_Y - k, sw);
  for (int k = -6; k <= 6; k += 2) fat((int)BTN_X + 9 + k, (int)BTN_Y - 9 + k, C(0xd1a94e));
  for (int k = 0; k <= 6; k += 2) fat((int)BTN_X + 12 + k, (int)BTN_Y - 12 + k, C(0x7a5236));
}

static void render() {
  int ox = 0, oy = 0;
  if (shake > 0) { ox = ((int)(rnd() % 5) - 2) * 1; oy = ((int)(rnd() % 5) - 2) * 1; }
  for (int y = 0; y < SCR; y++) {
    int sy = y - oy; if (sy < 0) sy = 0; if (sy >= SCR) sy = SCR - 1;
    uint16_t *d = fb + y * SCR; const uint16_t *s = bg + sy * SCR;
    if (ox >= 0) { memcpy(d + ox, s, (SCR - ox) * 2); for (int x = 0; x < ox; x++) d[x] = 0; }
    else { memcpy(d, s - ox, (SCR + ox) * 2); for (int x = SCR + ox; x < SCR; x++) d[x] = 0; }
  }
  const RoomDef &r = ROOMS[room];
  for (int k = 0; k < r.nPlates; k++)
    drawArt(plateWeighted(r.plates[k]) ? aPlateDown : aPlateUp, OFF + r.plates[k].i * TILE + ox, OFF + r.plates[k].j * TILE + oy);
  if (r.north >= 0) drawDoor(DN_I, DN_J, doorN, false, ox, oy);
  if (r.south >= 0) drawDoor(DS_I, DS_J, doorS, true, ox, oy);
  drawTorches(ox, oy);

  // Figuren nach y sortiert
  struct Ent { float y; int k, idx; } ents[12]; int ne = 0;
  for (int k = 0; k < cur->nBlocks; k++) { float bx, by; blockPos(cur->blocks[k], bx, by); ents[ne++] = {by + 8, 0, k}; }
  for (int k = 0; k < cur->nSlimes; k++) ents[ne++] = {cur->slimes[k].y, 1, k};
  ents[ne++] = {hero.y, 2, 0};
  for (int k = 0; k < 4; k++) if (hearts[k].on) ents[ne++] = {hearts[k].y, 3, k};
  if (r.chest) ents[ne++] = {cellC(r.chestAt.j) + 8, 4, 0};
  for (int a = 1; a < ne; a++) for (int b = a; b > 0 && ents[b].y < ents[b - 1].y; b--) { Ent t = ents[b]; ents[b] = ents[b - 1]; ents[b - 1] = t; }
  for (int e = 0; e < ne; e++) {
    switch (ents[e].k) {
      case 0: drawBlock(cur->blocks[ents[e].idx], ox, oy); break;
      case 1: drawSlime(cur->slimes[ents[e].idx], ox, oy); break;
      case 2: drawHero(ox, oy); drawSword(ox, oy); break;
      case 3: {
        const Pickup &h = hearts[ents[e].idx];
        int hop = (int)(fabsf(sinf(h.t * 5)) * 9 * fmaxf(0, 1 - h.t));
        bool blink = h.t > 6 && ((int)(h.t * 8) & 1);
        if (!blink) drawArt(aHeartFull, (int)h.x - 12 + ox, (int)h.y - 21 - hop + oy);
      } break;
      case 4: {
        int x0 = OFF + r.chestAt.i * TILE + ox, y0 = OFF + r.chestAt.j * TILE + oy;
        darkenRect(x0 + 6, y0 + 42, 40, 6, 0.35f);
        drawArt(aChest[cur->chestOpen ? 1 : 0], x0, y0);
        if (chestT >= 0) {  // Schatz steigt auf
          float up = fminf(1, chestT / 0.8f);
          int sx = x0 + 24, sy = y0 + 6 - (int)(up * 40);
          for (int k = -3; k <= 3; k++) { fat(sx + k * 3, sy, C(0xffe27a)); fat(sx, sy + k * 3, C(0xffe27a)); }
          fat(sx, sy, 0xFFFF);
        }
      } break;
    }
  }
  for (auto &p : parts) if (p.life > 0) fat((int)p.x + ox, (int)p.y + oy, p.c);

  drawHUD();
  if (mode == M_PLAY) drawControls();

  if (hintT > 0 && mode == M_PLAY && room == 0) {
    uint16_t c = hintT < 1 ? mix(C(0xf4efe6), C(0x5b4e6e), 1 - hintT) : C(0xf4efe6);
    text("LINKS: LAUFEN", 196, 2, c);
    text("RECHTS: SCHWERT", 222, 2, c);
  }
  if (fade > 0) {  // abblenden und wieder aufblenden
    float k = fade > 0.25f ? (0.5f - fade) / 0.25f : fade / 0.25f;
    for (int i = 0; i < SCR * SCR; i++) fb[i] = mix(fb[i], 0, fminf(1, k));
  }
  if (mode != M_PLAY) {
    float k = fminf(0.6f, (t_ - endT) * 1.5f);
    if (mode == M_DEAD) k = fminf(0.6f, (hero.deadT - 0.8f) * 1.2f);
    if (k > 0) {
      for (int i = 0; i < SCR * SCR; i++) fb[i] = mix(fb[i], 0, k);
      char buf[32]; uint32_t s = (endMs - startMs) / 1000;
      if (mode == M_WIN) {
        text("SCHATZ GEFUNDEN!", 170, 3, C(0xffe27a));
        snprintf(buf, sizeof buf, "ZEIT %u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
        text(buf, 230, 3, C(0xf4efe6));
        text("ENDE DES PROTOTYPS", 280, 2, C(0xb9b0c8));
        text("TIPPEN = NEU STARTEN", 312, 2, C(0xb9b0c8));
      } else {
        text("AUTSCH!", 200, 5, C(0xff8fa0));
        text("TIPPEN = RAUM NOCHMAL", 300, 2, C(0xb9b0c8));
      }
    }
  } else if (msgT > 0 && fade <= 0) {
    text(msg, 160, 2, C(0xf4efe6));
  }
}

// ---------------------------------------------------------------- Takt
void tick(uint32_t now, const Touch *pts, int n) {
  if (!lastMs) { lastMs = now; startMs = now; }
  float dt = (now - lastMs) / 1000.0f; lastMs = now;
  if (dt > 0.05f) dt = 0.05f;
  bool any = n > 0;
  t_ += dt;
  if (mode != M_PLAY) {
    // nach Ende: ein Tipp (irgendwo) startet neu, erst nach kurzer Pause
    float waited = mode == M_DEAD ? hero.deadT - 1.2f : t_ - endT - 0.8f;
    if (!any && anyWas && waited > 0) {
      if (mode == M_WIN) newGame(now); else retryRoom();
    }
    anyWas = any;
    if (mode == M_DEAD) hero.deadT += dt;
    updateMisc(dt);
    render();
    return;
  }
  anyWas = any;
  readInput(pts, n);
  if (fade > 0) { updateFade(dt); updateMisc(dt); render(); return; }
  if (freeze > 0) { freeze -= dt; if (shake > 0) shake -= dt; render(); return; }
  updateHero(dt);
  updateAttackHit();
  updateSlimes(dt);
  updateBlocksAndDoors(dt);
  updateMisc(dt);
  if (chestT >= 0) {
    chestT += dt;
    if (chestT > 1.2f) { mode = M_WIN; endMs = now; endT = t_; chestT = -1; }
  }
  if (mode == M_PLAY && fade <= 0) checkExits();
  render();
}

Debug debug() {
  Debug d;
  memset(&d, 0, sizeof d);
  d.hx = hero.x; d.hy = hero.y; d.hp = hero.hp; d.mode = mode; d.room = room; d.fading = fade > 0;
  d.doorOpen = doorN >= 1;
  d.nSlimes = 0;
  for (int k = 0; k < cur->nSlimes; k++) if (cur->slimes[k].state < 3) {
    d.sx[d.nSlimes] = cur->slimes[k].x; d.sy[d.nSlimes] = cur->slimes[k].y; d.nSlimes++;
  }
  d.nBlocks = cur->nBlocks;
  for (int k = 0; k < cur->nBlocks; k++) {
    d.bi[k] = cur->blocks[k].i; d.bj[k] = cur->blocks[k].j;
    if (cur->blocks[k].moving || cur->blocks[k].sinkT > 0) d.blockMoving = true;
  }
  d.chestOpen = cur->chestOpen;
  return d;
}

// Fuer Tests: Bildschirmmitte der Steuerzonen
void controlCenters(float &padX, float &padY, float &btnX, float &btnY) { padX = PAD_X; padY = PAD_Y; btnX = BTN_X; btnY = BTN_Y; }

}  // namespace kw
