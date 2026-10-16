#pragma once
// Kleine Zeichenbibliothek fuer den Kumpel: geglaettete Formen und Schrift
// direkt in einen 466x466-RGB565-Framebuffer. Keine Arduino-Abhaengigkeit.
#include <stdint.h>
#include "font.h"

namespace kb {

constexpr int SCR = 466;

constexpr uint16_t C(uint32_t rgb) {
  return (uint16_t)((((rgb >> 16) & 0xF8) << 8) | (((rgb >> 8) & 0xFC) << 3) | ((rgb & 0xFF) >> 3));
}
uint16_t rgb565(int r, int g, int b);
uint16_t mix565(uint16_t a, uint16_t b, float t);   // t=0 -> a, t=1 -> b

enum Align : uint8_t { LEFT, CENTER, RIGHT };

void gBegin(uint16_t *fb);
void gClear(uint16_t c);
void gOffset(int dx, int dy);   // verschiebt alles Folgende (Seitenwechsel)
void gAlpha(float a);           // globale Deckkraft 0..1 fuer alles Folgende

void gCircle(float cx, float cy, float r, uint16_t c);
void gRing(float cx, float cy, float r, float thick, uint16_t c);
// Bogen mit runden Enden. Winkel in Grad, 0 = rechts, im Uhrzeigersinn (y nach unten).
void gArc(float cx, float cy, float r, float thick, float a0, float a1, uint16_t c);
// Strich mit runden Enden (Kapsel) von (x0,y0) nach (x1,y1), Radius r
void gCapsule(float x0, float y0, float x1, float y1, float r, uint16_t c);
void gRRect(float x, float y, float w, float h, float rad, uint16_t c);
void gEllipse(float cx, float cy, float rx, float ry, uint16_t c);
void gDrop(float cx, float cy, float s, uint16_t c);   // Wassertropfen, Bauch-Radius 70*s
void gTriangle(float x0, float y0, float x1, float y1, float x2, float y2, uint16_t c);

int gTextW(const Font &f, const char *utf8);
// y = Oberkante der Zeile. Liefert die Breite.
int gText(const Font &f, int x, int y, const char *utf8, uint16_t c, Align al = LEFT);
// Text auf maxW Pixel kuerzen (mit "…")
void gFit(const Font &f, const char *in, char *out, int outLen, int maxW);

}  // namespace kb
