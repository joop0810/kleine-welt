#include "gfx.h"
#include <math.h>
#include <string.h>

namespace kb {

static uint16_t *fb = nullptr;
static int ox = 0, oy = 0;
static int galpha = 256;

uint16_t rgb565(int r, int g, int b) {
  r = r < 0 ? 0 : (r > 255 ? 255 : r); g = g < 0 ? 0 : (g > 255 ? 255 : g); b = b < 0 ? 0 : (b > 255 ? 255 : b);
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
uint16_t mix565(uint16_t a, uint16_t b, float t) {
  int r1 = (a >> 11) << 3, g1 = ((a >> 5) & 63) << 2, b1 = (a & 31) << 3;
  int r2 = (b >> 11) << 3, g2 = ((b >> 5) & 63) << 2, b2 = (b & 31) << 3;
  return rgb565(r1 + (int)((r2 - r1) * t), g1 + (int)((g2 - g1) * t), b1 + (int)((b2 - b1) * t));
}

void gBegin(uint16_t *f) { fb = f; }
void gOffset(int dx, int dy) { ox = dx; oy = dy; }
void gAlpha(float a) { galpha = a <= 0 ? 0 : (a >= 1 ? 256 : (int)(a * 256)); }

void gClear(uint16_t c) {
  if (c == 0) { memset(fb, 0, SCR * SCR * 2); return; }
  for (int i = 0; i < SCR * SCR; i++) fb[i] = c;
}

// a = Deckkraft 0..256
static inline void blend(int x, int y, uint16_t c, int a) {
  a = (a * galpha) >> 8;
  if (a <= 0) return;
  uint16_t *p = fb + y * SCR + x;
  if (a >= 255) { *p = c; return; }
  uint16_t d = *p;
  // RGB565 Mischung mit 5 Bit Genauigkeit fuer alle Kanaele (schnell, reicht fuer Kanten)
  uint32_t fg = (c | ((uint32_t)c << 16)) & 0x07E0F81F;
  uint32_t bg = (d | ((uint32_t)d << 16)) & 0x07E0F81F;
  uint32_t k = (uint32_t)a >> 3;
  uint32_t r = ((((fg - bg) * k) >> 5) + bg) & 0x07E0F81F;
  *p = (uint16_t)(r | (r >> 16));
}

// Allgemeiner Maler: dist(px,py) liefert vorzeichenbehafteten Abstand (<0 innen)
template <typename F>
static void paint(float x0, float y0, float x1, float y1, uint16_t c, F dist) {
  int ix0 = (int)floorf(x0 - 1) + ox, iy0 = (int)floorf(y0 - 1) + oy;
  int ix1 = (int)ceilf(x1 + 1) + ox, iy1 = (int)ceilf(y1 + 1) + oy;
  if (ix0 < 0) ix0 = 0;
  if (iy0 < 0) iy0 = 0;
  if (ix1 > SCR - 1) ix1 = SCR - 1;
  if (iy1 > SCR - 1) iy1 = SCR - 1;
  for (int y = iy0; y <= iy1; y++) {
    float py = y - oy + 0.5f;
    for (int x = ix0; x <= ix1; x++) {
      float d = dist(x - ox + 0.5f, py);
      if (d >= 0.5f) continue;
      int a = d <= -0.5f ? 256 : (int)((0.5f - d) * 256);
      blend(x, y, c, a);
    }
  }
}

void gCircle(float cx, float cy, float r, uint16_t c) {
  if (r <= 0) return;
  float rin = r - 0.75f, rin2 = rin > 0 ? rin * rin : 0;
  paint(cx - r, cy - r, cx + r, cy + r, c, [&](float x, float y) {
    float dx = x - cx, dy = y - cy, q = dx * dx + dy * dy;
    if (q < rin2) return -1.0f;
    return sqrtf(q) - r;
  });
}

// Ring-Band zeilenweise: nur die Pixel im Band werden angefasst (kein Rechteck-Scan)
template <typename F>
static void band(float cx, float cy, float r, float h, uint16_t c, F dist) {
  float ro = r + h + 1, ri = r - h - 1;
  int y0 = (int)floorf(cy - ro) + oy, y1 = (int)ceilf(cy + ro) + oy;
  if (y0 < 0) y0 = 0;
  if (y1 > SCR - 1) y1 = SCR - 1;
  for (int y = y0; y <= y1; y++) {
    float py = y - oy + 0.5f, dy = py - cy;
    float qo = ro * ro - dy * dy;
    if (qo <= 0) continue;
    float wo = sqrtf(qo);
    float qi = ri > 0 ? ri * ri - dy * dy : -1;
    float wi = qi > 0 ? sqrtf(qi) : -1;
    // zwei Spannen links und rechts (oder eine, wenn die Zeile das Loch nicht trifft)
    for (int side = 0; side < 2; side++) {
      float a, b;
      if (wi < 0) { if (side) break; a = cx - wo; b = cx + wo; }
      else if (!side) { a = cx - wo; b = cx - wi; }
      else { a = cx + wi; b = cx + wo; }
      int xa = (int)floorf(a) + ox, xb = (int)ceilf(b) + ox;
      if (xa < 0) xa = 0;
      if (xb > SCR - 1) xb = SCR - 1;
      for (int x = xa; x <= xb; x++) {
        float d = dist(x - ox + 0.5f, py);
        if (d >= 0.5f) continue;
        int al = d <= -0.5f ? 256 : (int)((0.5f - d) * 256);
        blend(x, y, c, al);
      }
    }
  }
}

void gRing(float cx, float cy, float r, float thick, uint16_t c) {
  float h = thick * 0.5f;
  band(cx, cy, r, h, c, [&](float x, float y) {
    float dx = x - cx, dy = y - cy;
    return fabsf(sqrtf(dx * dx + dy * dy) - r) - h;
  });
}

void gArc(float cx, float cy, float r, float thick, float a0, float a1, uint16_t c) {
  if (a1 < a0) { float t = a0; a0 = a1; a1 = t; }
  if (a1 - a0 >= 360) { gRing(cx, cy, r, thick, c); return; }
  float h = thick * 0.5f;
  const float D = 3.14159265f / 180;
  float u0x = cosf(a0 * D), u0y = sinf(a0 * D), u1x = cosf(a1 * D), u1y = sinf(a1 * D);
  float e0x = cx + r * u0x, e0y = cy + r * u0y, e1x = cx + r * u1x, e1y = cy + r * u1y;
  bool big = a1 - a0 > 180;
  // Ist die Richtung (dx,dy) im Bogen? Nur Kreuzprodukte, kein atan2 (zu langsam auf dem ESP32)
  auto inside = [&](float dx, float dy) {
    float c0 = u0x * dy - u0y * dx;   // >= 0: im Uhrzeigersinn hinter a0
    float c1 = dx * u1y - dy * u1x;   // >= 0: vor a1
    return big ? (c0 >= 0 || c1 >= 0) : (c0 >= 0 && c1 >= 0);
  };
  band(cx, cy, r, h, c, [&](float x, float y) {
    float dx = x - cx, dy = y - cy;
    if (inside(dx, dy)) return fabsf(sqrtf(dx * dx + dy * dy) - r) - h;
    float d0 = (x - e0x) * (x - e0x) + (y - e0y) * (y - e0y);
    float d1 = (x - e1x) * (x - e1x) + (y - e1y) * (y - e1y);
    return sqrtf(d0 < d1 ? d0 : d1) - h;
  });
}

void gCapsule(float x0, float y0, float x1, float y1, float r, uint16_t c) {
  float vx = x1 - x0, vy = y1 - y0, L2 = vx * vx + vy * vy;
  float bx0 = (x0 < x1 ? x0 : x1) - r, bx1 = (x0 > x1 ? x0 : x1) + r;
  float by0 = (y0 < y1 ? y0 : y1) - r, by1 = (y0 > y1 ? y0 : y1) + r;
  paint(bx0, by0, bx1, by1, c, [&](float x, float y) {
    float px = x - x0, py = y - y0;
    float t = L2 > 0 ? (px * vx + py * vy) / L2 : 0;
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    float dx = px - vx * t, dy = py - vy * t;
    return sqrtf(dx * dx + dy * dy) - r;
  });
}

void gRRect(float x, float y, float w, float h, float rad, uint16_t c) {
  float cx = x + w * 0.5f, cy = y + h * 0.5f, hx = w * 0.5f - rad, hy = h * 0.5f - rad;
  paint(x, y, x + w, y + h, c, [&](float px, float py) {
    float qx = fabsf(px - cx) - hx, qy = fabsf(py - cy) - hy;
    float mx = qx > 0 ? qx : 0, my = qy > 0 ? qy : 0;
    float inside = qx > qy ? qx : qy;
    return sqrtf(mx * mx + my * my) + (inside < 0 ? inside : 0) - rad;
  });
}

void gEllipse(float cx, float cy, float rx, float ry, uint16_t c) {
  float m = rx < ry ? rx : ry;
  paint(cx - rx, cy - ry, cx + rx, cy + ry, c, [&](float x, float y) {
    float nx = (x - cx) / rx, ny = (y - cy) / ry;
    float k = sqrtf(nx * nx + ny * ny);
    return (k - 1) * m;   // Naeherung, fuer runde Formen gut genug
  });
}

void gDrop(float cx, float cy, float s, uint16_t c) {
  // Bauch: Kreis R um (cx, cy+20s), Spitze bei cy-95s, Flanken als Tangenten
  float R = 70 * s, by = cy + 20 * s, ty = cy - 95 * s;
  float d = by - ty;                       // Abstand Spitze -> Kreismitte
  float sa = R / d;                        // sin des halben Spitzenwinkels
  float ca = sqrtf(1 - sa * sa);
  paint(cx - R, ty, cx + R, by + R, c, [&](float x, float y) {
    float px = fabsf(x - cx), py = y - ty;
    // Projektion auf die Flanke (Richtung (sa, ca) von der Spitze aus)
    float along = px * sa + py * ca;
    float tlen = d * ca;                   // Laenge bis zum Beruehrpunkt
    // Form = konvexe Huelle aus Spitze und Kreis: Flanke, Spitze oder Kreisrand
    if (along >= 0 && along <= tlen) return px * ca - py * sa;   // >0 aussen
    if (along < 0) return sqrtf(px * px + py * py);
    return sqrtf(px * px + (y - by) * (y - by)) - R;
  });
}

void gTriangle(float x0, float y0, float x1, float y1, float x2, float y2, uint16_t c) {
  float bx0 = fminf(x0, fminf(x1, x2)), bx1 = fmaxf(x0, fmaxf(x1, x2));
  float by0 = fminf(y0, fminf(y1, y2)), by1 = fmaxf(y0, fmaxf(y1, y2));
  float area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
  float sg = area < 0 ? -1.0f : 1.0f;
  auto edge = [&](float ax, float ay, float bx, float by, float px, float py) {
    float ex = bx - ax, ey = by - ay, L = sqrtf(ex * ex + ey * ey);
    return sg * (ex * (py - ay) - ey * (px - ax)) / L;   // >0 innen
  };
  paint(bx0, by0, bx1, by1, c, [&](float x, float y) {
    float e = edge(x0, y0, x1, y1, x, y);
    float f = edge(x1, y1, x2, y2, x, y);
    float g = edge(x2, y2, x0, y0, x, y);
    float m = e < f ? e : f;
    m = m < g ? m : g;
    return -m;
  });
}

// ------------------------------------------------------------------ Schrift
static uint32_t nextCp(const char *&s) {
  uint8_t c = (uint8_t)*s++;
  if (c < 0x80) return c;
  if ((c & 0xE0) == 0xC0 && *s) return ((c & 0x1F) << 6) | ((uint8_t)*s++ & 0x3F);
  if ((c & 0xF0) == 0xE0 && s[0] && s[1]) {
    uint32_t v = ((c & 0x0F) << 12) | (((uint8_t)s[0] & 0x3F) << 6) | ((uint8_t)s[1] & 0x3F);
    s += 2; return v;
  }
  while ((*s & 0xC0) == 0x80) s++;
  return '?';
}
static const Glyph *find(const Font &f, uint32_t cp) {
  int lo = 0, hi = f.n - 1;
  while (lo <= hi) {
    int m = (lo + hi) / 2;
    if (f.glyphs[m].cp == cp) return &f.glyphs[m];
    if (f.glyphs[m].cp < cp) lo = m + 1; else hi = m - 1;
  }
  if (cp != '?') return find(f, '?');
  return nullptr;
}

int gTextW(const Font &f, const char *s) {
  int w = 0;
  while (*s) { const Glyph *g = find(f, nextCp(s)); if (g) w += g->adv; }
  return w;
}

int gText(const Font &f, int x, int y, const char *s, uint16_t c, Align al) {
  int w = gTextW(f, s);
  if (al == CENTER) x -= w / 2;
  else if (al == RIGHT) x -= w;
  while (*s) {
    const Glyph *g = find(f, nextCp(s));
    if (!g) continue;
    const uint8_t *m = f.bmp + g->off;
    int gx = x + g->xo + ox, gy = y + g->yo + oy;
    for (int j = 0; j < g->h; j++) {
      int yy = gy + j;
      if (yy < 0 || yy >= SCR) continue;
      for (int i = 0; i < g->w; i++) {
        int xx = gx + i;
        if (xx < 0 || xx >= SCR) continue;
        uint8_t a = m[j * g->w + i];
        if (a) blend(xx, yy, c, a == 255 ? 256 : a);
      }
    }
    x += g->adv;
  }
  return w;
}

void gFit(const Font &f, const char *in, char *out, int outLen, int maxW) {
  strncpy(out, in, outLen - 1);
  out[outLen - 1] = 0;
  if (gTextW(f, out) <= maxW) return;
  int n = strlen(out);
  while (n > 0) {
    n--;
    while (n > 0 && ((uint8_t)out[n] & 0xC0) == 0x80) n--;   // nicht mitten im UTF-8-Zeichen
    if (n + 4 > outLen) continue;
    memcpy(out + n, "\xE2\x80\xA6", 4);
    if (gTextW(f, out) <= maxW) return;
  }
}

}  // namespace kb
