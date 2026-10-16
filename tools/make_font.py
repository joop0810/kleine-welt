#!/usr/bin/env python3
"""Erzeugt firmware-src/Kumpel/font_data.h aus der Schrift Poppins (SIL OFL 1.1).

Aufruf (im Repo-Hauptordner):  python3 tools/make_font.py [Pfad/zu/Poppins]

Jede Groesse wird als 8-Bit-Alphamaske pro Zeichen gespeichert (geglaettete Kanten).
Die erzeugte Datei ist eingecheckt - die Cloud-Bauumgebung braucht kein Python/PIL.
"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
FONTDIR = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/usr/share/fonts/truetype/google-fonts")

TEXT = "".join(chr(c) for c in range(32, 127)) + "äöüÄÖÜß°€·–…"
DIGITS = "0123456789.,-–°%:+ "

FONTS = [  # Name im Code, Datei, Pixelgroesse, Zeichensatz
    ("F_SMALL", "Poppins-Medium.ttf", 21, TEXT),
    ("F_MED", "Poppins-Bold.ttf", 30, TEXT),
    ("F_BIG", "Poppins-Medium.ttf", 96, DIGITS),
]


def build(name, file, size, chars):
    font = ImageFont.truetype(str(FONTDIR / file), size)
    ascent, descent = font.getmetrics()
    glyphs, data = [], bytearray()
    for ch in sorted(set(chars), key=ord):
        adv = round(font.getlength(ch))
        box = font.getbbox(ch)            # relativ zum Ursprung (Oberkante = ascent-Linie)
        w, h = max(0, box[2] - box[0]), max(0, box[3] - box[1])
        if w and h:
            img = Image.new("L", (w, h), 0)
            ImageDraw.Draw(img).text((-box[0], -box[1]), ch, font=font, fill=255)
            px = img.tobytes()
        else:
            px = b""
        glyphs.append((ord(ch), len(data), w, h, box[0], box[1], adv))
        data += px
    lines = [f"// {name}: {file} {size}px, {len(glyphs)} Zeichen, {len(data)} Byte"]
    lines.append(f"static const uint8_t {name}_BMP[] = {{")
    for i in range(0, len(data), 32):
        lines.append("  " + ",".join(str(b) for b in data[i:i + 32]) + ",")
    lines.append("};")
    lines.append(f"static const Glyph {name}_GLYPHS[] = {{")
    for g in glyphs:
        lines.append("  {%d,%d,%d,%d,%d,%d,%d}," % g)
    lines.append("};")
    lines.append(f"const Font {name} = {{{name}_GLYPHS, {len(glyphs)}, {name}_BMP, {ascent + descent}, {ascent}}};")
    return "\n".join(lines)


def main():
    out = ["// Automatisch erzeugt von tools/make_font.py - nicht von Hand aendern.",
           "// Schrift: Poppins (c) Indian Type Foundry, SIL Open Font License 1.1",
           '#include "font.h"', "namespace kb {"]
    for f in FONTS:
        out.append(build(*f))
    out.append("}  // namespace kb\n")
    dst = ROOT / "firmware-src/Kumpel/font_data.cpp"
    dst.write_text("\n".join(out), encoding="utf-8")
    print(f"{dst.relative_to(ROOT)}: {dst.stat().st_size // 1024} KB")


if __name__ == "__main__":
    main()
