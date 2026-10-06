// Kleine Welt - die zehn Szenen. Wird in game.cpp eingebunden (gleiche Hilfsfunktionen).
// Jede Szene: build() malt die feste Landschaft in base/mat, animate() bewegte Dinge
// (vor dem Tageslicht), glow() eigene Lichter (nach dem Tageslicht).
// Sichtbar ist nur der Kreis um (116,116) mit Radius 116.

// Sonnenschirm mit Handtuch und Schatten
static void parasol(int x, int y, uint32_t c1, uint32_t c2) {
  for (int j = -2; j <= 2; j++) for (int i = -9; i <= 9; i++)
    if (i * i / 81.0f + j * j / 6.0f <= 1) { int X = x + 3 + i, Y = y + 1 + j; if ((unsigned)X < (unsigned)L && (unsigned)Y < (unsigned)L) base[Y * L + X] = mix(base[Y * L + X], C(0x8a7050), 0.35f); }
  for (int j = -1; j <= 2; j++) for (int i = 3; i <= 13; i++) bp(x + i, y + j, ((i / 2) % 2) ? C(c1) : C(0xf6f0e6));
  for (int k = 0; k < 14; k++) bp(x, y - k, C(0xeae2d0));
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
static void jettyAt(int x0, int x1, int ytop, int ybot) {
  const uint16_t pl = C(0xb68a5a), plD = C(0x7a5536), post = C(0x4a3424);
  for (int y = ytop; y <= ybot; y++) for (int x = x0; x <= x1; x++) bp(x, y, (y % 3 == 0) ? plD : pl, M_JETTY);
  for (int y = ytop + 2; y <= ybot; y += 7) { bp(x0 - 1, y, post, M_JETTY); bp(x1 + 1, y, post, M_JETTY); bp(x0 - 1, y + 1, post, M_JETTY); bp(x1 + 1, y + 1, post, M_JETTY); }
  for (int x = x0; x <= x1; x++) bp(x, ytop - 1, plD, M_JETTY);
}
// Strand/Meer/Himmel-Grundlage: Meer bis zur Kurve shore(x), darunter Sand
template <typename F> static void seaAndBeach(F shore) {
  for (int y = 0; y < L; y++) for (int x = 0; x < L; x++) {
    float by = shore(x);
    if (y < HOR) bp(x, y, 0, M_SKY);
    else if (y < by) bp(x, y, seaCol(x, y, by), M_SEA);
    else if (y < by + 3) bp(x, y, C(0xc9b080), M_WET);
    else bp(x, y, sandCol(x, y), M_SAND);
  }
}
// kleine Insel am Horizont
static void islet(int x0, int x1, int h) {
  int mid = (x0 + x1) / 2; float half = (x1 - x0) / 2.0f;
  for (int x = x0; x <= x1; x++) {
    float u = (x - mid) / half; int hgt = (int)(h * (1 - u * u) + (x > mid ? 2 * sinf(x * 0.6f) : 0));
    for (int k = 0; k <= hgt; k++) if (matAt(x, HOR - k) == M_SKY) bp(x, HOR - k, C(0x7f98b0), M_ROCK);
  }
}

// ================================================================ 1 Die Cala
static float c1topL(int x) { return x < 54 ? 73 + 1.5f * sinf(x * 0.35f) : 73 + (x - 54) * 1.25f; }
static float c1coastL(int y) { return y < 92 ? 70 : 70 + (y - 92) * 0.05f + 2.5f * sinf(y * 0.37f) + 1.5f * sinf(y * 1.1f); }
static float c1topR(int x) { return 97 + (L - x) * 0.22f + 1.5f * sinf(x * 0.5f); }
static float c1coastR(int y) { return 176 - (y - 100) * 0.07f + 2 * sinf(y * 0.45f) + sinf(y * 1.3f); }
static float c1beach(int x) { float u = (x - 116) / 116.0f; return 180 - 26 * u * u; }

static void finca(int ox, int oy) {
  const uint16_t wall = C(0xf3ede0), wallS = C(0xd9cfbd), roof = C(0xc8643c), roofD = C(0x8e3f24), door = C(0x6e4630);
  for (int y = 59; y <= 73; y++) for (int x = 24; x <= 50; x++) bp(x + ox, y + oy, x >= 46 ? wallS : wall);
  for (int y = 53; y <= 59; y++) { int in = 59 - y; for (int x = 22 + in; x <= 52 - in; x++) bp(x + ox, y + oy, (y % 2) ? roof : roofD); }
  for (int y = 48; y <= 59; y++) for (int x = 27; x <= 32; x++) bp(x + ox, y + oy, x >= 31 ? wallS : wall);
  for (int x = 26; x <= 33; x++) { bp(x + ox, 47 + oy, roof); bp(x + ox, 46 + oy, roofD); }
  for (int x = 28; x <= 31; x++) bp(x + ox, 45 + oy, roof);
  int wins[][2] = {{36, 63}, {42, 63}, {29, 51}, {26, 64}};
  for (auto &w : wins) fillRect(w[0] + ox, w[1] + oy, w[0] + 1 + ox, w[1] + 3 + oy, C(0x2b3a5c), M_WINDOW);
  fillRect(31 + ox, 67 + oy, 34 + ox, 73 + oy, door);
  bp(31 + ox, 67 + oy, wall); bp(34 + ox, 67 + oy, wall);
  for (int x = 50; x <= 58; x++) { bp(x + ox, 72 + oy, wallS); bp(x + ox, 73 + oy, wallS); }
  for (int k = 0; k < 14; k++) bp(50 + ox + (int)(hash2(k, 1, 9) % 9), 69 + oy + (int)(hash2(k, 2, 9) % 3), (k & 1) ? C(0xd63a8c) : C(0xb02a6e));
  cypress(20 + ox, 74 + oy, 22);
}

static void buildCala() {
  seaAndBeach([](int x) { return c1beach(x); });
  for (int y = 0; y < L; y++) for (int x = 0; x < L; x++) {
    float by = c1beach(x);
    bool lL = x <= c1coastL(y) && y >= c1topL(x) && y < by + 3;
    bool lR = x >= 170 && x >= c1coastR(y) && y >= c1topR(x) && y < by + 3;
    if (!lL && !lR) continue;
    float top = lL ? c1topL(x) : c1topR(x), edge = lL ? c1coastL(y) - x : x - c1coastR(y);
    uint16_t c = limestone(x, y, edge); Mat m = M_ROCK;
    float depth = y - top, cover = depth < 5 ? 1.0f : (depth < 30 ? 0.62f - depth * 0.006f : 0.42f);
    if (vnoise(x / 5.0f, y / 5.0f, 37) > 1 - cover * 0.75f && edge > 3) { c = scrubCol(x, y); m = M_SCRUB; }
    if (y >= by - 1) { c = C(0x9e8264); m = M_ROCK; }
    bp(x, y, c, m);
  }
  islet(86, 122, 7);
  finca(0, 0);
  bush(62, 84, 3); bush(14, 77, 4); bush(56, 100, 3);
  pine(58, 80, 1.0f); pine(64, 96, 0.8f); pine(198, 106, 0.85f); pine(216, 100, 1.05f);
  bush(186, 113, 3); bush(226, 108, 3);
  jettyAt(136, 145, 152, (int)c1beach(140) + 2);
  parasol(64, 200, 0xd8403c, 0xf6f0e6); parasol(96, 206, 0x2f6fb0, 0xf6f0e6); parasol(170, 202, 0xe0a030, 0xf6f0e6);
  palm(214, 236, 1.0f, -18);
  addBoat(0, 40, 132, 4.5f); addBoat(1, 150, 102, -2.2f); addBoat(2, 190, 146, -3.5f);
  dockX = 154; dockY = 158;
  static const uint32_t cols[] = {0xffd060, 0xff8a6a, 0x9ad8ff, 0xffe8b0};
  for (int y = 154, k = 0; y < 182; y += 4, k++) { addLight(135, y, cols[k & 3], 1); addLight(146, y + 2, cols[(k + 2) & 3], 1); }
  addFlies(12, 74, 66, 104);
}

// ================================================================ 2 Felsbucht (schmal und steil)
static float c2left(int y) { return 74 + (y - 60) * 0.12f + 3 * sinf(y * 0.21f) + 1.5f * sinf(y * 0.9f); }
static float c2right(int y) { return 162 - (y - 60) * 0.10f + 3 * sinf(y * 0.18f + 1) + 1.5f * sinf(y * 0.8f); }
static void buildFelsbucht() {
  for (int y = 0; y < L; y++) for (int x = 0; x < L; x++) {
    float l = c2left(y), r = c2right(y);
    float topL = 46 + x * 0.25f + 3 * sinf(x * 0.2f), topR = 52 + (L - x) * 0.22f + 3 * sinf(x * 0.23f);
    bool land = (x < l && y >= topL) || (x > r && y >= topR);
    float pebbleY = 206 + 3 * sinf(x * 0.1f);
    if (land) {
      float edge = x < l ? l - x : x - r;
      float top = x < l ? topL : topR;
      uint16_t c = limestone(x, y, edge, 23); Mat m = M_ROCK;
      if ((y - top) < 6 || (vnoise(x / 4.0f, y / 6.0f, 51) > 0.78f && edge > 4)) { c = scrubCol(x, y); m = M_SCRUB; }
      // senkrechte Rinnen in der Wand
      if (edge > 4 && (hash2(x / 3, 0, 9) & 7) == 0 && (y & 1)) c = mix(c, 0, 0.15f);
      bp(x, y, c, m);
    } else if (y < HOR) bp(x, y, 0, M_SKY);
    else if (y < pebbleY) {
      // sehr klar: helles Tuerkis, dunkle Steine unter Wasser
      float v = clampf((y - HOR) / (pebbleY - HOR), 0, 1);
      uint32_t col = v < 0.3f ? lerpRGB(0x1f6a96, 0x2a9fc0, v / 0.3f) : lerpRGB(0x2a9fc0, 0x6ee0d0, (v - 0.3f) / 0.7f);
      float rock = vnoise(x / 7.0f, y / 4.0f, 61);
      if (v > 0.25f && rock > 0.68f) col = lerpRGB(col, 0x1d5a6a, 0.45f);
      float edgeD = fminf(x - l, r - x);
      if (edgeD < 6) col = lerpRGB(col, 0x2a6f78, (6 - edgeD) / 10.0f);
      if ((hash2(x, y, 11) & 31) == 0) col = lerpRGB(col, 0xffffff, 0.15f);
      bp(x, y, C(col), M_SEA);
    } else if (y < pebbleY + 2) bp(x, y, C(0xbfae94), M_WET);
    else bp(x, y, C((hash2(x, y, 5) & 3) == 0 ? 0xcdbfa6 : ((hash2(x, y, 6) & 3) == 0 ? 0xa89c88 : 0xe0d4bc)), M_SAND);
  }
  pine(38, 60, 1.1f); pine(62, 66, 0.9f); pine(18, 60, 0.9f);
  pine(176, 60, 1.0f); pine(200, 64, 1.15f); pine(156, 70, 0.7f);
  bush(80, 70, 3); bush(150, 76, 3);
  // Handtuecher auf den Kieseln
  for (int j = 0; j < 3; j++) for (int i = 0; i < 9; i++) { bp(96 + i, 214 + j, (i / 2) & 1 ? C(0x2f6fb0) : C(0xf6f0e6)); bp(128 + i, 216 + j, (i / 2) & 1 ? C(0xe06a5a) : C(0xf6f0e6)); }
  addBoat(0, 118, 98, 0, true);
  addFlies(10, 50, 70, 80);
}
// Schwimmer mit kleinen Wellenringen
static void animFelsbucht(float dt) {
  (void)dt;
  static const float sw[3][3] = {{102, 150, 0}, {130, 172, 2}, {116, 125, 4}};
  for (auto &s : sw) {
    float x = s[0] + sinf(t_ * 0.15f + s[2]) * 6, y = s[1] + cosf(t_ * 0.11f + s[2]) * 3;
    int X = (int)x, Y = (int)y;
    if (((int)(t_ * 2 + s[2])) % 3) { lpOn(X - 3, Y + 1, C(0xc8f4ec), M_SEA); lpOn(X + 3, Y + 1, C(0xc8f4ec), M_SEA); }
    lp(X, Y, C(0x5a3a28)); lp(X + 1, Y, C(0x5a3a28)); lp(X, Y - 1, C(0x3a2618)); lp(X + 1, Y - 1, C(0x3a2618));
    lp(X - 1, Y + 1, C(0xe8b088)); lp(X + 2, Y + 1, C(0xe8b088));
  }
}

// ================================================================ 3 Fischerhafen
static void buildHafen() {
  for (int y = 0; y < L; y++) for (int x = 0; x < L; x++) {
    if (y < HOR) bp(x, y, 0, M_SKY);
    else bp(x, y, seaCol(x, y, 200), M_SEA);
  }
  mountains(HOR + 2, 26, 40, 3, 0x8aa0b4, 0x7890a6);
  // Mole von rechts in den Hafen, aus grossen Bloecken
  for (int x = 96; x < L; x++) {
    float yy = 118 - (x - 96) * 0.06f;
    for (int j = -3; j <= 3; j++) {
      uint32_t h = hash2(x / 4, j / 2, 13);
      bp(x, (int)yy + j, C(j <= -2 ? 0xd8c8a8 : ((h & 3) ? 0xb8a78a : 0x9a8a70)), M_ROCK);
    }
  }
  for (int y = 112; y < 118; y++) for (int x = 92; x < 97; x++) bp(x, y, C(0xe8e0d0), M_OBJ);   // Molenkopf
  // Ort am Hang links: Felsuntergrund, darauf Haeuser in Reihen bis ans Wasser
  for (int y = 50; y < L; y++) for (int x = 0; x < L; x++) {
    float edge = 78 + (y - 60) * 0.12f + 3 * sinf(y * 0.2f);
    if (x < edge && y > 56 + x * 0.25f) bp(x, y, (vnoise(x / 6.0f, y / 6.0f, 15) > 0.55f) ? scrubCol(x, y) : limestone(x, y, edge - x, 17), M_ROCK);
  }
  const uint32_t walls[] = {0xe8d2a8, 0xf0e6d2, 0xd9b68a, 0xeadcc0, 0xe2c49a, 0xf2dcc4};
  const uint32_t shut[] = {0x3c7a52, 0x3c7a52, 0x4a6a8a, 0x3c7a52, 0x7a4a3a, 0x3c7a52};
  for (int row = 0; row < 6; row++) {
    int yb = 84 + row * 18;
    for (int k = 0; k < 6; k++) {
      int x = -4 + k * 14 + (row & 1) * 7 + (int)(hash2(k, row, 6) % 4);
      float edge = 78 + (yb - 60) * 0.12f;
      if (x + 12 > edge || x < -8 || (hash2(row, k, 7) % 5) == 0) continue;
      house(x, yb, 11 + (int)(hash2(k, row, 8) % 4), 9 + (int)(hash2(row, k, 9) % 6), walls[(k + row) % 6], shut[(k * 2 + row) % 6], (k + row) % 3 != 0, 1 + (k & 1));
    }
  }
  // Kirchturm oben im Ort
  fillRect(40, 54, 47, 82, C(0xd8c49e), M_GLOWWALL); fillRect(39, 52, 48, 54, C(0xb8a07a)); fillRect(42, 58, 45, 62, C(0x3a2e28), M_WINDOW);
  for (int j = 0; j < 6; j++) for (int i = 43 - j / 2; i <= 44 + j / 2; i++) bp(i, 46 + j, C(0x8e3f24));
  cypress(60, 82, 16); cypress(8, 66, 14);
  // Kai unten mit Pflaster
  for (int y = 197; y < L; y++) for (int x = 0; x < L; x++) bp(x, y, C((hash2(x / 3, y / 2, 4) & 3) ? 0xcbb894 : 0xb8a684), M_SAND);
  for (int x = 0; x < L; x++) for (int j = 0; j < 2; j++) bp(x, 196 + j, C(0x8a7a62), M_ROCK);
  for (int k = 0; k < 4; k++) palm(92 + k * 40, 226, 0.38f, k & 1 ? 3 : -3);
  // Poller am Kai
  for (int x = 70; x < 230; x += 22) { bp(x, 194, C(0x3a3430)); bp(x, 195, C(0x3a3430)); bp(x + 1, 194, C(0x3a3430)); }
  // Laternen an der Promenade
  for (int x = 76; x < 226; x += 30) { line(x, 186, x, 196, C(0x2e2a28)); addLight(x, 186, 0xffd890); }
  addLight(94, 111, 0xff4a3a);       // rotes Molenlicht
  for (int k = 0; k < 4; k++) addLight(10 + k * 18, 186 - k * 6, 0xffd890);
  addBoat(0, 30, 104, 3.2f); addBoat(1, 200, 96, -2.0f);
  addFlies(0, 0, 0, 0); nFlies = 0;
}
// Llauts am Kai, schaukelnd
static void animHafen(float dt) {
  (void)dt;
  const uint32_t hull[] = {0x3c8a6a, 0x2f6fb0, 0xd8403c, 0x3c8a6a, 0xe0a030};
  for (int k = 0; k < 5; k++) {
    int x0 = 74 + k * 30; float bob = sinf(t_ * 1.6f + k * 1.3f) * 0.8f; int y0 = 186 + (int)bob;
    for (int i = 0; i < 18; i++) {
      int w = (i < 2 || i > 15) ? 1 : 2;
      for (int j = 0; j < w + 1; j++) lp(x0 + i, y0 + j, j == 0 ? C(0xf2efe8) : C(hull[k]));
      lp(x0 + i, y0 + w + 1, C(0xb07a46));
    }
    for (int j = 1; j < 5; j++) for (int i = 6; i < 11; i++) lp(x0 + i, y0 - j, j == 4 ? C(0xb07a46) : C(0xe8e2d6));
    lp(x0 + 8, y0 - 3, C(0x2b3a5c));
    for (int j = 0; j < 9; j++) lp(x0 + 15, y0 - j, C(0x5a4030));   // Mast
    for (int i = 0; i < 18; i++) if ((hash2(i, k, (int)(t_ * 3)) & 3) == 0) lpOn(x0 + i, y0 + 5, C(0xd8f0f0), M_SEA);
  }
}

// ================================================================ 4 Olivenfinca im Hinterland
static void buildFinca() {
  for (int y = 0; y < HOR; y++) for (int x = 0; x < L; x++) bp(x, y, 0, M_SKY);
  mountains(HOR - 4, 46, 46, 7, 0x9aaccc, 0x8a9cbc);
  mountains(HOR + 6, 30, 30, 9, 0x6f8a6a, 0x5f7a5c);
  // Terrassenhang mit Trockenmauern
  for (int y = HOR; y < L; y++) for (int x = 0; x < L; x++) {
    if (matAt(x, y) != M_SKY) continue;
    float g = vnoise(x / 9.0f, y / 6.0f, 71);
    uint32_t c = g > 0.6f ? 0xa6a060 : (g > 0.35f ? 0x9a9258 : 0x8a8a4e);
    if ((hash2(x, y, 72) & 15) == 0) c = 0xb8a070;
    bp(x, y, C(c), M_SCRUB);
  }
  for (int k = 0; k < 6; k++) { int y = HOR + 12 + k * 20 + (k * k) % 5; stoneWall(0, L - 1, y); }
  // Finca rechts mit Turm
  house(140, 128, 34, 18, 0xe8d6b4, 0x3c7a52, true, 3);
  fillRect(170, 100, 178, 128, C(0xe2ceaa)); fillRect(169, 98, 179, 100, C(0x8e3f24));
  fillRect(173, 104, 174, 107, C(0x2b3a5c), M_WINDOW);
  cypress(132, 128, 26); cypress(184, 128, 22);
  // Olivenbaeume auf den Terrassen
  int pos[][3] = {{20, 124, 10}, {52, 120, 9}, {90, 126, 10}, {116, 120, 8}, {30, 146, 12}, {70, 150, 12}, {110, 148, 11},
                  {160, 150, 12}, {200, 146, 11}, {16, 170, 13}, {58, 176, 14}, {104, 172, 13}, {150, 174, 14}, {196, 176, 13},
                  {36, 198, 14}, {84, 202, 15}, {132, 200, 15}, {180, 202, 14}};
  for (auto &p : pos) olive(p[0], p[1], p[2] / 10.0f);
  almond(212, 124, 1.0f, almondBloom()); almond(8, 104, 0.9f, almondBloom());
  addLight(176, 112, 0xffd890);
  addFlies(10, 130, 220, 200);
}
// Schafe ziehen langsam ueber die Terrassen
static void animFinca(float dt) {
  (void)dt;
  static const float sh[5][3] = {{60, 187, 0}, {78, 189, 1.7f}, {140, 162, 3.1f}, {152, 164, 4.4f}, {100, 213, 5.5f}};
  for (auto &s : sh) {
    float x = s[0] + sinf(t_ * 0.05f + s[2]) * 14; int X = (int)x, Y = (int)s[1];
    bool left = cosf(t_ * 0.05f + s[2]) < 0;
    for (int j = 0; j < 3; j++) for (int i = 0; i < 5; i++) lp(X + i, Y - j, (j == 2 && (i == 0 || i == 4)) ? C(0xd8d4c8) : C(0xf2efe6));
    int hx = left ? X - 1 : X + 5; lp(hx, Y - 2, C(0x3a3430)); lp(hx, Y - 1, C(0x3a3430));
    lp(X + 1, Y + 1, C(0x3a3430)); lp(X + 3, Y + 1, C(0x3a3430));
  }
}

// ================================================================ 5 Bergdorf
static void buildDorf() {
  for (int y = 0; y < HOR; y++) for (int x = 0; x < L; x++) bp(x, y, 0, M_SKY);
  mountains(HOR + 4, 60, 50, 21, 0x8ea2b8, 0x7f92aa);
  mountains(HOR + 20, 36, 34, 23, 0x6a8466, 0x5c7458);
  for (int y = HOR; y < L; y++) for (int x = 0; x < L; x++)
    if (matAt(x, y) == M_SKY) bp(x, y, C(vnoise(x / 7.0f, y / 7.0f, 25) > 0.5f ? 0x6e8a4a : 0x5e7a40), M_SCRUB);
  // Haeuser locker am Hang (Natursteinfarben, gruene Laeden), dazwischen Gaerten
  const uint32_t stone[] = {0xc9a87c, 0xd8bc90, 0xbf9e74, 0xe0c8a0, 0xcfae84};
  for (int row = 0; row < 5; row++) {
    int yb = 118 + row * 22;
    for (int x = 0; x < L; x++) for (int j = 1; j < 3; j++) if (matAt(x, yb + j) == M_SCRUB) bp(x, yb + j, C(0xb8a888), M_SAND);   // Gasse/Mauer
    int x = 4 + (row % 2) * 14;
    for (int k = 0; x < 226; k++) {
      uint32_t h = hash2(k, row, 3);
      int w = 15 + (int)(h % 9), hh = 9 + (int)((h >> 4) % 8);
      if (x > 92 && x < 138 && row < 2) { x += 48; continue; }          // Platz fuer die Kirche
      if ((h >> 8) % 4 == 0) {                                           // Garten statt Haus
        if ((h >> 10) & 1) orangeTree(x + w / 2, yb, 0.9f); else cypress(x + w / 2, yb, 14);
      } else house(x, yb - (int)((h >> 12) % 3), w, hh, stone[(k + row) % 5], 0x3c7a52, ((h >> 14) % 3) != 0, w > 19 ? 2 : 1);
      x += w + 4 + (int)((h >> 16) % 6);
    }
  }
  // Kirche mit Glockenturm
  fillRect(100, 94, 124, 118, C(0xd8c098), M_GLOWWALL); fillRect(124, 100, 134, 118, C(0xc8b088), M_GLOWWALL);
  for (int j = 0; j < 8; j++) for (int i = 100 + j; i <= 124 - j; i++) bp(i, 91 - j, C(0xa05a3a));
  fillRect(104, 66, 113, 92, C(0xd0b890), M_GLOWWALL); fillRect(106, 72, 111, 78, C(0x3a2e28), M_WINDOW);
  for (int j = 0; j < 7; j++) for (int i = 104 + j / 2; i <= 113 - j / 2; i++) bp(i, 65 - j, C(0x8e3f24));
  fillRect(109, 100, 115, 110, C(0x6e4630)); disc(112, 99, 3, C(0x6e4630));
  for (int k = 0; k < 8; k++) orangeTree(12 + k * 28, 214 + (k % 2) * 6, 1.0f);
  for (int x = 20; x < 220; x += 40) { line(x, 196, x, 206, C(0x2e2a28)); addLight(x, 196, 0xffd890); }
  addFlies(10, 186, 220, 222);
}

// ================================================================ 6 Steilkueste mit Kuestenstrasse
static float c6cliff(int y) { return 150 - (y - 20) * 0.62f + 8 * sinf(y * 0.06f) + 2 * sinf(y * 0.4f); }
static float c6road(float x) { return 128 + 20 * sinf(x * 0.045f) - x * 0.05f; }
static void buildKueste() {
  for (int y = 0; y < L; y++) for (int x = 0; x < L; x++) {
    if (y < HOR) bp(x, y, 0, M_SKY); else bp(x, y, seaCol(x, y, 260), M_SEA);
  }
  // fernes Kap
  for (int x = 150; x < L; x++) { int top = (int)(HOR - 14 + (x - 150) * -0.08f + 4 * sinf(x * 0.09f)); for (int y = top; y <= HOR; y++) if (matAt(x, y) == M_SKY) bp(x, y, C(0x8a9ab0), M_ROCK); }
  // grosse Felswand links, die steil ins Meer faellt
  for (int y = 10; y < L; y++) {
    float cx = c6cliff(y);
    for (int x = 0; x < L && x < cx; x++) {
      float edge = cx - x;
      uint16_t c = limestone(x, y, edge, 29); Mat m = M_ROCK;
      if (vnoise(x / 6.0f, y / 5.0f, 81) > 0.66f && edge > 5) { c = scrubCol(x, y); m = M_SCRUB; }
      if (y > 200) c = mix(c, C(0x6a5a48), 0.25f);
      bp(x, y, c, m);
    }
  }
  // Serpentinenstrasse mit Mauerchen
  for (int x = 0; x < 120; x++) {
    int y = (int)c6road(x);
    if (x >= c6cliff(y) - 3) continue;
    for (int j = 0; j < 3; j++) bp(x, y + j, C(0x9a9490));
    bp(x, y + 3, C(0xe8e2d6));
  }
  pine(20, 60, 1.0f); pine(48, 84, 0.9f); pine(10, 150, 1.1f); pine(70, 104, 0.8f);
  // Aussichtsturm (Talaia)
  fillRect(30, 40, 38, 62, C(0xc8b088)); fillRect(29, 38, 39, 40, C(0xa89068)); for (int i = 29; i <= 39; i += 2) bp(i, 37, C(0xa89068));
  addLight(34, 37, 0xffd890);
  addBoat(0, 150, 126, -3.8f); addBoat(1, 60, 100, 2.4f); addBoat(0, 220, 178, -5.0f);
  addFlies(4, 60, 60, 120);
}
// Auto faehrt die Strasse entlang (nachts mit Scheinwerfern)
static float carX = 0;
static void animKueste(float dt) {
  carX += dt * 6; if (carX > 140) carX = -10;
  int X = (int)carX, Y = (int)c6road(carX);
  for (int i = 0; i < 6; i++) { lp(X + i, Y - 1, C(0xd8403c)); lp(X + i, Y, C(0xd8403c)); }
  for (int i = 1; i < 5; i++) lp(X + i, Y - 2, C(0x9ad0e8));
  lp(X + 1, Y + 1, C(0x222222)); lp(X + 4, Y + 1, C(0x222222));
}
static void glowKueste(float k) {
  if (k <= 0) return;
  int X = (int)carX, Y = (int)c6road(carX);
  for (int i = 0; i < 9; i++) {      // Lichtkegel der Scheinwerfer auf der Strasse
    int x = X + 6 + i, y = (int)c6road((float)x);
    if ((unsigned)x < (unsigned)L && (unsigned)y < (unsigned)L) lf[y * L + x] = mix(lf[y * L + x], C(0xfff2c0), k * (1 - i / 9.0f));
  }
  if ((unsigned)X < (unsigned)L && (unsigned)Y < (unsigned)L) lf[Y * L + X] = mix(lf[Y * L + X], C(0xff3030), k);   // Ruecklicht
}

// ================================================================ 7 Windmuehlen in der Ebene
static void molino(int x, int yb, float s) {
  int w = (int)(7 * s), h = (int)(26 * s);
  for (int j = 0; j < h; j++) { int ww = w - j * w / (h * 3); for (int i = -ww; i <= ww; i++) bp(x + i, yb - j, i > ww / 2 ? C(0xb8a07c) : C(0xd8c49e)); }
  for (int j = 0; j < (int)(4 * s); j++) for (int i = -w + 1 + j; i <= w - 1 - j; i++) bp(x + i, yb - h - j, C(0x7a5a40));
  fillRect(x - 1, yb - (int)(6 * s), x + 1, yb, C(0x5a4030));
}
static const int MOL[4][3] = {{58, 150, 10}, {170, 140, 8}, {120, 124, 5}, {200, 120, 4}};
static void buildMuehlen() {
  for (int y = 0; y < HOR; y++) for (int x = 0; x < L; x++) bp(x, y, 0, M_SKY);
  mountains(HOR + 2, 14, 50, 41, 0x9aaccc, 0x8a9cbc);
  for (int y = HOR; y < L; y++) for (int x = 0; x < L; x++) {
    if (matAt(x, y) != M_SKY) continue;
    // Felderflicken: weiter hinten flacher (Perspektive)
    float depth = (y - HOR) + 4.0f;
    float fx = x / (40.0f), fy = 18.0f / depth * 6;
    float n = vnoise(fx + 3, fy, 43);
    static const uint32_t f[5] = {0xc8a858, 0x8aa050, 0xa86a44, 0xb8b060, 0x7a9a48};
    uint32_t c = f[(int)(n * 5) % 5];
    if ((hash2(x, y, 44) & 7) == 0) c = lerpRGB(c, 0x000000, 0.12f);
    if ((int)(n * 50) % 10 == 0) c = 0x9a8a6a;   // Feldweg/Rand
    bp(x, y, C(c), M_SCRUB);
  }
  stoneWall(0, L - 1, 178); stoneWall(0, 150, 150);
  for (auto &m : MOL) molino(m[0], m[1], m[2] / 10.0f);
  bool bl = almondBloom();
  int al[][3] = {{20, 176, 11}, {100, 182, 12}, {150, 196, 13}, {210, 186, 11}, {40, 210, 14}, {120, 222, 14}, {190, 216, 13}, {86, 150, 8}};
  for (auto &a : al) almond(a[0], a[1], a[2] / 10.0f, bl);
  house(14, 134, 20, 10, 0xe0cba4, 0x3c7a52, true, 2);
  addFlies(10, 160, 220, 220);
}
// Fluegel drehen sich
static void animMuehlen(float dt) {
  (void)dt;
  for (int k = 0; k < 4; k++) {
    float s = MOL[k][2] / 10.0f; int cx = MOL[k][0], cy = MOL[k][1] - (int)(26 * s) - (int)(2 * s);
    float a0 = t_ * (0.6f + k * 0.15f);
    for (int b = 0; b < 6; b++) {
      float a = a0 + b * 1.0472f, c = cosf(a), sn = sinf(a);
      for (float r = 2; r < 18 * s; r += 0.7f) {
        lp(cx + (int)(c * r), cy + (int)(sn * r), C(0x5a4030));
        if (r > 5 * s) lp(cx + (int)(c * r - sn * 2 * s), cy + (int)(sn * r + c * 2 * s), C(0xf0ead8));
      }
    }
    lp(cx, cy, C(0x3a2a20));
  }
}

// ================================================================ 8 Palma mit Kathedrale
static void cathedral(int x, int yb) {
  const uint16_t st = C(0xd8b47c), sh = C(0xb8945e), dk = C(0x6a5034);
  fillRect(x, yb - 36, x + 70, yb, st, M_GLOWWALL);
  for (int i = 0; i < 8; i++) {   // Strebepfeiler mit Fialen
    int px = x + 2 + i * 9;
    fillRect(px, yb - 44, px + 2, yb, sh, M_GLOWWALL);
    for (int j = 0; j < 5; j++) bp(px + 1, yb - 45 - j, sh, M_GLOWWALL);
  }
  for (int i = 0; i < 7; i++) fillRect(x + 6 + i * 9, yb - 30, x + 7 + i * 9, yb - 20, dk, M_WINDOW);   // Spitzbogenfenster
  fillRect(x - 10, yb - 52, x + 2, yb, st, M_GLOWWALL);    // Hauptfassade mit Tuermchen
  for (int j = 0; j < 8; j++) { bp(x - 10, yb - 52 - j, sh, M_GLOWWALL); bp(x + 2, yb - 52 - j, sh, M_GLOWWALL); }
  disc(x - 4, yb - 38, 3, dk, M_WINDOW);                    // Rosette
  fillRect(x + 60, yb - 64, x + 68, yb - 36, st, M_GLOWWALL); // Glockenturm
  for (int i = 0; i < 3; i++) fillRect(x + 61 + i * 3, yb - 60, x + 61 + i * 3, yb - 54, dk, M_WINDOW);
}
static void buildPalma() {
  for (int y = 0; y < L; y++) for (int x = 0; x < L; x++) {
    if (y < HOR) bp(x, y, 0, M_SKY); else bp(x, y, seaCol(x, y, 250), M_SEA);
  }
  mountains(HOR - 2, 22, 60, 51, 0x9aaccc, 0x8a9cbc);
  // Stadtmauer und Ufer
  for (int y = 120; y < 136; y++) for (int x = 0; x < L; x++) bp(x, y, C((hash2(x / 4, y / 3, 52) & 3) ? 0xc8ae84 : 0xb09670), M_ROCK);
  for (int x = 0; x < L; x++) { bp(x, 136, C(0x8a7a62), M_ROCK); bp(x, 137, C(0x8a7a62), M_ROCK); }
  // Altstadt hinter der Mauer
  const uint32_t walls[] = {0xead6b0, 0xd8c098, 0xf0e2c6, 0xe0c49a};
  for (int k = 0; k < 9; k++) {
    int x = 120 + k * 13 + (int)(hash2(k, 0, 8) % 4);
    if (x > 228) break;
    house(x, 120, 12, 8 + (int)(hash2(k, 1, 8) % 10), walls[k & 3], 0x3c7a52, k & 1, 1);
  }
  for (int k = 0; k < 3; k++) house(4 + k * 13, 120, 12, 10 + k * 3, walls[k], 0x3c7a52, true, 1);
  cathedral(54, 120);
  // Palmenpromenade auf der Mauer
  for (int k = 0; k < 7; k++) palm(12 + k * 34, 136, 0.42f, k & 1 ? 3 : -3);
  // Masten im Jachthafen (rechts unten)
  for (int k = 0; k < 9; k++) {
    int x = 156 + k * 8, y = 176 + (k % 3) * 4;
    for (int j = 0; j < 24; j++) bp(x, y - j, C(0xe8e4dc), M_OBJ);
    fillRect(x - 4, y, x + 4, y + 2, C(0xf2efe8)); fillRect(x - 3, y + 3, x + 3, y + 3, C(0x2f6fb0));
  }
  for (int x = 140; x < L; x++) for (int j = 0; j < 2; j++) bp(x, 196 + j, C(0xa89068), M_JETTY);
  for (int x = 6; x < L; x += 18) addLight(x, 134, 0xffd890);
  addBoat(0, 30, 160, 3.0f); addBoat(2, 120, 186, -2.6f); addBoat(1, 180, 104, -1.8f);
  addFlies(0, 0, 0, 0); nFlies = 0;
}

// ================================================================ 9 Strandbar
static float c9shore(int x) { return 128 + 4 * sinf(x * 0.03f); }
static void buildStrandbar() {
  seaAndBeach([](int x) { return c9shore(x); });
  islet(150, 196, 9);
  // Chiringuito links: Holzbude mit Strohdach
  fillRect(14, 150, 70, 178, C(0xb68a5a)); fillRect(14, 168, 70, 170, C(0x7a5536));
  for (int x = 14; x <= 70; x += 6) fillRect(x, 150, x, 178, C(0x9a744a));
  fillRect(20, 156, 64, 164, C(0x3a2e28), M_WINDOW);   // Tresen-Oeffnung
  for (int j = 0; j < 12; j++) for (int i = 8 - j / 2; i <= 76 + j / 2; i++) bp(i, 138 + j, (hash2(i, j, 91) & 3) ? C(0xd8b46a) : C(0xb8944e));
  for (int i = 6; i <= 78; i += 2) { bp(i, 150, C(0xb8944e)); bp(i + 1, 151, C(0xb8944e)); }
  // Barhocker
  for (int k = 0; k < 4; k++) { int x = 24 + k * 12; fillRect(x, 182, x + 3, 183, C(0x6e4630)); line(x + 1, 184, x + 1, 189, C(0x4a3424)); }
  // Liegen mit Schirmen am Strand
  parasol(110, 174, 0x2f6fb0, 0xf6f0e6); parasol(150, 178, 0xd8403c, 0xf6f0e6); parasol(190, 174, 0x3c8a6a, 0xf6f0e6);
  palm(222, 236, 1.05f, -14); palm(4, 220, 0.8f, 10);
  // Lichterkette vom Dach zur Palme
  static const uint32_t cols[] = {0xffd060, 0xff8a6a, 0x9ad8ff, 0xffe8b0, 0xd8a0ff};
  for (int k = 0; k < 24; k++) { float u = k / 23.0f; int x = 76 + (int)(u * 128), y = 144 + (int)(sinf(u * 3.1416f) * 16) - (int)(u * 4); addLight(x, y, cols[k % 5], 1); }
  addLight(30, 160, 0xffc070); addLight(54, 160, 0xffc070);
  addBoat(3, 40, 118, 2.2f); addBoat(0, 200, 102, -2.0f);
  addFlies(0, 0, 0, 0); nFlies = 0;
}

// ================================================================ 10 Felsentor im Meer
static void buildFelsentor() {
  for (int y = 0; y < L; y++) for (int x = 0; x < L; x++) {
    if (y < HOR) bp(x, y, 0, M_SKY); else bp(x, y, seaCol(x, y, 240), M_SEA);
  }
  // grosse Felsnase mit Loch (rechts der Mitte)
  for (int y = 40; y < 170; y++) for (int x = 100; x < 220; x++) {
    float u = (x - 160) / 60.0f, top = 52 + 70 * u * u + 6 * sinf(x * 0.3f);
    if (y < top) continue;
    float wl = 150 + 10 * sinf(x * 0.15f);   // Wasserlinie unten
    if (y > wl) continue;
    // das Tor
    float hx = (x - 152) / 16.0f, hy = (y - 128) / 26.0f;
    if (hx * hx + hy * hy < 1 && y > 110) continue;
    float edge = fminf(y - top, wl - y);
    uint16_t c = limestone(x, y, x < 160 ? edge : -1, 33); Mat m = M_ROCK;
    if (y - top < 4 && (hash2(x, y, 34) & 1)) { c = scrubCol(x, y); m = M_SCRUB; }
    if (x > 168) c = mix(c, C(0x6a5440), 0.18f);
    float hr = hx * hx + hy * hy;
    if (y > 104 && hr < 1.35f) c = mix(c, C(0x4a3a2c), 0.55f * (1.35f - hr) / 0.35f);   // Schatten im Tor
    bp(x, y, c, m);
  }
  // Festland links unten mit Terrassen und Steinhaus
  for (int y = 120; y < L; y++) for (int x = 0; x < 100; x++) {
    float cx = 70 - (y - 120) * -0.2f + 6 * sinf(y * 0.12f), top = 120 + x * 0.35f;
    if (x > cx || y < top) continue;
    uint16_t c = limestone(x, y, cx - x, 35); Mat m = M_ROCK;
    if (vnoise(x / 5.0f, y / 5.0f, 36) > 0.5f) { c = scrubCol(x, y); m = M_SCRUB; }
    bp(x, y, c, m);
  }
  house(20, 140, 18, 10, 0xd8c098, 0x3c7a52, false, 2);
  pine(46, 146, 0.9f); pine(10, 136, 0.8f); bush(60, 160, 3);
  pine(150, 58, 0.8f); pine(176, 66, 0.7f);
  addBoat(0, 30, 190, 3.6f); addBoat(1, 200, 104, -2.2f); addBoat(2, 120, 214, -2.8f);
  addFlies(6, 130, 70, 160);
}

static const SceneDef SCENES[] = {
  {"DIE CALA", 92, buildCala, nullptr, nullptr},
  {"FELSBUCHT", 80, buildFelsbucht, animFelsbucht, nullptr},
  {"FISCHERHAFEN", 70, buildHafen, animHafen, nullptr},
  {"OLIVENFINCA", 110, buildFinca, animFinca, nullptr},
  {"BERGDORF", 100, buildDorf, nullptr, nullptr},
  {"STEILKÜSTE", 96, buildKueste, animKueste, glowKueste},
  {"WINDMÜHLEN", 120, buildMuehlen, animMuehlen, nullptr},
  {"PALMA", 96, buildPalma, nullptr, nullptr},
  {"STRANDBAR", 88, buildStrandbar, nullptr, nullptr},
  {"FELSENTOR", 96, buildFelsentor, nullptr, nullptr},
};
