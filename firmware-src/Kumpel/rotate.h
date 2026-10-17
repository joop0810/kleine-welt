#pragma once
// Bild drehen, je nachdem wie das Geraet steht (z. B. im Figuren-Staender mit USB-C nach unten).
// Der Kern zeichnet immer aufrecht in den Framebuffer; hier wird das fertige Bild um
// 90/180/270 Grad im Uhrzeigersinn gedreht und die Touch-Punkte passend zurueckgerechnet.
// Ohne Arduino-Abhaengigkeit, damit der PC-Test es pruefen kann.
#include <stdint.h>

namespace rot {

// q = Viertelumdrehungen im Uhrzeigersinn (0..3), n = Kantenlaenge (quadratisch)
// Inhaltspunkt (cx, cy) erscheint auf dem Panel bei:
inline void toPanel(int q, int n, int cx, int cy, int &px, int &py) {
  switch (q & 3) {
    case 1: px = n - 1 - cy; py = cx; break;
    case 2: px = n - 1 - cx; py = n - 1 - cy; break;
    case 3: px = cy; py = n - 1 - cx; break;
    default: px = cx; py = cy;
  }
}
// Panelpunkt (Touch) -> Inhaltspunkt
inline void toContent(int q, int n, int px, int py, int &cx, int &cy) {
  switch (q & 3) {
    case 1: cx = py; cy = n - 1 - px; break;
    case 2: cx = n - 1 - px; cy = n - 1 - py; break;
    case 3: cx = n - 1 - py; cy = px; break;
    default: cx = px; cy = py;
  }
}

// src (Inhalt) -> dst (Panel). In Kacheln, damit PSRAM-Zugriffe im Cache bleiben.
inline void image(int q, int n, const uint16_t *src, uint16_t *dst) {
  constexpr int T = 32;
  for (int by = 0; by < n; by += T)
    for (int bx = 0; bx < n; bx += T) {
      int ey = by + T < n ? by + T : n, ex = bx + T < n ? bx + T : n;
      for (int cy = by; cy < ey; cy++) {
        const uint16_t *row = src + cy * n;
        for (int cx = bx; cx < ex; cx++) {
          int px, py;
          toPanel(q, n, cx, cy, px, py);
          dst[py * n + px] = row[cx];
        }
      }
    }
}

}  // namespace rot
