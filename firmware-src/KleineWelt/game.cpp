// Kleine Welt - zehn kleine Mallorca-Szenen als Wohlfuehl-Deko fuer die Wand.
//
// Alle 10 Minuten wechselt die Szene mit einer langsamen Ueberblendung (schont
// nebenbei das AMOLED). Jede Szene folgt der echten Uhrzeit: Sonnenauf- und
// -untergang je nach Monat, goldene Stunde, Nacht mit Mond, Sternen und Lichtern.
//
// Antippen (alles freiwillig):
//   Boot -> legt am Steg an (wo es einen gibt) | Meer -> Fisch springt
//   Himmel -> Vogel fliegt | Fenster -> Licht an/aus
//   waagerecht wischen -> naechste/vorige Szene | lange halten -> Uhr stellen
//
// Gezeichnet wird in 233x233 "Kunstpixeln", doppelt skaliert. Alle Grafik ist
// eigene Pixelkunst bzw. wird im Code erzeugt.
#include "game.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
static float vnoise(float x, float y, int seed) {
  int xi = (int)floorf(x), yi = (int)floorf(y); float fx = x - xi, fy = y - yi;
  auto h = [&](int a, int b) { return (hash2(a, b, seed) & 1023) / 1023.0f; };
  float a = h(xi, yi), b = h(xi + 1, yi), c = h(xi, yi + 1), d = h(xi + 1, yi + 1);
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  float top = a + (b - a) * fx, bot = c + (d - c) * fx;
  return top + (bot - top) * fy;
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
constexpr int L = 233;
enum Mat : uint8_t { M_SKY, M_SEA, M_ROCK, M_SCRUB, M_SAND, M_WET, M_OBJ, M_WINDOW, M_JETTY, M_GLOWWALL };
static uint16_t *base = nullptr;     // Tagesfarben der festen Szene
static uint8_t *mat = nullptr;       // Material je Kunstpixel
static uint16_t *lf = nullptr;       // aktuelles Bild in Kunstpixeln
static uint16_t *oldf = nullptr;     // letztes Bild der vorigen Szene (Ueberblendung)
static int HOR = 92;                 // Horizont der aktuellen Szene

static inline void lp(int x, int y, uint16_t c) { if ((unsigned)x < (unsigned)L && (unsigned)y < (unsigned)L) lf[y * L + x] = c; }
static inline void bp(int x, int y, uint16_t c, Mat m = M_OBJ) {
  if ((unsigned)x < (unsigned)L && (unsigned)y < (unsigned)L) { base[y * L + x] = c; mat[y * L + x] = m; }
}
static inline uint8_t matAt(int x, int y) { return ((unsigned)x < (unsigned)L && (unsigned)y < (unsigned)L) ? mat[y * L + x] : M_ROCK; }
static inline void lpBehind(int x, int y, uint16_t c) {
  if ((unsigned)x < (unsigned)L && (unsigned)y < (unsigned)L) { uint8_t m = mat[y * L + x]; if (m == M_SKY || m == M_SEA) lf[y * L + x] = c; }
}
static inline void lpSky(int x, int y, uint16_t c) {
  if ((unsigned)x < (unsigned)L && (unsigned)y < (unsigned)L && mat[y * L + x] == M_SKY) lf[y * L + x] = c;
}
static inline void lpOn(int x, int y, uint16_t c, uint8_t onMat) {
  if ((unsigned)x < (unsigned)L && (unsigned)y < (unsigned)L && mat[y * L + x] == onMat) lf[y * L + x] = c;
}

// ---------------------------------------------------------------- Werkzeugkasten zum Zeichnen der Szenen
static void fillRect(int x0, int y0, int x1, int y1, uint16_t c, Mat m = M_OBJ) {
  for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) bp(x, y, c, m);
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
// Strecke (fuer Masten, Wege, Linien)
static void line(float x0, float y0, float x1, float y1, uint16_t c, Mat m = M_OBJ) {
  int n = (int)fmaxf(fabsf(x1 - x0), fabsf(y1 - y0)) + 1;
  for (int k = 0; k <= n; k++) { float t = (float)k / n; bp((int)roundf(x0 + (x1 - x0) * t), (int)roundf(y0 + (y1 - y0) * t), c, m); }
}
// gefuelltes Vieleck
static void poly(const float *xy, int n, uint16_t c, Mat m = M_OBJ) {
  float ymin = 1e9, ymax = -1e9;
  for (int i = 0; i < n; i++) { ymin = fminf(ymin, xy[i * 2 + 1]); ymax = fmaxf(ymax, xy[i * 2 + 1]); }
  for (int y = (int)ymin; y <= (int)ymax; y++) {
    float xs[16]; int k = 0; float yc = y + 0.5f;
    for (int i = 0; i < n && k < 16; i++) {
      float ax = xy[i * 2], ay = xy[i * 2 + 1], bx = xy[((i + 1) % n) * 2], by = xy[((i + 1) % n) * 2 + 1];
      if ((ay <= yc && by > yc) || (by <= yc && ay > yc)) xs[k++] = ax + (yc - ay) / (by - ay) * (bx - ax);
    }
    for (int a = 0; a < k; a++) for (int b = a + 1; b < k; b++) if (xs[b] < xs[a]) { float t = xs[a]; xs[a] = xs[b]; xs[b] = t; }
    for (int a = 0; a + 1 < k; a += 2) for (int x = (int)ceilf(xs[a] - 0.5f); x <= (int)floorf(xs[a + 1] - 0.5f); x++) bp(x, y, c, m);
  }
}
// Kalkstein-Farbe mit Flecken; edge = Abstand zur Kante (fuer helle Lichtkante)
static uint16_t limestone(int x, int y, float edge, int seed = 21) {
  float n = vnoise(x / 6.0f, y / 4.0f, seed);
  uint32_t c = n > 0.62f ? 0xd2b48c : (n > 0.38f ? 0xbea07c : 0x9e8264);
  if (edge >= 0 && edge < 2.5f) c = 0xe2caa2;
  else if (edge >= 2.5f && edge < 4) c = 0x8a6e54;
  if ((hash2(x, y, seed) & 31) == 0) c = 0x7c6450;
  return C(c);
}
static uint16_t scrubCol(int x, int y) { return (hash2(x, y, 5) & 3) ? C(0x5e7c36) : C(0x76963f); }
// Meer zwischen Horizont und Kueste: tief am Horizont, tuerkis zur Kueste
static uint16_t seaCol(int x, int y, float shoreY) {
  float v = clampf((y - HOR) / fmaxf(1, shoreY - HOR), 0, 1);
  uint32_t col = v < 0.55f ? lerpRGB(0x1f5f8f, 0x2a8fb8, v / 0.55f) : lerpRGB(0x2a8fb8, 0x45c9c4, (v - 0.55f) / 0.45f);
  if (shoreY - y < 7) col = lerpRGB(col, 0x8fe3d2, (7 - (shoreY - y)) / 7.0f);
  if ((hash2(x, y, 11) & 31) == 0) col = lerpRGB(col, 0xffffff, 0.12f);
  return C(col);
}
static uint16_t sandCol(int x, int y) { return C((hash2(x, y, 3) & 7) == 0 ? 0xdcc497 : 0xead6aa); }
// Gebirge: Kammlinie aus Rauschen, nur in den Himmel gezeichnet (davorliegendes bleibt)
static void mountains(float baseY, float amp, float scale, int seed, uint32_t col, uint32_t colShade) {
  for (int x = 0; x < L; x++) {
    float n = vnoise(x / scale, 0.5f, seed) * 0.7f + vnoise(x / (scale * 0.35f), 1.5f, seed + 1) * 0.3f;
    int top = (int)(baseY - amp * n);
    for (int y = top; y <= (int)baseY && y < L; y++) {
      if (matAt(x, y) != M_SKY) continue;
      float slope = vnoise(x / (scale * 0.2f), y / 6.0f, seed + 2);
      bp(x, y, slope > 0.55f ? C(colShade) : C(col), M_ROCK);
    }
  }
}

// Baeume und Pflanzen
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
static void bush(int x, int y, int r) { ell(x, y, r + 1, r * 0.7f, C(0x3e5e30)); ell(x - 1, y - 1, r * 0.8f, r * 0.5f, C(0x557a3c)); }
// Olivenbaum: knorriger Stamm, silbriggruene Krone aus Klecksen
static void olive(int x, int y, float s) {
  const uint16_t tr = C(0x6a5440), trD = C(0x4a3a2c), g1 = C(0x7d8f5a), g2 = C(0x9aaa74), g3 = C(0x5f7048);
  for (int k = 0; k < (int)(7 * s); k++) { bp(x + (k > 3 ? 1 : 0), y - k, tr); bp(x + 1 + (k > 3 ? 1 : 0), y - k, trD); }
  bp(x - 1, y, trD); bp(x + 2, y, trD);
  int cy = y - (int)(9 * s);
  for (int k = 0; k < 7; k++) {
    float ox = ((int)(hash2(x, k, 7) % 13) - 6) * s, oy = ((int)(hash2(k, x, 8) % 7) - 3) * s;
    ell(x + 1 + ox, cy + oy, 4 * s, 3 * s, k < 2 ? g3 : g1);
  }
  for (int k = 0; k < 10; k++) bp(x + 1 + ((int)(hash2(x, k, 9) % 15) - 7) * s, cy - 2 + ((int)(hash2(k, y, 9) % 5) - 2), g2);
}
// Mandelbaum (bluehend im Februar/Maerz) oder Orangenbaum
static void almond(int x, int y, float s, bool bloom) {
  const uint16_t tr = C(0x5e4636);
  line(x, y, x - 1, y - 7 * s, tr); line(x, y - 4 * s, x + 4 * s, y - 9 * s, tr); line(x, y - 5 * s, x - 4 * s, y - 9 * s, tr);
  for (int k = 0; k < 9; k++) {
    float ox = ((int)(hash2(x, k, 17) % 15) - 7) * s, oy = ((int)(hash2(k, x, 18) % 7) - 3) * s;
    ell(x + ox, y - 10 * s + oy, 3.5f * s, 2.5f * s, bloom ? ((k & 1) ? C(0xfbe4ee) : C(0xf3c2d4)) : C(0x6e8f4a));
  }
}
static void orangeTree(int x, int y, float s) {
  line(x, y, x, y - 4 * s, C(0x5e4636));
  ell(x, y - 8 * s, 6 * s, 5 * s, C(0x2f5e30)); ell(x - 1, y - 9 * s, 4.5f * s, 3.5f * s, C(0x3f7a3c));
  for (int k = 0; k < 6; k++) bp(x - 4 + (int)(hash2(x, k, 19) % 9), y - 11 + (int)(hash2(k, y, 19) % 6), C(0xf39a2a));
}
static void palm(int bx, int by, float s, int lean) {
  const uint16_t tr = C(0x8a6a44), trD = C(0x5e4630), fr = C(0x3f7a3a), frL = C(0x6aa64c);
  float tx = bx, ty = by; int len = (int)(70 * s);
  for (int k = 0; k < len; k++) {
    float v = (float)k / len;
    tx = bx + lean * v * v; ty = by - len * v;
    for (int i = -2; i <= 1; i++) bp((int)tx + i, (int)ty, (k % 4 == 0) ? trD : tr);
  }
  int cx = (int)tx, cy = (int)ty;
  const float ang[] = {-2.9f, -2.4f, -1.8f, -1.2f, -0.6f, -0.1f, 0.4f};
  for (float a : ang) {
    int fl = (int)(30 * s);
    for (int k = 0; k < fl; k++) {
      float d = k * 1.0f, droop = k * k * 0.028f / s;
      int x = cx + (int)(cosf(a) * d), y = cy + (int)(sinf(a) * d * 0.6f + droop);
      bp(x, y, frL); bp(x, y + 1, fr);
      if (k > 3 && k < fl - 3) { int ln = k < fl / 2 ? 3 : 2; for (int q = 1; q <= ln; q++) { bp(x - (k & 1), y - q, fr); bp(x + (k & 1), y + 1 + q, fr); } }
    }
  }
  ell(cx, cy + 2, 3, 2, C(0x6e4a2a));
}
// Haus: Wand, Ziegeldach, Fenster mit Laeden (Fenster leuchten nachts)
static void house(int x, int yb, int w, int h, uint32_t wall, uint32_t shutter, bool gable = true, int nwin = 2) {
  uint16_t wl = C(wall), ws = mix(C(wall), 0, 0.15f), rf = C(0xc8643c), rfD = C(0x8e3f24);
  fillRect(x, yb - h, x + w - 1, yb, wl);
  fillRect(x + w - 2, yb - h, x + w - 1, yb, ws);
  if (gable) for (int j = 0; j < (w + 3) / 2; j++) for (int i = x - 1 + j; i <= x + w - j; i++) bp(i, yb - h - 1 - j, (j % 2) ? rfD : rf);
  else fillRect(x - 1, yb - h - 1, x + w, yb - h, rfD);
  for (int k = 0; k < nwin; k++) {
    int wx = x + 2 + k * (w - 4) / (nwin > 1 ? nwin - 1 : 1) - (k == nwin - 1 && nwin > 1 ? 2 : 0), wy = yb - h + 3;
    if (wx + 1 >= x + w - 1) wx = x + w - 4;
    fillRect(wx, wy, wx + 1, wy + 2, C(0x2b3a5c), M_WINDOW);
    if (shutter) { bp(wx - 1, wy, C(shutter)); bp(wx - 1, wy + 1, C(shutter)); bp(wx - 1, wy + 2, C(shutter)); bp(wx + 2, wy, C(shutter)); bp(wx + 2, wy + 1, C(shutter)); bp(wx + 2, wy + 2, C(shutter)); }
    if (h > 12) fillRect(wx, wy + 6, wx + 1, wy + 8, C(0x2b3a5c), M_WINDOW);
  }
}
static void stoneWall(int x0, int x1, int y) {
  for (int x = x0; x <= x1; x++) for (int j = 0; j < 3; j++) {
    uint32_t h = hash2(x / 3 + j, y + j, 77);
    bp(x, y + j, C((h & 3) == 0 ? 0x8a7a64 : ((h & 3) == 1 ? 0xb8a688 : 0xa3927a)));
  }
}

// ---------------------------------------------------------------- Lichter, Boote, Tiere (pro Szene eingerichtet)
struct LightPt { uint8_t x, y; uint32_t col; uint8_t blink; };
static LightPt lights[48]; static int nLights = 0;
static void addLight(int x, int y, uint32_t col, int blink = 0) { if (nLights < 48 && x >= 0 && y >= 0 && x < L && y < L) lights[nLights++] = {(uint8_t)x, (uint8_t)y, col, (uint8_t)blink}; }
struct Fly { float x, y, ph; };
static Fly flies[10]; static int nFlies = 0;
static void addFlies(int x0, int y0, int x1, int y1) { nFlies = 10; for (auto &f : flies) f = {frand(x0, x1), frand(y0, y1), frand(0, 6.28f)}; }

struct Boat { int kind; float x, y, vx; int state; float t; float lane; bool anchored; };
static Boat boats[5]; static int nBoats = 0;
static float dockX = -1, dockY = -1;
static void addBoat(int kind, float x, float lane, float vx, bool anchored = false) {
  if (nBoats < 5) boats[nBoats++] = {kind, x, lane, vx, 0, 0, lane, anchored};
}
// Schaumpunkte an Felsen (fuer Wellen an Steilkuesten)
static uint16_t foamPts[700]; static int nFoam = 0;
static int16_t shoreY[L];   // Strandkante je Spalte (-1 = keine)

// ---------------------------------------------------------------- Zeit und Licht
static float hours = 12.0f;
static int month = 7;
static float testHours = -1;
static uint32_t clockSetMs = 0;
static float clockSetHours = 12.0f;
static const float SUNRISE[12] = {8.1f, 7.75f, 7.2f, 7.4f, 6.85f, 6.5f, 6.65f, 7.1f, 7.6f, 8.0f, 7.6f, 8.0f};
static const float SUNSET[12] = {17.7f, 18.25f, 18.75f, 20.25f, 20.75f, 21.25f, 21.25f, 20.8f, 20.1f, 19.3f, 17.75f, 17.5f};
static bool almondBloom() { return month == 2 || month == 3; }

struct Light { float day, golden, dawn; int mr, mg, mb; uint32_t skyTop, skyMid, skyHor; };
static Light light;
static void computeLight() {
  float sr = SUNRISE[(month + 11) % 12], ss = SUNSET[(month + 11) % 12];
  float h = hours;
  float up = clampf((h - (sr - 0.5f)) / 1.0f, 0, 1), down = clampf(((ss + 0.6f) - h) / 1.1f, 0, 1);
  float d = fminf(up, down);
  float gold = fmaxf(0, 1 - fabsf(h - (ss - 0.1f)) / 1.1f);
  float dawn = fmaxf(0, 1 - fabsf(h - (sr + 0.2f)) / 0.9f);
  light.day = d; light.golden = gold; light.dawn = dawn;
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
static float sunP() { float sr = SUNRISE[(month + 11) % 12], ss = SUNSET[(month + 11) % 12]; return (hours - sr) / (ss - sr); }
static float sunX(float p) { return 44 + p * 116; }
static float moonP() { float h = hours < 12 ? hours + 24 : hours; return (h - 20.5f) / 10.0f; }
static bool lightsOn() { return (1 - light.day) > 0.35f && !(hours > 1.5f && hours < 6.0f); }
static float nightK() { return clampf(((1 - light.day) - 0.25f) / 0.4f, 0, 1); }

static float t_ = 0;
static bool windowsOn = true;

// ---------------------------------------------------------------- Szenen
struct SceneDef {
  const char *name;
  int hor;
  void (*build)();
  void (*animate)(float dt);   // bewegte Dinge, vor dem Tageslicht gezeichnet
  void (*glow)(float k);       // eigene Lichter, nach dem Tageslicht (k = Nachtstaerke)
};
#include "scenes.h"
constexpr int NSCENES = sizeof(SCENES) / sizeof(SCENES[0]);
static int scene = -1;
static int sceneOffset = 0;       // durch Wischen verstellt
static float fadeT = 0;           // Ueberblendung laeuft (Sekunden uebrig)
static const float FADE = 3.0f;

// ---------------------------------------------------------------- Bewohner (allgemein)
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

static const char *BOAT_BIG[13] = {
  "......k.....", "......kw....", ".....Wkww...", ".....Wkwww..", "....WWkwww..", "....WWkwwww.",
  "...WWWkwwww.", "...WWWkwwwww", "..WWWWkwwwww", "......k.....", "bbbbbbbbbbbb", ".hhhhhhhhhh.", "..hhhhhhhh.."};
static const char *BOAT_SMALL[8] = {"...k...", "..Wkw..", "..Wkww.", ".WWkww.", ".WWkwww", "...k...", "bbbbbbb", ".hhhhh."};
static const char *BOAT_LLAUT[7] = {"....kttt....", "....tWWt....", "....tttt....", "nnnnnnnnnnnn", "gggggggggggg", ".nnnnnnnnnn.", "..NNNNNNNN.."};
static const char *BOAT_PADDLE[6] = {"..k.....", "..sk....", ".kTTk...", "..TT..k.", "..k..k..", "yyyyyyyy"};
static uint16_t boatCol(char ch) {
  switch (ch) {
    case 'k': return C(0x3a3030); case 'w': return C(0xfbf8f0); case 'W': return C(0xd0d8e4);
    case 'b': return C(0x2f6fb0); case 'h': return C(0xf2efe8); case 't': return C(0xe8e2d6);
    case 'n': return C(0xb07a46); case 'N': return C(0x6e4630); case 'g': return C(0x3c8a6a);
    case 's': return C(0xe8b088); case 'T': return C(0xd8403c); case 'y': return C(0xf2c230);
    default: return 0;
  }
}
static void boatSize(int kind, int &w, int &h) {
  if (kind == 0) { w = 12; h = 13; } else if (kind == 1) { w = 7; h = 8; } else if (kind == 2) { w = 12; h = 7; } else { w = 8; h = 6; }
}
static const char *const *boatArt(int kind) { return kind == 0 ? BOAT_BIG : (kind == 1 ? BOAT_SMALL : (kind == 2 ? BOAT_LLAUT : BOAT_PADDLE)); }

static void spawnGull(float x, float y) {
  for (auto &g : gulls) if (!g.on) { float dir = x < L / 2 ? 1 : -1; g = {true, x, y, dir * frand(14, 22), frand(-3, -1), 0}; return; }
}
static void spawnFish(float x, float y) {
  for (auto &f : fish) if (!f.on) { f = {true, x, y, 0}; break; }
  for (auto &r : rings) if (!r.on) { r = {true, x, y, 0}; break; }
}
static bool hasSea = false;

static void updateBoats(float dt) {
  for (int k = 0; k < nBoats; k++) {
    Boat &b = boats[k];
    switch (b.state) {
      case 0:
        if (b.anchored) break;
        b.x += b.vx * dt;
        if (b.x > L + 20) b.x = -20;
        if (b.x < -20) b.x = L + 20;
        break;
      case 1: {
        float dx = dockX - b.x, dy = dockY - b.y, d = sqrtf(dx * dx + dy * dy);
        float sp = fminf(9.0f, 2 + d * 0.4f);
        if (d < 0.6f) { b.state = 2; b.t = 0; b.x = dockX; b.y = dockY; }
        else { b.x += dx / d * sp * dt; b.y += dy / d * sp * dt; b.vx = dx > 0 ? fabsf(b.vx) : -fabsf(b.vx); }
      } break;
      case 2:
        b.t += dt;
        if (b.t > 90) { b.state = 3; b.t = 0; b.vx = 4.0f; }
        break;
      case 3:
        b.x += b.vx * dt;
        b.y += (b.lane - b.y) * dt * 0.4f;
        if (b.x > L + 20) { b.state = 0; b.x = -20; b.y = b.lane; }
        break;
    }
  }
}

static void initWorld() {
  for (int k = 0; k < 4; k++) clouds[k] = {frand(0, L), frand(14, (float)HOR - 30), frand(14, 30)};
  for (auto &s : stars) {
    int x = 0, y = 0;
    for (int tries = 0; tries < 50; tries++) { x = rnd() % L; y = rnd() % (HOR - 4); if (mat[y * L + x] == M_SKY) break; }
    s = {(uint8_t)x, (uint8_t)y, (uint8_t)(rnd() & 255)};
  }
  for (auto &g : gulls) g.on = false;
  for (auto &f : fish) f.on = false;
  for (auto &r : rings) r.on = false;
}

static void updateLife(float dt) {
  for (auto &c : clouds) { c.x += dt * 1.2f; if (c.x - c.w > L + 4) { c.x = -c.w - 4; c.y = frand(14, (float)HOR - 30); c.w = frand(14, 30); } }
  for (auto &g : gulls) if (g.on) {
    g.t += dt; g.x += g.vx * dt; g.y += g.vy * dt + sinf(g.t * 1.3f) * 0.05f;
    if (g.x < -10 || g.x > L + 10 || g.y < -10) g.on = false;
  }
  if (light.day > 0.6f && (rnd() % 1000) < (int)(dt * 1000 / 25)) spawnGull(rnd() & 1 ? -5 : L + 5, frand(20, HOR - 25));
  for (auto &f : fish) if (f.on) { f.t += dt; if (f.t > 0.9f) f.on = false; }
  for (auto &r : rings) if (r.on) { r.t += dt; if (r.t > 1.6f) r.on = false; }
  if (hasSea && (rnd() % 1000) < (int)(dt * 1000 / 40))
    for (int tries = 0; tries < 20; tries++) {
      int x = 20 + rnd() % (L - 40), y = HOR + 8 + rnd() % (L - HOR - 8);
      if (matAt(x, y) == M_SEA && matAt(x, y - 9) != M_ROCK) { spawnFish(x, y); break; }
    }
  updateBoats(dt);
}

// ---------------------------------------------------------------- Zeichnen (allgemein)
static uint16_t skyRow[L];

static void drawSun() {
  float p = sunP();
  if (p < -0.05f || p > 1.05f) return;
  float x = sunX(p), y = HOR + 6 - sinf(clampf(p, 0, 1) * 3.1416f) * (HOR - 18);
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
  float p = moonP();
  if (p < 0 || p > 1) return;
  float x = 50 + p * 130, y = fminf(66.0f, HOR - 26.0f) - sinf(p * 3.1416f) * fminf(40.0f, HOR - 34.0f);
  for (int j = -6; j <= 6; j++) for (int i = -6; i <= 6; i++) {
    float d = sqrtf(i * i + j * j);
    if (d > 5.5f) continue;
    uint16_t c = C(0xf2eee0);
    if ((i == -2 && j == -1) || (i == 1 && j == 2) || (i == 2 && j == -2)) c = C(0xcfcabb);
    int X = (int)x + i, Y = (int)y + j;
    if ((unsigned)X < (unsigned)L && (unsigned)Y < (unsigned)L) lpSky(X, Y, mix(lf[Y * L + X], c, night));
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
    if (b.state == 2) lp(X, Y, boatCol(ch)); else lpBehind(X, Y, boatCol(ch));
  }
  if (b.state != 2 && !b.anchored && fabsf(b.vx) > 0.5f)
    for (int i = 1; i < 6; i++) {
      int X = (int)b.x - (b.vx > 0 ? 1 : -1) * (w / 2 + i * 2), Y = (int)(b.y + bob) + 2 + (i & 1);
      if ((((int)(t_ * 4)) + i) % 3) lpOn(X, Y, C(0xe8f8f8), M_SEA);
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
  uint16_t c = (hasSea && light.day > 0.5f) ? C(0xffffff) : C(0x2a2a3a);
  if (light.golden > 0.4f) c = C(0x3a2a30);
  for (auto &g : gulls) if (g.on) {
    int x = (int)g.x, y = (int)g.y; bool up = ((int)(g.t * 5)) & 1;
    lp(x, y, c);
    if (up) { lp(x - 1, y - 1, c); lp(x - 2, y - 1, c); lp(x + 1, y - 1, c); lp(x + 2, y - 1, c); }
    else { lp(x - 1, y, c); lp(x - 2, y + 1, c); lp(x + 1, y, c); lp(x + 2, y + 1, c); }
  }
}
// Wellen: Schaumkante am Strand laeuft vor und zurueck, an Felsen glitzert Gischt
static void drawShore() {
  for (int x = 0; x < L; x++) {
    if (shoreY[x] < 0) continue;
    float by = shoreY[x];
    float w = 1.5f + 1.5f * sinf(t_ * 0.9f + x * 0.045f) + 0.6f * sinf(t_ * 2.1f + x * 0.21f);
    int fy = (int)(by + w - 1.5f);
    for (int y = (int)by - 2; y <= fy; y++) {
      if ((unsigned)y >= (unsigned)L) continue;
      uint8_t m = mat[y * L + x];
      if (m != M_SEA && m != M_WET && m != M_SAND) continue;
      lf[y * L + x] = (y == fy) ? C(0xf4fbf8) : mix(lf[y * L + x], C(0x9fe6dc), 0.55f);
    }
  }
  int tick = (int)(t_ * 3);
  for (int k = 0; k < nFoam; k++) {
    int i = foamPts[k];
    if ((hash2(i, tick, 31) & 3) == 0) lf[i] = mix(lf[i], C(0xf4fbf8), 0.75f);
  }
}
static void drawGlitter() {
  if (!hasSea) return;
  float p = sunP();
  int tick4 = (int)(t_ * 5);
  if (p > -0.02f && p < 1.02f) {
    float sx = sunX(p);
    float low = clampf(1 - sinf(clampf(p, 0, 1) * 3.1416f), 0, 1);
    uint16_t gc = mixRGB(0xfff6d8, 0xffa040, low);
    for (int y = HOR + 1; y < L; y++) {
      float spread = 3 + (y - HOR) * (0.12f + low * 0.18f);
      for (int i = (int)-spread; i <= (int)spread; i++) {
        int X = (int)sx + i;
        if ((unsigned)X >= (unsigned)L || mat[y * L + X] != M_SEA) continue;
        uint32_t hh = hash2(X, y, tick4);
        if ((hh & 7) == 0 || (fabsf((float)i) < spread * 0.3f && (hh & 3) == 0)) lf[y * L + X] = mix(lf[y * L + X], gc, 0.75f);
      }
    }
  }
  float night = 1 - light.day, mp = moonP();
  if (night > 0.3f && mp >= 0 && mp <= 1) {
    float mx = 50 + mp * 130;
    for (int y = HOR + 1; y < L; y++) for (int i = -3; i <= 3; i++) {
      int X = (int)mx + i;
      if ((unsigned)X >= (unsigned)L || mat[y * L + X] != M_SEA) continue;
      if ((hash2(X, y, tick4) & 7) == 0) lf[y * L + X] = mix(lf[y * L + X], C(0xdfe6f0), 0.6f * night);
    }
  }
  if (light.day > 0.3f)
    for (int k = 0; k < 40; k++) {
      int X = hash2(k, tick4, 5) % L, Y = HOR + 2 + hash2(tick4, k, 6) % (L - HOR - 2);
      if (mat[Y * L + X] == M_SEA) lf[Y * L + X] = mix(lf[Y * L + X], C(0xffffff), 0.5f * light.day);
    }
}
static void drawNightLights() {
  float k = nightK();
  if (k <= 0) return;
  bool on = lightsOn();
  if (windowsOn && on)
    for (int i = 0; i < L * L; i++) if (mat[i] == M_WINDOW) {
      uint16_t w = (hash2(i / 2, i / (L * 4), (int)(t_ * 0.2f)) & 15) == 0 ? C(0xffc860) : C(0xffd77a);
      lf[i] = mix(lf[i], w, k);
    } else if (mat[i] == M_GLOWWALL) {
      lf[i] = mix(lf[i], mix(base[i], C(0xffc070), 0.35f), k);   // angestrahlte Fassade
    }
  if (on)
    for (int i = 0; i < nLights; i++) {
      const LightPt &p = lights[i];
      float b = p.blink ? 0.75f + 0.25f * sinf(t_ * 2 + i) : 1.0f;
      lp(p.x, p.y, mix(lf[p.y * L + p.x], C(p.col), k * b));
      if (!p.blink)   // Lichthof
        for (int d = 0; d < 4; d++) { static const int8_t o[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}; int X = p.x + o[d][0], Y = p.y + o[d][1]; if ((unsigned)X < (unsigned)L && (unsigned)Y < (unsigned)L) lf[Y * L + X] = mix(lf[Y * L + X], C(p.col), 0.35f * k); }
    }
  for (int b = 0; b < nBoats; b++) {
    int w, h; boatSize(boats[b].kind, w, h);
    if (boats[b].kind == 3) continue;
    int X = (int)boats[b].x, Y = (int)boats[b].y - h + 2;
    if ((unsigned)X < (unsigned)L && (unsigned)Y < (unsigned)L && (mat[Y * L + X] == M_SKY || mat[Y * L + X] == M_SEA || boats[b].state == 2))
      lf[Y * L + X] = mix(lf[Y * L + X], C(0xfff2c0), k);
  }
  for (int i = 0; i < nFlies; i++) {
    Fly &f = flies[i];
    float b = 0.5f + 0.5f * sinf(t_ * 1.3f + f.ph * 3);
    if (b < 0.4f) continue;
    int x = (int)(f.x + sinf(t_ * 0.4f + f.ph) * 5), y = (int)(f.y + cosf(t_ * 0.33f + f.ph * 2) * 3);
    if ((unsigned)x < (unsigned)L && (unsigned)y < (unsigned)L) lf[y * L + x] = mix(lf[y * L + x], C(0xd8ff7a), k * b);
  }
}

// ---------------------------------------------------------------- Szene laden
static void finishScene() {
  // Strandkante und Gischt-Punkte suchen
  nFoam = 0;
  for (int x = 0; x < L; x++) {
    shoreY[x] = -1;
    for (int y = HOR; y < L - 1; y++) {
      uint8_t a = mat[y * L + x], b = mat[(y + 1) * L + x];
      if (a == M_SEA && (b == M_SAND || b == M_WET)) { shoreY[x] = y + 1; break; }
    }
  }
  hasSea = false;
  for (int y = HOR; y < L; y++) for (int x = 1; x < L - 1; x++) {
    int i = y * L + x;
    if (mat[i] != M_SEA) continue;
    hasSea = true;
    if (nFoam < 700 && (mat[i - 1] == M_ROCK || mat[i + 1] == M_ROCK || (y + 1 < L && mat[i + L] == M_ROCK)) && (hash2(x, y, 41) & 1)) foamPts[nFoam++] = i;
  }
}
static void loadScene(int s) {
  scene = s;
  HOR = SCENES[s].hor;
  memset(mat, M_SKY, L * L);
  memset(base, 0, L * L * 2);
  nLights = 0; nFlies = 0; nBoats = 0; dockX = dockY = -1;
  SCENES[s].build();
  finishScene();
  initWorld();
}

// ---------------------------------------------------------------- Uhr stellen
static bool clockUi = false;
static int setH = 12, setM = 0;
static uint32_t clockUiIdleMs = 0;
static bool clockChanged = false;

static void textC(const char *s, int cy, int sc, uint16_t c) {
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
  hours = setH + setM / 60.0f; clockSetHours = hours; clockSetMs = 0;
}

// ---------------------------------------------------------------- Eingabe
static bool wasDown = false;
static uint32_t downMs = 0;
static int downX = 0, downY = 0, lastX = 0, lastY = 0;
static bool longFired = false;
static uint32_t lastMs = 0;
static uint32_t nextRepeat = 0;
static int heldZone = 0;

static void startFade(int to) {
  memcpy(oldf, lf, L * L * 2);
  loadScene(to);
  fadeT = FADE;
}
static void handleTap(int sx, int sy) {
  int x = sx / 2, y = sy / 2;
  if ((unsigned)x >= (unsigned)L || (unsigned)y >= (unsigned)L) return;
  for (int k = 0; k < nBoats; k++) {
    Boat &b = boats[k]; int w, h; boatSize(b.kind, w, h);
    if (fabsf(x - b.x) < w / 2 + 6 && y > b.y - h - 4 && y < b.y + 6) {
      if (dockX >= 0 && b.state == 0 && !b.anchored && b.kind != 1 && b.kind != 3) {
        bool free = true; for (int o = 0; o < nBoats; o++) if (o != k && (boats[o].state == 1 || boats[o].state == 2)) free = false;
        if (free) { b.state = 1; return; }
      }
      if (b.state == 2) { b.state = 3; b.t = 0; b.vx = 4.0f; return; }
      spawnFish(b.x + 8, b.y + 3);
      return;
    }
  }
  // Fenster in der Naehe? -> Licht an/aus
  for (int j = -4; j <= 4; j++) for (int i = -4; i <= 4; i++) if (matAt(x + i, y + j) == M_WINDOW) { windowsOn = !windowsOn; return; }
  uint8_t m = mat[y * L + x];
  if (m == M_SEA || m == M_WET) { spawnFish(x, y); return; }
  if (m == M_SKY) { spawnGull(x, y); return; }
}

static void input(uint32_t now, const Touch *pts, int n) {
  bool down = n > 0;
  int x = down ? pts[0].x : lastX, y = down ? pts[0].y : lastY;
  bool pressStartedInMenu = false;
  if (down && !wasDown) {
    downMs = now; downX = x; downY = y; longFired = false;
    if (clockUi) {
      pressStartedInMenu = true;
      heldZone = clockZone(x, y);
      if (heldZone) { clockStep(heldZone); nextRepeat = now + 450; }
    }
  }
  if (down) { lastX = x; lastY = y; }
  if (clockUi && down && heldZone && !pressStartedInMenu && now >= nextRepeat) {
    clockStep(heldZone);
    uint32_t held = now - downMs;
    nextRepeat = now + (held > 2500 ? 40 : (held > 1200 ? 90 : 160));
  }
  if (down && !longFired && !clockUi && now - downMs > 1500) {
    int dx = x - downX, dy = y - downY;
    if (dx * dx + dy * dy < 30 * 30) {
      longFired = true; clockUi = true; clockUiIdleMs = 0; heldZone = 0;
      setH = (int)hours % 24; setM = (int)(hours * 60) % 60;
    }
  }
  if (!down && wasDown && !longFired) {
    int dx = lastX - downX, dy = lastY - downY;
    if (clockUi) {
      if (!heldZone && downY > 350 && downX > 180 && downX < 286) clockOk();
      heldZone = 0;
    } else if (abs(dx) > 90 && abs(dx) > 2 * abs(dy) && now - downMs < 1000) {
      sceneOffset += dx < 0 ? 1 : -1;            // nach links wischen = naechste Szene
      int want = ((((int)(hours * 6)) + sceneOffset) % NSCENES + NSCENES) % NSCENES;
      if (want != scene) startFade(want);
    } else if (now - downMs < 600 && dx * dx + dy * dy < 30 * 30) handleTap(downX, downY);
  }
  wasDown = down;
}

// ---------------------------------------------------------------- Takt
static uint8_t bright = 180;

void begin(uint16_t *fbuf, uint16_t *work, uint32_t seed) {
  fb = fbuf; rng = seed ? seed : 1;
  uint8_t *w = (uint8_t *)work;
  base = (uint16_t *)w; w += L * L * 2;
  lf = (uint16_t *)w; w += L * L * 2;
  oldf = (uint16_t *)w; w += L * L * 2;
  mat = w;
  lastMs = 0; scene = -1; sceneOffset = 0; fadeT = 0;
}
void setClock(int h, int m, int s, int mon) {
  if (testHours >= 0) return;
  clockSetHours = h + m / 60.0f + s / 3600.0f; clockSetMs = lastMs;
  if (mon >= 1 && mon <= 12) month = mon;
}
void testSetHours(float h, int mon) { testHours = h; if (h >= 0) hours = h; if (mon >= 1 && mon <= 12) month = mon; }
static int forcedScene = -1;
void testSetScene(int s) { forcedScene = s; }
int sceneCount() { return NSCENES; }
const char *sceneName(int s) { return (s >= 0 && s < NSCENES) ? SCENES[s].name : ""; }
bool takeClockChange(int &h, int &m) { if (!clockChanged) return false; clockChanged = false; h = setH; m = setM; return true; }
uint8_t wantBrightness() { return bright; }

void tick(uint32_t now, const Touch *pts, int n) {
  if (!lastMs) { lastMs = now; clockSetMs = now; }
  float dt = (now - lastMs) / 1000.0f; lastMs = now;
  if (dt > 0.1f) dt = 0.1f;
  t_ += dt;
  if (testHours >= 0) hours = testHours;
  else { hours = clockSetHours + (now - clockSetMs) / 3600000.0f; while (hours >= 24) hours -= 24; }

  // welche Szene? alle 10 Minuten die naechste (plus Wischen)
  int want = forcedScene >= 0 ? forcedScene % NSCENES : ((((int)(hours * 6)) + sceneOffset) % NSCENES + NSCENES) % NSCENES;
  if (scene < 0) loadScene(want);
  else if (want != scene && fadeT <= 0) startFade(want);

  input(now, pts, n);
  if (clockUi) { clockUiIdleMs += (uint32_t)(dt * 1000); if (clockUiIdleMs > 20000) clockUi = false; }

  computeLight();
  updateLife(dt);

  for (int y = 0; y < HOR && y < L; y++) {
    float v = (float)y / HOR;
    uint32_t c = v < 0.55f ? lerpRGB(light.skyTop, light.skyMid, v / 0.55f) : lerpRGB(light.skyMid, light.skyHor, (v - 0.55f) / 0.45f);
    skyRow[y] = C(c);
  }
  for (int y = HOR; y < L; y++) skyRow[y] = C(light.skyHor);
  // 1) feste Szene + Himmel
  for (int y = 0; y < L; y++) {
    const uint16_t *b = base + y * L; const uint8_t *m = mat + y * L; uint16_t *o = lf + y * L;
    for (int x = 0; x < L; x++) o[x] = m[x] == M_SKY ? skyRow[y] : b[x];
  }
  // 2) Himmel-Objekte (hinter allem)
  drawMoonAndStars();
  drawSun();
  drawClouds();
  // 3) Wasser, Boote, Szenen-Leben
  drawShore();
  for (int k = 0; k < nBoats; k++) if (boats[k].state != 2) drawBoat(boats[k], k);
  drawSeaLife();
  for (int k = 0; k < nBoats; k++) if (boats[k].state == 2) drawBoat(boats[k], k);
  if (SCENES[scene].animate) SCENES[scene].animate(dt);
  // 4) Licht der Tageszeit auf alles ausser Himmel
  for (int i = 0; i < L * L; i++) if (mat[i] != M_SKY) lf[i] = lit(lf[i]);
  // 5) Selbstleuchtendes
  drawGlitter();
  drawGulls();
  drawNightLights();
  if (SCENES[scene].glow) SCENES[scene].glow(nightK());
  // Ueberblendung von der vorigen Szene
  if (fadeT > 0) {
    fadeT -= dt;
    float p = clampf(1 - fadeT / FADE, 0, 1); p = p * p * (3 - 2 * p);
    for (int i = 0; i < L * L; i++) lf[i] = mix(oldf[i], lf[i], p);
  }
  // 6) doppelt skaliert ins Bild
  for (int y = 0; y < SCR; y += 2) {
    const uint16_t *src = lf + (y >> 1) * L; uint16_t *d = fb + y * SCR;
    for (int x = 0; x < L; x++) { d[2 * x] = d[2 * x + 1] = src[x]; }
    if (y + 1 < SCR) memcpy(d + SCR, d, SCR * 2);
  }
  if (clockUi) drawClockUi();
  else if (t_ < 8) {
    float k = t_ < 6 ? 1 : (8 - t_) / 2;
    textC("WISCHEN = NÄCHSTES BILD", 288, 2, mix(C(0x404040), C(0xfff8ee), k));
    textC("LANGE DRÜCKEN = UHR", 312, 2, mix(C(0x404040), C(0xfff8ee), k));
  }
  bright = (uint8_t)(60 + 100 * light.day);
}

Debug debug() {
  Debug d; memset(&d, 0, sizeof d);
  d.hours = hours; d.daylight = light.day; d.clockUi = clockUi; d.lightsOn = lightsOn() && windowsOn;
  d.scene = scene; d.fading = fadeT > 0; d.nBoats = nBoats;
  for (int k = 0; k < nBoats && k < 5; k++) { d.boatState[k] = boats[k].state; d.boatX[k] = boats[k].x; d.boatY[k] = boats[k].y; if (boats[k].state == 2) d.boatsDocked++; }
  return d;
}

}  // namespace kw
