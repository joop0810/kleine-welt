#pragma once
#include <stdint.h>

namespace kb {

// Ein Zeichen: Alphamaske w*h ab bmp[off], Versatz (xo, yo) zum Ursprung
// (Ursprung = linke Kante, Oberkante der Zeile), Vorschub adv.
struct Glyph {
  uint32_t cp, off;
  uint8_t w, h;
  int8_t xo, yo;
  uint8_t adv;
};

struct Font {
  const Glyph *glyphs;
  uint16_t n;
  const uint8_t *bmp;
  uint8_t lineH, ascent;
};

extern const Font F_SMALL;  // 21 px, Beschriftung
extern const Font F_MED;    // 30 px fett, Titel und Werte
extern const Font F_BIG;    // 96 px, nur Ziffern . , - ° % : +

}  // namespace kb
