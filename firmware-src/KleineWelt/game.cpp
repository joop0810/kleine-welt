// Kleine Welt - "Cala": eine kleine Mittelmeerbucht als Wohlfuehl-Deko fuer die Wand.
//
// Was passiert von selbst:
//   - echter Tageslauf nach der Uhr (Sonnenauf-/untergang je nach Monat, wie auf Mallorca)
//   - Himmel, Wolken, Moewen, Sterne, Mond; Meer mit Glitzern und Wellen am Strand
//   - Segelboote und ein Fischerboot (Llaut) kreuzen durch die Bucht
//   - abends Licht in der Finca, Lichterkette am Steg, Gluehwuermchen
// Antippen (alles optional, nichts kann "kaputtgehen"):
//   - Boot        -> faehrt zum Steg, legt eine Weile an
//   - Meer        -> ein Fisch springt
//   - Himmel      -> eine Moewe fliegt los
//   - Finca       -> Licht an/aus
//   - lange halten-> Uhr stellen
//
// Gezeichnet wird in 233x233 "Kunstpixeln", doppelt skaliert. Alle Grafik ist
// eigene Pixelkunst bzw. wird im Code erzeugt.
#include "game.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

namespace kw {

// ---------------------------------------------------------------- Grundlagen
static uint16_t *fb = nullptr;
static uint32_t rng = 1;
static uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static float frand(float a, float b) { return a + (b - a) * (rnd() % 10000) / 10000.0f; }
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

static constexpr uint16_t C(uint32_t rgb) {
  return (uint16_t)((((rgb >> 16) & 0xF8) << 8) | (((rgb >> 8) & 0xFC) << 3) | ((rgb & 0xFF) >> 3));
}
static inline void unpack(uint16_t c, int &r, int &g, int &b) { r = (c >> 11) << 3; g = ((c >> 5) & 63) << 2; b = (c & 31) << 3; }
static inline uint16_t pack(int r, int g, int b) {
  r = r < 0 ? 0 : (r > 255 ? 255 : r); g = g < 0 ? 0 : (g > 255 ? 255 : g); b = b < 0 ? 0 : (b > 255 ? 255 : b);
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
static inline uint16_t mix(uint16_t a, uint16_t b, float t) {
  int r1, g1, b1, r2, g2, b2; unpack(a, r1, g1, b1); unpack(b, r2, g2, b2);
  return pack(r1 + (int)((r2 - r1) * t), g1 + (int)((g2 - g1) * t), b1 + (int)((b2 - b1) * t));
}
static inline uint16_t mixRGB(uint32_t a, uint32_t b, float t) {
  int r1 = a >> 16, g1 = (a >> 8) & 255, b1 = a & 255, r2 = b >> 16, g2 = (b >> 8) & 255, b2 = b & 255;
  return pack(r1 + (int)((r2 - r1) * t), g1 + (int)((g2 - g1) * t), b1 + (int)((b2 - b1) * t));
}
static inline uint32_t lerpRGB(uint32_t a, uint32_t b, float t) {
  int r1 = a >> 16, g1 = (a >> 8) & 255, b1 = a & 255, r2 = b >> 16, g2 = (b >> 8) & 255, b2 = b & 255;
  return ((uint32_t)(r1 + (r2 - r1) * t) << 16) | ((uint32_t)(g1 + (g2 - g1) * t) << 8) | (uint32_t)(b1 + (b2 - b1) * t);
}
static uint32_t hash2(int a, int b, int c) {
  uint32_t h = (uint32_t)a * 374761393u + (uint32_t)b * 668265263u + (uint32_t)c * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16);
}
// Bildschirm (volle Aufloesung) - nur fuer Text und Uhr-Menue
static void rect(int x, int y, int w, int h, uint16_t c) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > SCR) w = SCR - x;
  if (y + h > SCR) h = SCR - y;
  for (int j = 0; j < h; j++) { uint16_t *p = fb + (y + j) * SCR + x; for (int i = 0; i < w; i++) p[i] = c; }
}
static const uint16_t K_OUT = C(0x1b1424);

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



// ---------------------------------------------------------------- Kunstpixel-Ebene
constexpr int L = 233;               // logische Aufloesung
constexpr int HOR = 92;              // Horizont
enum Mat : uint8_t { M_SKY, M_SEA, M_ROCK, M_SCRUB, M_SAND, M_WET, M_OBJ, M_WINDOW, M_JETTY };
static uint16_t *base = nullptr;     // Tagesfarben der festen Szene
static uint8_t *mat = nullptr;       // Material je Kunstpixel
static uint16_t *lf = nullptr;       // aktuelles Bild in Kunstpixeln

static inline void lp(int x, int y, uint16_t c) { if ((unsigned)x < (unsigned)L && (unsigned)y < (unsigned)L) lf[y * L + x] = c; }
static inline void bp(int x, int y, uint16_t c, Mat m = M_OBJ) {
  if ((unsigned)x < (unsigned)L && (unsigned)y < (unsigned)L) { base[y * L + x] = c; mat[y * L + x] = m; }
}
// nur auf Himmel oder Meer zeichnen (Felsen stehen davor)
static inline void lpBehind(int x, int y, uint16_t c) {
  if ((unsigned)x < (unsigned)L && (unsigned)y < (unsigned)L) { uint8_t m = mat[y * L + x]; if (m == M_SKY || m == M_SEA) lf[y * L + x] = c; }
}
static inline void lpSky(int x, int y, uint16_t c) {
  if ((unsigned)x < (unsigned)L && (unsigned)y < (unsigned)L && mat[y * L + x] == M_SKY) lf[y * L + x] = c;
}

// ---------------------------------------------------------------- Landschaft
// Linke Landzunge mit Finca-Plateau, rechte kleinere Felsnase; der Strand liegt unten
static float topL(int x) { return x < 54 ? 73 + 1.5f * sinf(x * 0.35f) : 73 + (x - 54) * 1.25f; }
static float coastL(int y) { return y < HOR ? 70 : 70 + (y - HOR) * 0.05f + 2.5f * sinf(y * 0.37f) + 1.5f * sinf(y * 1.1f); }
static float topR(int x) { return 97 + (L - x) * 0.22f + 1.5f * sinf(x * 0.5f); }
static float coastR(int y) { return 176 - (y - 100) * 0.07f + 2 * sinf(y * 0.45f) + sinf(y * 1.3f); }
static float beachY(int x) { float u = (x - 116) / 116.0f; return 180 - 26 * u * u; }

static bool landL(int x, int y) { return x <= coastL(y) && y >= topL(x) && y < beachY(x) + 3; }
static bool landR(int x, int y) { return x >= 170 && x >= coastR(y) && y >= topR(x) && y < beachY(x) + 3; }
// weiches Rauschen fuer Felsen und Bewuchs
static float vnoise(float x, float y, int seed) {
  int xi = (int)floorf(x), yi = (int)floorf(y); float fx = x - xi, fy = y - yi;
  auto h = [&](int a, int b) { return (hash2(a, b, seed) & 1023) / 1023.0f; };
  float a = h(xi, yi), b = h(xi + 1, yi), c = h(xi, yi + 1), d = h(xi + 1, yi + 1);
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

static void disc(float cx, float cy, float r, uint16_t c, Mat m = M_OBJ) {
  for (int y = (int)(cy - r); y <= (int)(cy + r); y++) for (int x = (int)(cx - r); x <= (int)(cx + r); x++)
    if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) bp(x, y, c, m);
}
static void ell(float cx, float cy, float rx, float ry, uint16_t c, Mat m = M_OBJ) {
  for (int y = (int)(cy - ry); y <= (int)(cy + ry); y++) for (int x = (int)(cx - rx); x <= (int)(cx + rx); x++) {
    float u = (x - cx) / rx, v = (y - cy) / ry;
    if (u * u + v * v <= 1) bp(x, y, c, m);
  }
}

// Aleppo-Kiefer: flacher Schirm auf schraegem Stamm
static void pine(int x, int y, float s) {
  const uint16_t trunk = C(0x5a3a26), dk = C(0x2b4a2c), md = C(0x3d6b37), lt = C(0x5c8a44);
  for (int k = 0; k < (int)(14 * s); k++) { bp(x + k / 4, y - k, trunk); bp(x + k / 4 + 1, y - k, trunk); }
  int tx = x + (int)(14 * s) / 4, ty = y - (int)(14 * s);
  ell(tx, ty, 11 * s, 4 * s, dk);
  ell(tx - 1, ty - 1, 9 * s, 3 * s, md);
  for (int k = 0; k < 12; k++) { int px = tx - (int)(8 * s) + (int)(hash2(x, k, 3) % (int)(16 * s + 1)); bp(px, ty - (int)(2 * s) + (int)(hash2(k, y, 4) % 2), lt); }
}
static void cypress(int x, int y, int h) {
  const uint16_t dk = C(0x22402a), md = C(0x2f5a34);
  for (int j = 0; j < h; j++) {
    float v = (float)j / h; int w = (int)(3.2f * sinf(v * 3.1416f) + 0.6f);
    for (int i = -w; i <= w; i++) bp(x + i, y - j, i > 0 ? dk : md);
  }
}
static void bush(int x, int y, int r) {
  ell(x, y, r + 1, r * 0.7f, C(0x3e5e30)); ell(x - 1, y - 1, r * 0.8f, r * 0.5f, C(0x557a3c));
}

static void finca() {
  const uint16_t wall = C(0xf3ede0), wallS = C(0xd9cfbd), roof = C(0xc8643c), roofD = C(0x8e3f24), door = C(0x6e4630);
  // Haupthaus
  for (int y = 59; y <= 73; y++) for (int x = 24; x <= 50; x++) bp(x, y, x >= 46 ? wallS : wall);
  for (int y = 53; y <= 59; y++) {           // Ziegeldach
    int in = (59 - y);
    for (int x = 22 + in; x <= 52 - in; x++) bp(x, y, (y % 2) ? roof : roofD);
  }
  // kleiner Turm
  for (int y = 48; y <= 59; y++) for (int x = 27; x <= 32; x++) bp(x, y, x >= 31 ? wallS : wall);
  for (int x = 26; x <= 33; x++) { bp(x, 47, roof); bp(x, 46, roofD); }
  for (int x = 28; x <= 31; x++) bp(x, 45, roof);
  // Fenster (nachts erleuchtet) und Tuer mit Bogen
  const uint16_t win = C(0x2b3a5c);
  int wins[][2] = {{36, 63}, {42, 63}, {29, 51}, {26, 64}};
  for (auto &w : wins) for (int y = 0; y < 4; y++) for (int x = 0; x < 2; x++) bp(w[0] + x, w[1] + y, win, M_WINDOW);
  for (int y = 67; y <= 73; y++) for (int x = 31; x <= 34; x++) bp(x, y, door);
  bp(31, 67, wall); bp(34, 67, wall);
  // Terrasse mit Mauer und Bougainvillea
  for (int x = 50; x <= 58; x++) { bp(x, 72, wallS); bp(x, 73, wallS); }
  for (int k = 0; k < 14; k++) bp(50 + (int)(hash2(k, 1, 9) % 9), 69 + (int)(hash2(k, 2, 9) % 3), (k & 1) ? C(0xd63a8c) : C(0xb02a6e));
  cypress(20, 74, 22);
}

static void jetty() {
  const uint16_t pl = C(0xb68a5a), plD = C(0x7a5536), post = C(0x4a3424);
  int x0 = 136, x1 = 145, ytop = 152, ybot = (int)beachY(140) + 2;
  for (int y = ytop; y <= ybot; y++) for (int x = x0; x <= x1; x++) bp(x, y, (y % 3 == 0) ? plD : pl, M_JETTY);
  for (int y = ytop + 2; y <= ybot; y += 7) { bp(x0 - 1, y, post, M_JETTY); bp(x1 + 1, y, post, M_JETTY); bp(x0 - 1, y + 1, post, M_JETTY); bp(x1 + 1, y + 1, post, M_JETTY); }
  for (int x = x0; x <= x1; x++) bp(x, ytop - 1, plD, M_JETTY);
}

static void parasol(int x, int y, uint32_t c1, uint32_t c2) {
  // Schatten auf dem Sand
  for (int j = -2; j <= 2; j++) for (int i = -9; i <= 9; i++)
    if (i * i / 81.0f + j * j / 6.0f <= 1) { int X = x + 3 + i, Y = y + 1 + j; if ((unsigned)X < (unsigned)L && (unsigned)Y < (unsigned)L) base[Y * L + X] = mix(base[Y * L + X], C(0x8a7050), 0.35f); }
  // Handtuch
  for (int j = -1; j <= 2; j++) for (int i = 3; i <= 13; i++) bp(x + i, y + j, ((i / 2) % 2) ? C(c1) : C(0xf6f0e6));
  // Stange
  for (int k = 0; k < 14; k++) bp(x, y - k, C(0xeae2d0));
  // Schirm: Halbkuppel mit Streifen, gezackter Saum
  for (int dy = 0; dy <= 5; dy++) {
    float v = (5 - dy) / 6.0f; int w = (int)(10 * sqrtf(1 - v * v) + 0.5f);
    for (int i = -w; i <= w; i++) {
      int sector = (int)floorf((i + 0.5f) * 5.0f / (w + 1) + 5);
      uint16_t c = (sector & 1) ? C(c1) : C(c2);
      if (dy == 5 && ((i + 20) % 3 == 0)) continue;
      if (dy == 0 || i == -w) c = mix(c, 0xFFFF, 0.25f);
      bp(x + i, y - 19 + dy, c);
    }
  }
  bp(x, y - 20, C(0xeae2d0));
}

static void palm(int bx, int by) {
  const uint16_t tr = C(0x8a6a44), trD = C(0x5e4630), fr = C(0x3f7a3a), frL = C(0x6aa64c);
  float tx = bx, ty = by;
  for (int k = 0; k < 70; k++) {                   // geschwungener Stamm
    float v = k / 70.0f;
    tx = bx - 18 * v * v; ty = by - 70 * v;
    for (int i = -2; i <= 1; i++) bp((int)tx + i, (int)ty, (k % 4 == 0) ? trD : tr);
  }
  int cx = (int)tx, cy = (int)ty;
  const float ang[] = {-2.9f, -2.4f, -1.8f, -1.2f, -0.6f, -0.1f, 0.4f};
  for (float a : ang) {
    for (int k = 0; k < 30; k++) {
      float d = k * 1.0f, droop = k * k * 0.028f;
      int x = cx + (int)(cosf(a) * d), y = cy + (int)(sinf(a) * d * 0.6f + droop);
      bp(x, y, frL); bp(x, y + 1, fr);
      if (k > 3 && k < 27) {           // Fiedern beidseitig, nach aussen kuerzer
        int len = k < 14 ? 3 : 2;
        for (int q = 1; q <= len; q++) { bp(x - (k & 1), y - q, fr); bp(x + (k & 1), y + 1 + q, fr); }
      }
    }
  }
  ell(cx, cy + 2, 3, 2, C(0x6e4a2a));   // Kokosnuesse
}

static void buildScene() {
  for (int y = 0; y < L; y++) for (int x = 0; x < L; x++) {
    uint16_t c; Mat m;
    uint32_t h = hash2(x, y, 11);
    bool lL = landL(x, y), lR = landR(x, y);
    float by = beachY(x);
    if (lL || lR) {
      float top = lL ? topL(x) : topR(x);
      float edge = lL ? coastL(y) - x : x - coastR(y);
      float n = vnoise(x / 6.0f, y / 4.0f, 21), g = vnoise(x / 5.0f, y / 5.0f, 37);
      // Kalkstein: helle und dunkle Flecken, Kante zur Bucht im Licht
      uint32_t lime = n > 0.62f ? 0xd2b48c : (n > 0.38f ? 0xbea07c : 0x9e8264);
      if (edge < 2.5f) lime = 0xe2caa2;
      if (edge >= 2.5f && edge < 4) lime = 0x8a6e54;
      if ((h & 31) == 0) lime = 0x7c6450;
      c = C(lime); m = M_ROCK;
      // Bewuchs: oben dicht, am Hang in Flecken
      float depth = y - top;
      float cover = depth < 5 ? 1.0f : (depth < 30 ? 0.62f - depth * 0.006f : 0.42f);
      if (g > 1 - cover * 0.75f && edge > 3) { c = (h & 3) ? C(0x5e7c36) : C(0x76963f); m = M_SCRUB; }
      if (y >= by - 1) { c = C(0x9e8264); m = M_ROCK; }
    } else if (y < HOR) {
      c = 0; m = M_SKY;
    } else if (y < by) {
      // Meer: tief am Horizont, tuerkis zum Strand
      float v = clampf((y - HOR) / (by - HOR), 0, 1);
      uint32_t col = v < 0.55f ? lerpRGB(0x1f5f8f, 0x2a8fb8, v / 0.55f) : lerpRGB(0x2a8fb8, 0x45c9c4, (v - 0.55f) / 0.45f);
      if (by - y < 7) col = lerpRGB(col, 0x8fe3d2, (7 - (by - y)) / 7.0f);
      if ((h & 31) == 0) col = lerpRGB(col, 0xffffff, 0.12f);
      c = C(col); m = M_SEA;
    } else if (y < by + 3) {
      c = C(0xc9b080); m = M_WET;
    } else {
      uint32_t s = (h & 7) == 0 ? 0xdcc497 : 0xead6aa;
      c = C(s); m = M_SAND;
    }
    base[y * L + x] = c; mat[y * L + x] = m;
  }
  // Fernes Inselchen am Horizont
  for (int x = 86; x <= 122; x++) {
    float u = (x - 104) / 18.0f; int hgt = (int)(7 * (1 - u * u) + (x > 106 ? 2 * sinf(x * 0.6f) : 0));
    for (int k = 0; k <= hgt; k++) if (mat[(HOR - k) * L + x] == M_SKY) bp(x, HOR - k, C(0x7f98b0), M_ROCK);
  }
  finca();
  bush(62, 84, 3); bush(14, 77, 4); bush(56, 100, 3);
  pine(58, 80, 1.0f); pine(64, 96, 0.8f);
  pine(198, 106, 0.85f); pine(216, 100, 1.05f);
  bush(186, 113, 3); bush(226, 108, 3);
  jetty();
  parasol(64, 200, 0xd8403c, 0xf6f0e6);
  parasol(96, 206, 0x2f6fb0, 0xf6f0e6);
  parasol(170, 202, 0xe0a030, 0xf6f0e6);
  palm(214, 236);
}

// ---------------------------------------------------------------- Zeit und Licht
static float hours = 12.0f;          // 0..24, Ortszeit
static int month = 7;
static float testHours = -1;
static uint32_t clockSetMs = 0;
static float clockSetHours = 12.0f;
// Sonnenauf- und -untergang (Ortszeit, gerundet wie auf Mallorca)
static const float SUNRISE[12] = {8.1f, 7.75f, 7.2f, 7.4f, 6.85f, 6.5f, 6.65f, 7.1f, 7.6f, 8.0f, 7.6f, 8.0f};
static const float SUNSET[12] = {17.7f, 18.25f, 18.75f, 20.25f, 20.75f, 21.25f, 21.25f, 20.8f, 20.1f, 19.3f, 17.75f, 17.5f};

struct Light {
  float day, golden, dawn;            // 0..1
  int mr, mg, mb;                     // Farbmultiplikatoren (256 = 1.0)
  uint32_t skyTop, skyMid, skyHor;
};
static Light light;

static void computeLight() {
  float sr = SUNRISE[(month + 11) % 12], ss = SUNSET[(month + 11) % 12];
  float h = hours;
  float up = clampf((h - (sr - 0.5f)) / 1.0f, 0, 1), down = clampf(((ss + 0.6f) - h) / 1.1f, 0, 1);
  float d = fminf(up, down);
  float gold = fmaxf(0, 1 - fabsf(h - (ss - 0.1f)) / 1.1f);
  float dawn = fmaxf(0, 1 - fabsf(h - (sr + 0.2f)) / 0.9f);
  light.day = d; light.golden = gold; light.dawn = dawn;
  // Land und Meer: Nacht blaeulich dunkel, goldene Stunde warm
  float r = 0.26f + 0.74f * d, g = 0.29f + 0.71f * d, b = 0.45f + 0.55f * d;
  r *= 1 + 0.10f * gold + 0.05f * dawn; g *= 1 - 0.12f * gold; b *= 1 - 0.30f * gold - 0.08f * dawn;
  light.mr = (int)(r * 256); light.mg = (int)(g * 256); light.mb = (int)(b * 256);
  uint32_t top = lerpRGB(0x060914, 0x3d8fd6, d), mid = lerpRGB(0x0c1430, 0x7fbde8, d), hor = lerpRGB(0x1c2a48, 0xc4e6f2, d);
  top = lerpRGB(top, 0x44508e, gold * 0.8f); mid = lerpRGB(mid, 0xe58a96, gold * 0.85f); hor = lerpRGB(hor, 0xffa858, gold);
  top = lerpRGB(top, 0x5a78b8, dawn * 0.6f); mid = lerpRGB(mid, 0xe8a8b8, dawn * 0.7f); hor = lerpRGB(hor, 0xffcf96, dawn * 0.85f);
  light.skyTop = top; light.skyMid = mid; light.skyHor = hor;
}
static inline uint16_t lit(uint16_t c) {
  int r, g, b; unpack(c, r, g, b);
  return pack((r * light.mr) >> 8, (g * light.mg) >> 8, (b * light.mb) >> 8);
}

// ---------------------------------------------------------------- Bewohner
struct Boat {
  int kind;            // 0 grosses Segelboot, 1 kleines Segelboot, 2 Llaut (Fischerboot)
  float x, y, vx;      // y = Wasserlinie
  int state;           // 0 faehrt, 1 legt an, 2 liegt, 3 legt ab
  float t;
};
static Boat boats[3];
struct Cloud { float x, y, w; };
static Cloud clouds[4];
struct Gull { bool on; float x, y, vx, vy, t; };
static Gull gulls[4];
struct Fish { bool on; float x, y, t; };
static Fish fish[3];
struct Ring { bool on; float x, y, t; };
static Ring rings[4];
struct Star { uint8_t x, y, ph; };
static Star stars[70];
struct Fly { float x, y, ph; };
static Fly flies[10];
static bool fincaLight = true;
static float t_ = 0;

static const char *BOAT_BIG[13] = {
  "......k.....", "......kw....", ".....Wkww...", ".....Wkwww..", "....WWkwww..", "....WWkwwww.",
  "...WWWkwwww.", "...WWWkwwwww", "..WWWWkwwwww", "......k.....", "bbbbbbbbbbbb", ".hhhhhhhhhh.", "..hhhhhhhh.."};
static const char *BOAT_SMALL[8] = {"...k...", "..Wkw..", "..Wkww.", ".WWkww.", ".WWkwww", "...k...", "bbbbbbb", ".hhhhh."};
static const char *BOAT_LLAUT[7] = {"....kttt....", "....tWWt....", "....tttt....", "nnnnnnnnnnnn", "gggggggggggg", ".nnnnnnnnnn.", "..NNNNNNNN.."};
static uint16_t boatCol(char ch) {
  switch (ch) {
    case 'k': return C(0x3a3030); case 'w': return C(0xfbf8f0); case 'W': return C(0xd0d8e4);
    case 'b': return C(0x2f6fb0); case 'h': return C(0xf2efe8); case 't': return C(0xe8e2d6);
    case 'n': return C(0xb07a46); case 'N': return C(0x6e4630); case 'g': return C(0x3c8a6a);
    default: return 0;
  }
}
static void boatSize(int kind, int &w, int &h) { if (kind == 0) { w = 12; h = 13; } else if (kind == 1) { w = 7; h = 8; } else { w = 12; h = 7; } }
static const char *const *boatArt(int kind) { return kind == 0 ? BOAT_BIG : (kind == 1 ? BOAT_SMALL : BOAT_LLAUT); }

static const float DOCK_X = 154, DOCK_Y = 158;   // rechts am Stegende

static void initWorld() {
  boats[0] = {0, 40, 132, 4.5f, 0, 0};
  boats[1] = {1, 150, 102, -2.2f, 0, 0};
  boats[2] = {2, 190, 146, -3.5f, 0, 0};
  for (int k = 0; k < 4; k++) clouds[k] = {frand(0, L), frand(22, 62), frand(14, 30)};
  for (auto &s : stars) {
    int x, y;
    do { x = rnd() % L; y = rnd() % (HOR - 4); } while (mat[y * L + x] != M_SKY);
    s = {(uint8_t)x, (uint8_t)y, (uint8_t)(rnd() & 255)};
  }
  for (auto &f : flies) f = {frand(12, 66), frand(74, 104), frand(0, 6.28f)};
  for (auto &g : gulls) g.on = false;
  for (auto &f : fish) f.on = false;
  for (auto &r : rings) r.on = false;
}

static void spawnGull(float x, float y) {
  for (auto &g : gulls) if (!g.on) {
    float dir = x < L / 2 ? 1 : -1;
    g = {true, x, y, dir * frand(14, 22), frand(-3, -1), 0}; return;
  }
}
static void spawnFish(float x, float y) {
  for (auto &f : fish) if (!f.on) { f = {true, x, y, 0}; break; }
  for (auto &r : rings) if (!r.on) { r = {true, x, y, 0}; break; }
}

static void updateBoats(float dt) {
  for (int k = 0; k < 3; k++) {
    Boat &b = boats[k];
    switch (b.state) {
      case 0:
        b.x += b.vx * dt;
        b.y += sinf(t_ * 0.3f + k) * 0.02f;
        if (b.x > L + 20) b.x = -20;
        if (b.x < -20) b.x = L + 20;
        break;
      case 1: {   // zum Steg
        float dx = DOCK_X - b.x, dy = DOCK_Y - b.y, d = sqrtf(dx * dx + dy * dy);
        float sp = fminf(9.0f, 2 + d * 0.4f);
        if (d < 0.6f) { b.state = 2; b.t = 0; b.x = DOCK_X; b.y = DOCK_Y; }
        else { b.x += dx / d * sp * dt; b.y += dy / d * sp * dt; b.vx = dx > 0 ? fabsf(b.vx) : -fabsf(b.vx); }
      } break;
      case 2:
        b.t += dt;
        if (b.t > 90) { b.state = 3; b.t = 0; b.vx = 4.0f; }
        break;
      case 3:     // ablegen, zurueck in die Spur
        b.x += b.vx * dt;
        b.y += ((k == 0 ? 132 : 146) - b.y) * dt * 0.4f;
        if (b.x > L + 20) { b.state = 0; b.x = -20; }
        break;
    }
  }
}

static void updateLife(float dt) {
  for (auto &c : clouds) { c.x += dt * 1.2f; if (c.x - c.w > L + 4) { c.x = -c.w - 4; c.y = frand(22, 62); c.w = frand(14, 30); } }
  for (auto &g : gulls) if (g.on) {
    g.t += dt; g.x += g.vx * dt; g.y += g.vy * dt + sinf(g.t * 1.3f) * 0.05f;
    if (g.x < -10 || g.x > L + 10 || g.y < -10) g.on = false;
  }
  // tagsueber ab und zu von selbst eine Moewe
  if (light.day > 0.6f && (rnd() % 1000) < (int)(dt * 1000 / 25)) spawnGull(rnd() & 1 ? -5 : L + 5, frand(20, 60));
  for (auto &f : fish) if (f.on) { f.t += dt; if (f.t > 0.9f) f.on = false; }
  for (auto &r : rings) if (r.on) { r.t += dt; if (r.t > 1.6f) r.on = false; }
  // ab und zu springt von selbst ein Fisch
  if ((rnd() % 1000) < (int)(dt * 1000 / 40)) spawnFish(frand(80, 150), frand(110, 160));
  updateBoats(dt);
}

// ---------------------------------------------------------------- Zeichnen der Szene
static uint16_t skyRow[HOR];

static void drawSun() {
  float sr = SUNRISE[(month + 11) % 12], ss = SUNSET[(month + 11) % 12];
  float p = (hours - sr) / (ss - sr);
  if (p < -0.05f || p > 1.05f) return;
  float x = 44 + p * 116, y = HOR + 6 - sinf(clampf(p, 0, 1) * 3.1416f) * 74;
  float low = clampf(1 - (HOR - y) / 40.0f, 0, 1);
  uint16_t core = mixRGB(0xfff6d8, 0xffb050, low), rim = mixRGB(0xffe9a0, 0xff7a3a, low);
  float r = 7 + low * 2;
  for (int j = -12; j <= 12; j++) for (int i = -12; i <= 12; i++) {
    float d = sqrtf(i * i + j * j);
    int X = (int)x + i, Y = (int)y + j;
    if ((unsigned)X >= (unsigned)L || (unsigned)Y >= (unsigned)L || mat[Y * L + X] != M_SKY) continue;
    if (d <= r - 1.5f) lf[Y * L + X] = core;
    else if (d <= r) lf[Y * L + X] = rim;
    else if (d <= r + 4) lf[Y * L + X] = mix(lf[Y * L + X], rim, 0.25f * (1 - (d - r) / 4));
  }
}

static void drawMoonAndStars() {
  float night = 1 - light.day;
  if (night < 0.05f) return;
  for (auto &s : stars) {
    float tw = 0.6f + 0.4f * sinf(t_ * 1.7f + s.ph);
    lpSky(s.x, s.y, mix(lf[s.y * L + s.x], C(0xfff8e8), night * tw * ((s.ph & 3) ? 0.7f : 1.0f)));
  }
  // Mond zieht von links nach rechts durch die Nacht
  float h = hours < 12 ? hours + 24 : hours;
  float p = (h - 20.5f) / 10.0f;
  if (p < 0 || p > 1) return;
  float x = 50 + p * 130, y = 66 - sinf(p * 3.1416f) * 40;
  for (int j = -6; j <= 6; j++) for (int i = -6; i <= 6; i++) {
    float d = sqrtf(i * i + j * j);
    if (d > 5.5f) continue;
    uint16_t c = C(0xf2eee0);
    if ((i == -2 && j == -1) || (i == 1 && j == 2) || (i == 2 && j == -2)) c = C(0xcfcabb);
    lpSky((int)x + i, (int)y + j, mix(lf[((int)y + j) * L + (int)x + i], c, night));
  }
}

static void drawClouds() {
  uint32_t cw = lerpRGB(0x2a3352, 0xffffff, light.day);
  cw = lerpRGB(cw, 0xffc4a8, light.golden * 0.8f);
  uint16_t c = C(cw), cs = mix(c, C(light.skyMid), 0.35f);
  for (auto &cl : clouds) {
    for (int j = -6; j <= 4; j++) for (int i = (int)-cl.w; i <= (int)cl.w; i++) {
      float u = i / cl.w;
      float top = -3 - 3 * (1 - u * u) - 1.5f * sinf(i * 0.5f + cl.w);
      if (j < top || j > 3 - fabsf(u) * 2) continue;
      lpSky((int)cl.x + i, (int)cl.y + j, j >= 1 ? cs : c);
    }
  }
}

static void drawBoat(const Boat &b, int k) {
  int w, h; boatSize(b.kind, w, h);
  const char *const *art = boatArt(b.kind);
  bool flip = b.vx < 0;
  float bob = sinf(t_ * 2.0f + k * 1.7f) * 0.6f;
  int x0 = (int)(b.x - w / 2), y0 = (int)(b.y + bob) - h + 2;
  for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
    char ch = art[j][flip ? w - 1 - i : i];
    if (ch == '.') continue;
    int X = x0 + i, Y = y0 + j;
    if (b.state == 2) lp(X, Y, boatCol(ch));   // am Steg: vor dem Steg
    else lpBehind(X, Y, boatCol(ch));
  }
  // Kielwasser
  if (b.state != 2 && fabsf(b.vx) > 0.5f)
    for (int i = 1; i < 6; i++) {
      int X = (int)b.x - (b.vx > 0 ? 1 : -1) * (w / 2 + i * 2), Y = (int)(b.y + bob) + 2 + (i & 1);
      if ((((int)(t_ * 4)) + i) % 3) lpBehind(X, Y, C(0xe8f8f8));
    }
}

static void drawSeaLife() {
  for (auto &r : rings) if (r.on) {
    float rad = 1 + r.t * 6, a = 1 - r.t / 1.6f;
    for (float q = 0; q < 6.28f; q += 0.25f) {
      int X = (int)(r.x + cosf(q) * rad), Y = (int)(r.y + sinf(q) * rad * 0.4f);
      if ((unsigned)X < (unsigned)L && (unsigned)Y < (unsigned)L && mat[Y * L + X] == M_SEA) lf[Y * L + X] = mix(lf[Y * L + X], C(0xffffff), 0.6f * a);
    }
  }
  for (auto &f : fish) if (f.on) {
    float p = f.t / 0.9f;
    int X = (int)(f.x - 6 + p * 12), Y = (int)(f.y - sinf(p * 3.1416f) * 9);
    uint16_t c = C(0xcfd8e0), cd = C(0x7f8a98);
    lp(X, Y, c); lp(X + 1, Y, c); lp(X - 1, Y + (p < 0.5f ? 1 : -1), cd); lp(X + 2, Y, cd);
    if (p < 0.2f || p > 0.8f) lp(X, Y + 2, C(0xffffff));
  }
}

static void drawGulls() {
  uint16_t c = light.day > 0.5f ? C(0xffffff) : C(0x2a2a3a);
  if (light.golden > 0.4f) c = C(0x3a2a30);
  for (auto &g : gulls) if (g.on) {
    int x = (int)g.x, y = (int)g.y; bool up = ((int)(g.t * 5)) & 1;
    lp(x, y, c);
    if (up) { lp(x - 1, y - 1, c); lp(x - 2, y - 1, c); lp(x + 1, y - 1, c); lp(x + 2, y - 1, c); }
    else { lp(x - 1, y, c); lp(x - 2, y + 1, c); lp(x + 1, y, c); lp(x + 2, y + 1, c); }
  }
}

// Wellen am Strand: Schaumkante laeuft vor und zurueck
static void drawShore() {
  for (int x = 0; x < L; x++) {
    float by = beachY(x);
    float w = 1.5f + 1.5f * sinf(t_ * 0.9f + x * 0.045f) + 0.6f * sinf(t_ * 2.1f + x * 0.21f);
    int fy = (int)(by + w - 1.5f);
    for (int y = (int)by - 2; y <= fy; y++) {
      if ((unsigned)y >= (unsigned)L) continue;
      uint8_t m = mat[y * L + x];
      if (m != M_SEA && m != M_WET && m != M_SAND) continue;
      uint16_t foam = C(0xf4fbf8);
      lf[y * L + x] = (y == fy) ? foam : mix(lf[y * L + x], C(0x9fe6dc), 0.55f);
    }
  }
}

// Glitzern auf dem Wasser (Sonne/Mond-Spiegelung + kleine Lichtpunkte)
static void drawGlitter() {
  float sr = SUNRISE[(month + 11) % 12], ss = SUNSET[(month + 11) % 12];
  float p = (hours - sr) / (ss - sr);
  int tick4 = (int)(t_ * 5);
  if (p > -0.02f && p < 1.02f) {
    float sx = 44 + p * 116;
    float low = clampf(1 - sinf(clampf(p, 0, 1) * 3.1416f), 0, 1);
    uint16_t gc = mixRGB(0xfff6d8, 0xffa040, low);
    for (int y = HOR + 1; y < 180; y++) {
      float spread = 3 + (y - HOR) * (0.12f + low * 0.18f);
      for (int i = (int)-spread; i <= (int)spread; i++) {
        int X = (int)sx + i;
        if ((unsigned)X >= (unsigned)L || mat[y * L + X] != M_SEA) continue;
        uint32_t hh = hash2(X, y, tick4);
        if ((hh & 7) == 0 || (fabsf((float)i) < spread * 0.3f && (hh & 3) == 0)) lf[y * L + X] = mix(lf[y * L + X], gc, 0.75f);
      }
    }
  }
  // Mondspiegelung
  float night = 1 - light.day;
  float h = hours < 12 ? hours + 24 : hours, mp = (h - 20.5f) / 10.0f;
  if (night > 0.3f && mp >= 0 && mp <= 1) {
    float mx = 50 + mp * 130;
    for (int y = HOR + 1; y < 180; y++) for (int i = -3; i <= 3; i++) {
      int X = (int)mx + i;
      if ((unsigned)X >= (unsigned)L || mat[y * L + X] != M_SEA) continue;
      if ((hash2(X, y, tick4) & 7) == 0) lf[y * L + X] = mix(lf[y * L + X], C(0xdfe6f0), 0.6f * night);
    }
  }
  // feines Funkeln ueberall im Wasser (tagsueber)
  if (light.day > 0.3f)
    for (int k = 0; k < 40; k++) {
      int X = hash2(k, tick4, 5) % L, Y = HOR + 2 + hash2(tick4, k, 6) % 85;
      if (Y >= L) continue;
      if (mat[Y * L + X] == M_SEA) lf[Y * L + X] = mix(lf[Y * L + X], C(0xffffff), 0.5f * light.day);
    }
}

// Lichter bei Nacht (werden nicht abgedunkelt)
static bool lightsOn() {
  float h = hours;
  return (1 - light.day) > 0.35f && !(h > 1.5f && h < 6.0f);
}
static void drawNightLights() {
  float night = 1 - light.day;
  if (night < 0.25f) return;
  float k = clampf((night - 0.25f) / 0.4f, 0, 1);
  bool on = lightsOn();
  // Fenster der Finca
  if (fincaLight && on)
    for (int i = 0; i < L * 80; i++) if (mat[i] == M_WINDOW) {
      int x = i % L, y = i / L;
      uint16_t w = (hash2(x / 2, y / 4, (int)(t_ * 0.2f)) & 15) == 0 ? C(0xffc860) : C(0xffd77a);
      lf[i] = mix(lf[i], w, k);
    }
  // Lichterkette am Steg
  if (on)
    for (int y = 154; y < 182; y += 4) {
      static const uint32_t cols[] = {0xffd060, 0xff8a6a, 0x9ad8ff, 0xffe8b0};
      int idx = (y / 4) & 3;
      float blink = 0.75f + 0.25f * sinf(t_ * 2 + y);
      lp(135, y, mix(lf[y * L + 135], C(cols[idx]), k * blink));
      lp(146, y + 2, mix(lf[(y + 2) * L + 146], C(cols[(idx + 2) & 3]), k * blink));
    }
  // Topplichter der Boote
  for (int b = 0; b < 3; b++) {
    int w, h; boatSize(boats[b].kind, w, h);
    int X = (int)boats[b].x, Y = (int)boats[b].y - h + 2;
    if ((unsigned)X < (unsigned)L && (unsigned)Y < (unsigned)L && (mat[Y * L + X] == M_SKY || mat[Y * L + X] == M_SEA || boats[b].state == 2))
      lf[Y * L + X] = mix(lf[Y * L + X], C(0xfff2c0), k);
  }
  // Gluehwuermchen im Gebuesch
  for (auto &f : flies) {
    float b = 0.5f + 0.5f * sinf(t_ * 1.3f + f.ph * 3);
    if (b < 0.4f) continue;
    float x = f.x + sinf(t_ * 0.4f + f.ph) * 5, y = f.y + cosf(t_ * 0.33f + f.ph * 2) * 3;
    lp((int)x, (int)y, mix(lf[(int)y * L + (int)x], C(0xd8ff7a), k * b));
  }
}

// ---------------------------------------------------------------- Uhr stellen
static bool clockUi = false;
static int setH = 12, setM = 0;
static uint32_t clockUiIdleMs = 0;
static bool clockChanged = false;

static void textC(const char *s, int cy, int sc, uint16_t c) {   // zentriert mit Rand
  int w = textWidth(s, sc), x = (SCR - w) / 2, y = cy - 7 * sc / 2;
  for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++)
    if (dx || dy) textRaw(s, x + dx * 2, y + dy * 2, sc, K_OUT);
  textRaw(s, x, y, sc, c);
}
static void arrow(int cx, int cy, bool upw, uint16_t c) {
  for (int j = 0; j < 14; j++) { int w = upw ? j : 13 - j; rect(cx - w * 2, cy - 14 + j * 2, w * 4 + 2, 2, c); }
}
static void drawClockUi() {
  for (int i = 0; i < SCR * SCR; i++) fb[i] = mix(fb[i], 0, 0.62f);
  char buf[8]; snprintf(buf, sizeof buf, "%02d:%02d", setH, setM);
  textC("UHR STELLEN", 128, 3, C(0xffd77a));
  textC(buf, 233, 9, C(0xfff8ee));
  uint16_t a = C(0xb9d8e8);
  arrow(158, 168, true, a); arrow(158, 312, false, a);
  arrow(308, 168, true, a); arrow(308, 312, false, a);
  rect(193, 362, 80, 40, C(0x2fa39a));
  textC("OK", 383, 3, C(0x0b1a18));
}
// Tipp im Uhr-Menue
// Pfeil-Zonen: links Stunden, rechts Minuten; oben +, unten -. Liefert 0 = kein Pfeil.
static int clockZone(int x, int y) {
  if (y < 120 || y > 350) return 0;
  bool left = x < 233, upper = y < 233;
  return left ? (upper ? 1 : 2) : (upper ? 3 : 4);
}
static void clockStep(int zone) {
  clockUiIdleMs = 0;
  if (zone == 1) setH = (setH + 1) % 24;
  else if (zone == 2) setH = (setH + 23) % 24;
  else if (zone == 3) setM = (setM + 1) % 60;
  else if (zone == 4) setM = (setM + 59) % 60;
}
static void clockOk() {
  clockUi = false; clockChanged = true;
  hours = setH + setM / 60.0f; clockSetHours = hours; clockSetMs = 0;   // sofort uebernehmen
}

// ---------------------------------------------------------------- Eingabe
static bool wasDown = false;
static uint32_t downMs = 0;
static int downX = 0, downY = 0;
static bool longFired = false;
static uint32_t lastMs = 0;

static void handleTap(int sx, int sy) {
  int x = sx / 2, y = sy / 2;
  if ((unsigned)x >= (unsigned)L || (unsigned)y >= (unsigned)L) return;
  // Boot getroffen?
  for (int k = 0; k < 3; k++) {
    Boat &b = boats[k]; int w, h; boatSize(b.kind, w, h);
    if (fabsf(x - b.x) < w / 2 + 6 && y > b.y - h - 4 && y < b.y + 6) {
      if (b.state == 0 && b.kind != 1) {
        bool free = true; for (auto &o : boats) if (&o != &b && (o.state == 1 || o.state == 2)) free = false;
        if (free) { b.state = 1; return; }
      }
      if (b.state == 2) { b.state = 3; b.t = 0; b.vx = 4.0f; return; }
      spawnFish(b.x + 8, b.y + 3);
      return;
    }
  }
  uint8_t m = mat[y * L + x];
  if (x > 18 && x < 60 && y > 42 && y < 76) { fincaLight = !fincaLight; return; }
  if (m == M_SEA || m == M_WET) { spawnFish(x, y); return; }
  if (m == M_SKY) { spawnGull(x, y); return; }
}

static uint32_t nextRepeat = 0;
static int heldZone = 0;

static void input(uint32_t now, const Touch *pts, int n) {
  bool down = n > 0;
  int x = down ? pts[0].x : downX, y = down ? pts[0].y : downY;
  bool pressStartedInMenu = false;
  if (down && !wasDown) {
    downMs = now; downX = x; downY = y; longFired = false;
    // im Uhr-Menue zaehlt ein Pfeil sofort beim Aufsetzen, Halten zaehlt weiter
    if (clockUi) {
      pressStartedInMenu = true;
      heldZone = clockZone(x, y);
      if (heldZone) { clockStep(heldZone); nextRepeat = now + 450; }
    }
  }
  if (clockUi && down && heldZone && !pressStartedInMenu && now >= nextRepeat) {
    clockStep(heldZone);
    uint32_t held = now - downMs;
    nextRepeat = now + (held > 2500 ? 40 : (held > 1200 ? 90 : 160));   // immer schneller
  }
  if (down && !longFired && !clockUi && now - downMs > 1500) {
    int dx = x - downX, dy = y - downY;
    if (dx * dx + dy * dy < 30 * 30) {
      longFired = true; clockUi = true; clockUiIdleMs = 0; heldZone = 0;
      setH = (int)hours % 24; setM = (int)(hours * 60) % 60;
    }
  }
  if (!down && wasDown && !longFired) {
    if (clockUi) {
      if (!heldZone && downY > 350 && downX > 180 && downX < 286) clockOk();
      heldZone = 0;
    } else if (now - downMs < 600) handleTap(downX, downY);
  }
  wasDown = down;
}

// ---------------------------------------------------------------- Takt
static int shiftX = 0, shiftY = 0;     // gegen Einbrennen: Bild wandert langsam um ein paar Pixel
static float shiftT = 0;
static uint8_t bright = 180;

void begin(uint16_t *fbuf, uint16_t *work, uint32_t seed) {
  fb = fbuf; rng = seed ? seed : 1;
  uint8_t *w = (uint8_t *)work;
  base = (uint16_t *)w; w += L * L * 2;
  lf = (uint16_t *)w; w += L * L * 2;
  mat = w;
  buildScene();
  initWorld();
  lastMs = 0;
}

void setClock(int h, int m, int s, int mon) {
  if (testHours >= 0) return;
  clockSetHours = h + m / 60.0f + s / 3600.0f; clockSetMs = lastMs;
  if (mon >= 1 && mon <= 12) month = mon;
}
void testSetHours(float h, int mon) { testHours = h; if (h >= 0) hours = h; if (mon >= 1 && mon <= 12) month = mon; }
bool takeClockChange(int &h, int &m) { if (!clockChanged) return false; clockChanged = false; h = setH; m = setM; return true; }
uint8_t wantBrightness() { return bright; }

void tick(uint32_t now, const Touch *pts, int n) {
  if (!lastMs) { lastMs = now; clockSetMs = now; }
  float dt = (now - lastMs) / 1000.0f; lastMs = now;
  if (dt > 0.1f) dt = 0.1f;
  t_ += dt;
  if (testHours >= 0) hours = testHours;
  else { hours = clockSetHours + (now - clockSetMs) / 3600000.0f; while (hours >= 24) hours -= 24; }

  input(now, pts, n);
  if (clockUi) { clockUiIdleMs += (uint32_t)(dt * 1000); if (clockUiIdleMs > 20000) clockUi = false; }

  computeLight();
  updateLife(dt);

  // Himmel-Verlauf je Zeile
  for (int y = 0; y < HOR; y++) {
    float v = (float)y / HOR;
    uint32_t c = v < 0.55f ? lerpRGB(light.skyTop, light.skyMid, v / 0.55f) : lerpRGB(light.skyMid, light.skyHor, (v - 0.55f) / 0.45f);
    skyRow[y] = C(c);
  }
  // 1) feste Szene + Himmel
  for (int y = 0; y < L; y++) {
    const uint16_t *b = base + y * L; const uint8_t *m = mat + y * L; uint16_t *o = lf + y * L;
    for (int x = 0; x < L; x++) o[x] = m[x] == M_SKY ? skyRow[y] : b[x];
  }
  // 2) Himmel-Objekte (hinter den Felsen)
  drawMoonAndStars();
  drawSun();
  drawClouds();
  // 3) Wasser-Leben
  drawShore();
  for (int k = 0; k < 3; k++) if (boats[k].state != 2) drawBoat(boats[k], k);
  drawSeaLife();
  for (int k = 0; k < 3; k++) if (boats[k].state == 2) drawBoat(boats[k], k);
  // 4) Licht der Tageszeit auf alles ausser Himmel
  for (int i = 0; i < L * L; i++) if (mat[i] != M_SKY) lf[i] = lit(lf[i]);
  // 5) Dinge, die selbst leuchten oder hell glitzern
  drawGlitter();
  drawGulls();
  drawNightLights();

  // gegen Einbrennen: alle 2 Minuten ein Pixel weiter auf einer kleinen Runde
  shiftT += dt;
  if (shiftT > 120) {
    shiftT = 0;
    static const int8_t path[8][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}};
    static int p = 0; p = (p + 1) & 7; shiftX = path[p][0]; shiftY = path[p][1];
  }
  // 6) doppelt skaliert ins Bild
  static int16_t xmap[SCR]; static int mapFor = 99;
  if (mapFor != shiftX) {
    mapFor = shiftX;
    for (int x = 0; x < SCR; x++) { int lx = (x - shiftX) >> 1; xmap[x] = lx < 0 ? 0 : (lx >= L ? L - 1 : lx); }
  }
  int prevLy = -1;
  for (int y = 0; y < SCR; y++) {
    int ly = (y - shiftY) >> 1; if (ly < 0) ly = 0; if (ly >= L) ly = L - 1;
    uint16_t *d = fb + y * SCR;
    if (ly == prevLy) { memcpy(d, d - SCR, SCR * 2); continue; }   // zweite Zeile = Kopie
    prevLy = ly;
    const uint16_t *src = lf + ly * L;
    for (int x = 0; x < SCR; x++) d[x] = src[xmap[x]];
  }
  if (clockUi) drawClockUi();
  else if (t_ < 8) {
    float k = t_ < 6 ? 1 : (8 - t_) / 2;
    textC("LANGE DRÜCKEN = UHR STELLEN", 300, 2, mix(C(0x404040), C(0xfff8ee), k));
  }
  // Helligkeit: tags hell, nachts sanft
  bright = (uint8_t)(70 + 120 * light.day);
}

Debug debug() {
  Debug d; memset(&d, 0, sizeof d);
  d.hours = hours; d.daylight = light.day; d.clockUi = clockUi; d.lightsOn = lightsOn() && fincaLight;
  for (int k = 0; k < 3; k++) { d.boatState[k] = boats[k].state; d.boatX[k] = boats[k].x; d.boatY[k] = boats[k].y; if (boats[k].state == 2) d.boatsDocked++; }
  return d;
}

}  // namespace kw
