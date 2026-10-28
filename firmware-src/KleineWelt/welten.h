// Kleine Welt - die Welten neben Mallorca: Kunst (Stroemung, Lavalampe, Sternenhimmel,
// Game of Life), Aquarium und Mond. Wird in game.cpp eingebunden (gleiche Hilfsfunktionen).
//
// Alles zeichnet in die Kunstpixel-Ebene lf (233x233); sichtbar ist der Kreis um (116,116)
// mit Radius 116. Die Mallorca-Puffer base/mat sind hier frei und dienen als Zwischenspeicher
// (beim Zurueckwechseln baut loadScene() die Mallorca-Szene neu auf).
// Alle Grafik wird im Code erzeugt bzw. ist eigene Pixelkunst.

constexpr float CX = 116.0f, CY = 116.0f, RAD = 116.0f;
enum { K_FLOW, K_LAVA, K_STARS, K_LIFE, NKUNST };
static const char *const KUNST_NAMES[NKUNST] = {"STRÖMUNG", "LAVALAMPE", "STERNENHIMMEL", "GAME OF LIFE"};

// ---------------------------------------------------------------- Hilfen
static inline float sq(float v) { return v * v; }
static inline bool inDisc(float x, float y, float r) { return sq(x - CX) + sq(y - CY) <= r * r; }
static inline float smooth01(float e0, float e1, float x) { float t = clampf((x - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t); }
static inline uint16_t addRGB(uint16_t a, uint32_t rgb, float k) {
  int r, g, b; unpack(a, r, g, b);
  return pack(r + (int)(((rgb >> 16) & 255) * k), g + (int)(((rgb >> 8) & 255) * k), b + (int)((rgb & 255) * k));
}
static inline void lpAdd(int x, int y, uint32_t rgb, float k) {
  if ((unsigned)x < (unsigned)L && (unsigned)y < (unsigned)L) lf[y * L + x] = addRGB(lf[y * L + x], rgb, k);
}
static inline void lpMix(int x, int y, uint16_t c, float k) {
  if ((unsigned)x < (unsigned)L && (unsigned)y < (unsigned)L) lf[y * L + x] = mix(lf[y * L + x], c, k);
}
static inline uint32_t scaleRGB(uint32_t c, float k) {
  int r = (int)(((c >> 16) & 255) * k), g = (int)(((c >> 8) & 255) * k), b = (int)((c & 255) * k);
  r = r > 255 ? 255 : r; g = g > 255 ? 255 : g; b = b > 255 ? 255 : b;
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}
// weiche Farbverlaeufe (Kosinus-Palette: a + b*cos(2pi(c*t + d)))
static uint32_t cosPal(float t, const float *p) {
  int ch[3];
  for (int i = 0; i < 3; i++) {
    float v = p[i] + p[3 + i] * cosf(6.2832f * (p[6 + i] * t + p[9 + i]));
    ch[i] = (int)(clampf(v, 0, 1) * 255);
  }
  return ((uint32_t)ch[0] << 16) | ((uint32_t)ch[1] << 8) | (uint32_t)ch[2];
}
static const float PALS[3][12] = {
  {0.50f, 0.50f, 0.55f, 0.50f, 0.45f, 0.45f, 1.0f, 1.0f, 1.0f, 0.00f, 0.15f, 0.30f},   // Meer: Tuerkis, Violett, Rosa
  {0.55f, 0.45f, 0.40f, 0.45f, 0.40f, 0.35f, 1.0f, 1.0f, 1.0f, 0.00f, 0.10f, 0.20f},   // Glut: Bernstein, Rot, Gold
  {0.40f, 0.55f, 0.55f, 0.40f, 0.45f, 0.45f, 1.0f, 0.8f, 0.9f, 0.55f, 0.35f, 0.25f},   // Nordlicht: Gruen, Blau, Lila
};
// Bild dunkler werden lassen (Nachleuchten); kleine Werte gehen sicher auf 0
static void fadeLf(int keep) {
  for (int i = 0; i < L * L; i++) {
    uint16_t c = lf[i]; if (!c) continue;
    int r = ((c >> 11) * keep) >> 8, g = (((c >> 5) & 63) * keep) >> 8, b = ((c & 31) * keep) >> 8;
    lf[i] = (uint16_t)((r << 11) | (g << 5) | b);
  }
}

// Farbe mit 4x4-Dither nach RGB565 (vermeidet Stufen in dunklen Verlaeufen)
static const int8_t BAYER4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
static inline uint16_t Cd(uint32_t rgb, int x, int y) {
  int o = BAYER4[(y & 3) * 4 + (x & 3)] - 8;
  return pack(((rgb >> 16) & 255) + o, ((rgb >> 8) & 255) + o / 2, (rgb & 255) + o);
}
// Zeilenbereich innerhalb des sichtbaren Kreises
static inline void discRow(int y, int &x0, int &x1) {
  float dy = y - CY, r2 = 117.5f * 117.5f - dy * dy;
  if (r2 <= 0) { x0 = 1; x1 = 0; return; }
  float h = sqrtf(r2); x0 = (int)(CX - h); x1 = (int)(CX + h) + 1;
  if (x0 < 0) x0 = 0;
  if (x1 > L - 1) x1 = L - 1;
}

// ================================================================= KUNST 1: Stroemung
// Teilchen folgen einem langsam wandernden Stroemungsfeld und hinterlassen leuchtende Spuren.
struct FlowPt { float x, y, age, life, hue; };
static FlowPt fpts[560];
static int flowPal = 0, flowSeed = 0;
static float burstX = 0, burstY = 0, burstT = 0;

static void flowSpawn(FlowPt &p) {
  for (int t = 0; t < 40; t++) { p.x = frand(6, 227); p.y = frand(6, 227); if (inDisc(p.x, p.y, 110)) break; }
  p.age = 0; p.life = frand(6, 16); p.hue = frand(0, 1);
}
static void initFlow() {
  memset(lf, 0, L * L * 2);
  for (auto &p : fpts) { flowSpawn(p); p.age = frand(0, p.life); }
  flowPal = rnd() % 3; flowSeed = rnd() % 1000; burstT = 0;
}
static float flowPot(float x, float y, float tt) {
  return vnoise(x / 52.0f + tt, y / 52.0f, 900 + flowSeed) + 0.45f * vnoise(x / 21.0f, y / 21.0f - tt, 901 + flowSeed);
}
static void renderFlow(float dt) {
  fadeLf(251);
  float tt = t_ * 0.03f;
  for (auto &p : fpts) {
    // Wirbelfeld (Curl-Noise): Teilchen kreisen, statt sich auf wenigen Linien zu sammeln
    const float e = 1.5f;
    float nx1 = flowPot(p.x + e, p.y, tt), nx0 = flowPot(p.x - e, p.y, tt);
    float ny1 = flowPot(p.x, p.y + e, tt), ny0 = flowPot(p.x, p.y - e, tt);
    float vx = (ny1 - ny0) / (2 * e) * 1400, vy = -(nx1 - nx0) / (2 * e) * 1400;
    float sp = sqrtf(vx * vx + vy * vy);
    if (sp > 26) { vx *= 26 / sp; vy *= 26 / sp; }
    if (burstT > 0) {   // Antippen: Teilchen werden weggeblasen
      float dx = p.x - burstX, dy = p.y - burstY, d2 = dx * dx + dy * dy + 40, d = sqrtf(d2);
      float f = fminf(110.0f, burstT * 12000 / d2);
      vx += dx / d * f; vy += dy / d * f;
    }
    float ox = p.x, oy = p.y;
    p.x += vx * dt; p.y += vy * dt; p.age += dt;
    float env = smooth01(0, 1.2f, p.age) * smooth01(p.life, p.life - 1.5f, p.age);
    uint32_t col = cosPal(p.hue * 0.7f + tt * 0.6f + p.y / 233.0f * 0.25f, PALS[flowPal]);
    int n = (int)fmaxf(fabsf(p.x - ox), fabsf(p.y - oy)) + 1;
    for (int k = 1; k <= n; k++) {
      float u = (float)k / n;
      lpAdd((int)(ox + (p.x - ox) * u), (int)(oy + (p.y - oy) * u), col, 0.60f * env);
    }
    if (p.age > p.life || !inDisc(p.x, p.y, 114)) flowSpawn(p);
  }
  if (burstT > 0) burstT -= dt;
}
static void tapFlow(int x, int y) { burstX = x; burstY = y; burstT = 1.0f; }

// ================================================================= KUNST 2: Lavalampe
// Wachsklumpen werden unten warm, steigen auf, kuehlen oben ab und sinken wieder.
// Das Feld wird in halber Aufloesung berechnet und weich hochgerechnet.
struct LavaBlob { float x, y, r, vy, heat, ph; };
static LavaBlob blobs[7];
static int lavaPal = 0;
constexpr int FW = 79, FS = 3;   // Feld 79x79, je 3 Kunstpixel
static const uint32_t LAVA_PAL[3][5] = {   // Hintergrund oben, unten, Rand, Kern, Schein
  {0x12041f, 0x3a0b2a, 0xd8301a, 0xffc04a, 0xff5420},
  {0x021620, 0x073436, 0x26b04e, 0xd0ff80, 0x40d070},
  {0x100626, 0x2a0a3c, 0xd83a84, 0xffc4e4, 0xff5aa8},
};
static uint16_t lavaLut[96];   // Farbe je Feldstaerke (f*32), ab f >= 1
static uint8_t lavaGlow[32];   // Schein-Anteil fuer 0.5 <= f < 1 (in 1/255)
static uint16_t lavaGlowC = 0;
static void initLava() {
  lavaPal = rnd() % 3;
  const uint32_t *P = LAVA_PAL[lavaPal];
  for (auto &b : blobs) b = {frand(62, 170), frand(50, 190), frand(12, 20), 0, frand(0, 1), frand(0, 6.28f)};
  // fester Hintergrund mit Lampenschein von unten
  for (int y = 0; y < L; y++) for (int x = 0; x < L; x++) {
    float glow = clampf(1 - (sq(x - CX) + sq(y - 250)) / (150.0f * 150.0f), 0, 1) * 0.35f;
    base[y * L + x] = Cd(lerpRGB(lerpRGB(P[0], P[1], y / 232.0f), P[4], glow * glow), x, y);
  }
  for (int k = 0; k < 96; k++) {
    float f = k / 32.0f, t = clampf((f - 1) / 1.8f, 0, 1);
    uint32_t c = lerpRGB(P[2], P[3], sqrtf(t));
    if (f < 1.07f) c = lerpRGB(c, P[3], 0.35f);   // helle Kante
    lavaLut[k] = C(c);
  }
  for (int k = 0; k < 32; k++) { float t = (k / 32.0f - 0.5f) / 0.5f; lavaGlow[k] = t > 0 ? (uint8_t)(t * t * 0.4f * 255) : 0; }
  lavaGlowC = C(P[4]);
}
static void renderLava(float dt) {
  float *fld = (float *)mat;
  for (auto &b : blobs) {
    float h = (b.y - 40) / 160.0f;                        // 0 oben .. 1 unten
    b.heat += dt * (h > 0.8f ? 0.14f : (h < 0.2f ? -0.14f : -0.01f));
    b.heat = clampf(b.heat, 0, 1.3f);
    float want = -(b.heat - 0.45f) * 20;                  // warm steigt, kalt sinkt
    b.vy += (want - b.vy) * dt * 0.5f;
    b.y += b.vy * dt;
    b.x += sinf(t_ * 0.19f + b.ph) * dt * 4;
    float dx = b.x - CX, dy = b.y - CY, d = sqrtf(dx * dx + dy * dy) + 0.01f, lim = 98 - b.r * 0.5f;
    if (d > lim) { b.x = CX + dx / d * lim; b.y = CY + dy / d * lim; }
  }
  // Feld: Summe r^2/d^2, unten ein Wachssee
  for (int j = 0; j < FW; j++) {
    float Y = j * (float)FS;
    for (int i = 0; i < FW; i++) {
      float X = i * (float)FS, f = 0;
      if (inDisc(X, Y, 122)) {
        for (auto &b : blobs) f += b.r * b.r / (sq(X - b.x) + sq(Y - b.y) + 1);
        float py = 226 - 5 * sinf(X * 0.045f + t_ * 0.4f) - Y;
        if (py < 1) py = 1;
        f += 160.0f / (py * py);
      }
      fld[j * FW + i] = f;
    }
  }
  for (int y = 0; y < L; y++) {
    int x0, x1; discRow(y, x0, x1);
    uint16_t *o = lf + y * L; const uint16_t *bg = base + y * L;
    for (int x = 0; x < x0; x++) o[x] = 0;
    for (int x = x1 + 1; x < L; x++) o[x] = 0;
    float fy = y * (1.0f / FS); int j0 = (int)fy; if (j0 > FW - 2) j0 = FW - 2; float wy = fy - j0;
    float row[FW];   // Feld senkrecht vorinterpoliert, dann nur noch waagerecht
    const float *r0 = fld + j0 * FW, *r1 = r0 + FW;
    for (int i = 0; i < FW; i++) row[i] = r0[i] + (r1[i] - r0[i]) * wy;
    int i0 = x0 / FS, sub = x0 % FS;
    for (int x = x0; x <= x1; x++) {
      int ii = i0 > FW - 2 ? FW - 2 : i0;
      float f = row[ii] + (row[ii + 1] - row[ii]) * (sub * (1.0f / FS));
      if (++sub == FS) { sub = 0; i0++; }
      if (f >= 1) { int k = (int)(f * 32); o[x] = lavaLut[k > 95 ? 95 : k]; }
      else if (f > 0.5f) { uint8_t g = lavaGlow[(int)(f * 32)]; o[x] = g ? mix(bg[x], lavaGlowC, g / 255.0f) : bg[x]; }
      else o[x] = bg[x];
    }
  }
}
static void tapLava(int x, int y) {
  int best = 0; float bd = 1e9f;
  for (int k = 0; k < 7; k++) { float d = sq(blobs[k].x - x) + sq(blobs[k].y - y); if (d < bd) { bd = d; best = k; } }
  blobs[best].heat = 1.3f; blobs[best].vy -= 8;
}

// ================================================================= KUNST 3: Sternenhimmel
// Der Himmel dreht sich langsam um den Polarstern in der Mitte, die Huegel bleiben stehen.
struct SkyStar { float r, a; uint8_t mag, ph; uint32_t col; };
static SkyStar sstars[110];
struct Meteor { bool on; float x, y, vx, vy, t; };
static Meteor meteors[3];
static float skyRot = 0;
static int16_t hillY[L];
static int houseX = 0, houseY = 0;

static void initStars() {
  // Himmelsbild (wird gedreht abgetastet): dunkler Grund, Milchstrasse, schwache Sterne
  const float a0 = 0.62f, ca = cosf(a0), sa = sinf(a0);
  for (int y = 0; y < L; y++) for (int x = 0; x < L; x++) {
    float dx = x - CX, dy = y - CY;
    uint32_t c = 0x03050f;
    if (dx * dx + dy * dy <= 118 * 118) {
      float dl = -sa * dx + ca * dy, along = ca * dx + sa * dy;
      float band = expf(-sq(dl / 26.0f));
      float n = vnoise(along / 18.0f, dl / 10.0f, 31) * 0.6f + vnoise(along / 7.0f, dl / 5.0f, 32) * 0.4f;
      float dust = vnoise(along / 12.0f + 3, dl / 4.0f, 33);
      float in = band * (0.3f + 0.7f * n) * (dust > 0.64f ? 0.4f : 1.0f);
      c = lerpRGB(c, 0x34324e, clampf(in * 0.9f, 0, 1));
      uint32_t h = hash2(x, y, 7);
      if (h % 150 == 0) c = lerpRGB(c, 0xd8e0ff, 0.25f + (h >> 8 & 63) / 160.0f);
      else if (band > 0.4f && h % 40 == 1) c = lerpRGB(c, 0xc0c8e8, 0.22f);
    }
    base[y * L + x] = C(c);
  }
  static const uint32_t SCOL[4] = {0xbfd6ff, 0xffffff, 0xfff0cc, 0xffcf9a};
  for (auto &s : sstars) {
    int m = rnd() % 100;
    s = {sqrtf(frand(0, 1)) * 114, frand(0, 6.2832f), (uint8_t)(m < 5 ? 3 : (m < 20 ? 2 : (m < 55 ? 1 : 0))), (uint8_t)(rnd() & 255), SCOL[rnd() % 4]};
  }
  for (int x = 0; x < L; x++)
    hillY[x] = (int16_t)(203 - 12 * vnoise(x / 42.0f, 0.5f, 61) - 6 * vnoise(x / 13.0f, 1.5f, 62));
  // ein paar Kiefern und ein Haeuschen als Schattenriss
  const int px[3] = {38, 47, 176};
  for (int k = 0; k < 3; k++) {
    int h = k == 1 ? 15 : 21, foot = hillY[px[k]];
    for (int j = 0; j < h; j++) {
      int w = (h - j) / 3;
      for (int i = -w; i <= w; i++) { int X = px[k] + i; if ((unsigned)X < (unsigned)L && foot - j < hillY[X]) hillY[X] = (int16_t)(foot - j); }
    }
  }
  houseX = 130; houseY = hillY[136];
  for (int x = houseX; x < houseX + 12; x++) { int top = houseY - 8 - (x < houseX + 6 ? x - houseX : houseX + 11 - x) / 2; if (top < hillY[x]) hillY[x] = (int16_t)top; }
  for (auto &m : meteors) m.on = false;
  skyRot = frand(0, 6.28f);
}
static void spawnMeteor(float x, float y) {
  for (auto &m : meteors) if (!m.on) {
    float a = frand(0.35f, 0.9f) * (rnd() & 1 ? 1 : -1) + 1.5708f;
    m = {true, x, y, cosf(a) * 120, sinf(a) * 120, 0};
    return;
  }
}
static void renderStars(float dt) {
  skyRot += dt * 0.006f;
  float cs = cosf(skyRot), sn = sinf(skyRot);
  for (int y = 0; y < L; y++) {
    float hz = clampf((y - 110) / 95.0f, 0, 1);
    uint32_t glow = scaleRGB(0x12204a, hz * hz);         // Horizont-Schimmer
    float dy = y - CY;
    int x0, x1; discRow(y, x0, x1);
    for (int x = 0; x < x0; x++) lf[y * L + x] = 0;
    for (int x = x1 + 1; x < L; x++) lf[y * L + x] = 0;
    for (int x = x0; x <= x1; x++) {
      if (y >= hillY[x]) { lf[y * L + x] = C(y - hillY[x] < 2 ? 0x0b1020 : 0x020307); continue; }
      float dx = x - CX;
      int sx = (int)(CX + dx * cs + dy * sn + 0.5f), sy = (int)(CY - dx * sn + dy * cs + 0.5f);
      uint16_t c = ((unsigned)sx < (unsigned)L && (unsigned)sy < (unsigned)L) ? base[sy * L + sx] : 0;
      lf[y * L + x] = glow ? addRGB(c, glow, 1.0f) : c;
    }
  }
  for (auto &s : sstars) {
    float a = s.a + skyRot;
    int x = (int)(CX + cosf(a) * s.r), y = (int)(CY + sinf(a) * s.r);
    if ((unsigned)x >= (unsigned)L || (unsigned)y >= (unsigned)L || y >= hillY[x] - 1) continue;
    float tw = 0.7f + 0.3f * sinf(t_ * (2.0f + (s.ph & 3)) + s.ph);
    float b = (0.35f + 0.22f * s.mag) * tw;
    lpAdd(x, y, s.col, b);
    if (s.mag >= 2) { float k = s.mag == 3 ? 0.4f : 0.18f; lpAdd(x + 1, y, s.col, b * k); lpAdd(x - 1, y, s.col, b * k); lpAdd(x, y + 1, s.col, b * k); lpAdd(x, y - 1, s.col, b * k); }
  }
  // Fensterlicht im Haeuschen
  lp(houseX + 4, houseY - 4, C(0xffc860)); lp(houseX + 5, houseY - 4, C(0xffb040));
  if ((rnd() % 1000) < (int)(dt * 1000 / 24)) spawnMeteor(frand(40, 190), frand(20, 90));
  for (auto &m : meteors) if (m.on) {
    m.t += dt; m.x += m.vx * dt; m.y += m.vy * dt;
    float life = 1 - m.t / 0.9f;
    if (life <= 0) { m.on = false; continue; }
    float sp = sqrtf(m.vx * m.vx + m.vy * m.vy);
    for (int k = 0; k < 20; k++) {
      int x = (int)(m.x - m.vx / sp * k), y = (int)(m.y - m.vy / sp * k);
      if ((unsigned)x < (unsigned)L && (unsigned)y < (unsigned)L && y < hillY[x]) lpAdd(x, y, 0xf0f4ff, life * (1 - k / 20.0f) * 0.85f);
    }
  }
}
static void tapStars(int x, int y) { spawnMeteor(x, y - 10); }

// ================================================================= KUNST 4: Game of Life
// Conways Regeln auf einem runden Feld. Junge Zellen leuchten hell, alte kuehlen ab,
// gestorbene glimmen nach. Erstarrt das Feld, wird neu gesaet.
constexpr int GN = 77;     // 77 Zellen zu je 3 Kunstpixeln
static uint8_t lcell[GN * GN], lnext[GN * GN], lage[GN * GN], lglow[GN * GN], lwall[GN * GN];
static float lifeAcc = 0;
static int lifeGen = 0, lifeStag = 0, lifePop = 0, lifePal = 0;
static uint32_t lifeHist[4];

static void lifeSow(int ci, int cj, int rad, int dens) {
  for (int j = cj - rad; j <= cj + rad; j++) for (int i = ci - rad; i <= ci + rad; i++)
    if ((unsigned)i < (unsigned)GN && (unsigned)j < (unsigned)GN && !lwall[j * GN + i] && (int)(rnd() % 100) < dens) { lcell[j * GN + i] = 1; lage[j * GN + i] = 1; }
}
static void initLife() {
  for (int j = 0; j < GN; j++) for (int i = 0; i < GN; i++) {
    int k = j * GN + i;
    lwall[k] = !inDisc(2.5f + i * 3, 2.5f + j * 3, 112);
    lcell[k] = 0; lage[k] = 0; lglow[k] = 0;
  }
  for (int j = 0; j < GN; j++) for (int i = 0; i < GN; i++)
    if (!lwall[j * GN + i] && inDisc(2.5f + i * 3, 2.5f + j * 3, 80) && rnd() % 100 < 33) { lcell[j * GN + i] = 1; lage[j * GN + i] = 1; }
  lifeAcc = 0; lifeGen = 0; lifeStag = 0; lifePal = rnd() % 3;
  memset(lifeHist, 0, sizeof lifeHist);
}
static void lifeStep() {
  uint32_t h = 2166136261u; int pop = 0;
  for (int j = 0; j < GN; j++) for (int i = 0; i < GN; i++) {
    int k = j * GN + i;
    if (lwall[k]) { lnext[k] = 0; continue; }
    int n = 0;
    for (int dj = -1; dj <= 1; dj++) for (int di = -1; di <= 1; di++) {
      if (!di && !dj) continue;
      int a = i + di, b = j + dj;
      if ((unsigned)a < (unsigned)GN && (unsigned)b < (unsigned)GN) n += lcell[b * GN + a];
    }
    uint8_t alive = lcell[k] ? (n == 2 || n == 3) : (n == 3);
    lnext[k] = alive;
    if (alive) { pop++; h = (h ^ (uint32_t)k) * 16777619u; }
  }
  for (int k = 0; k < GN * GN; k++) {
    lage[k] = lnext[k] ? (lcell[k] ? (uint8_t)(lage[k] < 250 ? lage[k] + 1 : 250) : 1) : 0;
    lcell[k] = lnext[k];
  }
  lifeGen++; lifePop = pop;
  bool repeat = false;
  for (uint32_t o : lifeHist) if (o == h) repeat = true;
  lifeHist[lifeGen & 3] = h;
  if (repeat || pop < 25) lifeStag++; else if (lifeStag > 0) lifeStag--;
  if (lifeStag > 40) {   // erstarrt oder fast leer: neu saeen
    for (int k = 0; k < 4; k++) lifeSow(15 + rnd() % 47, 15 + rnd() % 47, 6, 40);
    lifeStag = 0; lifePal = (lifePal + 1) % 3;
  } else if (lifeGen % 500 == 0) lifeSow(15 + rnd() % 47, 15 + rnd() % 47, 4, 40);
}
static void renderLife(float dt) {
  lifeAcc += dt;
  if (lifeAcc > 0.16f) { lifeAcc = 0; lifeStep(); }
  memset(lf, 0, L * L * 2);
  int up = (int)(dt * 1100), down = (int)(dt * 140) + 1;
  for (int j = 0; j < GN; j++) for (int i = 0; i < GN; i++) {
    int k = j * GN + i;
    if (lwall[k]) continue;
    int g = lglow[k];
    g = lcell[k] ? (g + up > 255 ? 255 : g + up) : (g - down < 0 ? 0 : g - down);
    lglow[k] = (uint8_t)g;
    if (!g) continue;
    uint32_t c;
    if (lcell[k]) {
      c = lage[k] <= 2 ? 0xeafff8 : cosPal(0.1f + lage[k] / 70.0f, PALS[lifePal]);
      int mx = (int)fmaxf(fmaxf((c >> 16) & 255, (c >> 8) & 255), c & 255);
      c = scaleRGB(c, (mx < 210 ? 210.0f / (mx + 1) : 1.0f) * g / 255.0f);
    } else c = scaleRGB(0x2a2a6a, g / 255.0f);
    uint16_t cc = C(c);
    int x = 1 + i * 3, y = 1 + j * 3;
    lf[y * L + x] = cc; lf[y * L + x + 1] = cc; lf[(y + 1) * L + x] = cc; lf[(y + 1) * L + x + 1] = cc;
  }
  for (float a = 0; a < 6.2832f; a += 0.004f) lpAdd((int)(CX + cosf(a) * 114), (int)(CY + sinf(a) * 114), 0x30304a, 0.6f);
}
static void tapLife(int x, int y) { lifeSow(x / 3, y / 3, 3, 55); }

// ================================================================= AQUARIUM
// Das runde Display ist das Glas. Kies, Pflanzen, Steine und eine Amphore haengen am Glas,
// die Wasserlinie bleibt waagerecht (Lagesensor) und schwappt nach. Antippen = fuettern.
static float gravX = 0, gravY = 1;                 // gemessene Schwerkraft (Bildkoordinaten)
static float waterA = 0, waterW = 0, waterTarget = 0;
static float wgx = 0, wgy = 1, wpx = 1, wpy = 0;   // Einheitsvektoren: nach unten / entlang der Oberflaeche
constexpr float WATER_H = 74;                      // Oberflaeche liegt 74 px ueber der Mitte
static float waveTab[360], rayTab[360];
static int16_t gravelTop[L];

static inline float waveAt(float s) { int i = (int)s + 180; if (i < 0) i = 0; if (i > 359) i = 359; return waveTab[i]; }
static inline float depthAt(float x, float y) {   // Tiefe unter der Oberflaeche (negativ = Luft)
  float dx = x - CX, dy = y - CY;
  return dx * wgx + dy * wgy + WATER_H - waveAt(dx * wpx + dy * wpy);
}

// Fische (eigene Pixelkunst, Kopf rechts)
static const char *const FISH_GOLD[8] = {
  "......ff.....",
  "t....fOOOo...",
  "tt..oOOOOOOo.",
  "tTtoOOOOOOwko",
  "tTtoOOOOOOOOo",
  "tt..oOOOOOOo.",
  "t....ooOOo...",
  "......f......"};
static const char *const FISH_BLUE[7] = {
  "....BBBB....",
  "...BbbbbbB..",
  "y.BbbbbbbwkB",
  "yyBbbbbbbbbB",
  "y.BbbbbbbbB.",
  "...BbbbbbB..",
  "....BBBB...."};
static const char *const FISH_ANGEL[11] = {
  "....s....",
  "...sS....",
  "..sSSs...",
  "..SsSSs..",
  "t.SsSSSs.",
  "ttSsSSSwk",
  "t.SsSSSs.",
  "..SsSSs..",
  "..sSSs...",
  "...sS....",
  "....s...."};
static const char *const FISH_NEON[3] = {
  "u.nnnn.",
  "ucccccw",
  "u.rrrr."};
struct FishArt { const char *const *rows; int w, h; };
static const FishArt FISH_ARTS[4] = {{FISH_GOLD, 13, 8}, {FISH_BLUE, 12, 7}, {FISH_ANGEL, 9, 11}, {FISH_NEON, 7, 3}};
static uint32_t fishCol(char ch) {
  switch (ch) {
    case 'O': return 0xf28a2a; case 'o': return 0xc2581a; case 'f': return 0xf6b264; case 't': return 0xf6a850;
    case 'T': return 0xd06a20; case 'w': return 0xffffff; case 'k': return 0x101018; case 'B': return 0x1a3a9a;
    case 'b': return 0x3a7ae8; case 'y': return 0xf2d23a; case 'S': return 0xe6e8f0; case 's': return 0x34343e;
    case 'n': return 0x9ab0d0; case 'c': return 0x40f0ff; case 'r': return 0xe83040; case 'u': return 0x7a8a9c;
    default: return 0;
  }
}
static bool tailPx(char ch) { return ch == 't' || ch == 'T' || ch == 'y' || ch == 'u'; }

struct AqFish { float x, y, vx, vy, tx, ty, speed, ph, scared; uint8_t kind; int8_t leader; };
static AqFish afish[12]; static int nAfish = 0;
struct Flake { bool on, rest; float x, y, t; };
static Flake flakes[16]; static int flakesEaten = 0;
struct Bubble { bool on; float x, y, ph, r; };
static Bubble bubbles[24];
static float bubbleAcc = 0;
struct Plant { int16_t x, y, h; float ph; uint32_t c1, c2; };
static Plant plants[7]; static int nPlants = 0;
static int bubblerX = 156;

static bool goodSpot(float x, float y, float margin) {
  if (!inDisc(x, y, 102)) return false;
  int xi = (int)clampf(x, 0, L - 1);
  if (y > gravelTop[xi] - 7) return false;
  return depthAt(x, y) > margin;
}
static void fishTarget(AqFish &f) {
  bool night = light.day < 0.3f;
  for (int t = 0; t < 40; t++) {
    float x = frand(18, 215), y = night ? frand(140, 200) : frand(40, 200);
    if (goodSpot(x, y, 14)) { f.tx = x; f.ty = y; return; }
  }
  f.tx = CX; f.ty = CY + 50;
}
static void addFish(int kind, int leader) {
  if (nAfish >= 12) return;
  AqFish &f = afish[nAfish++];
  for (int t = 0; t < 50; t++) { f.x = frand(30, 200); f.y = frand(70, 190); if (goodSpot(f.x, f.y, 14)) break; }
  f.vx = frand(-5, 5); f.vy = 0; f.kind = (uint8_t)kind; f.leader = (int8_t)leader;
  f.speed = kind == 3 ? 15 : (kind == 2 ? 8 : 11) * frand(0.85f, 1.15f);
  f.ph = frand(0, 6.28f); f.scared = 0;
  fishTarget(f);
}
static void initAquarium() {
  memset(mat, 0, L * L);
  waterA = waterTarget = atan2f(gravX, gravY); waterW = 0;
  wgx = sinf(waterA); wgy = cosf(waterA); wpx = wgy; wpy = -wgx;
  for (int i = 0; i < 360; i++) waveTab[i] = 0;
  // Kies
  for (int x = 0; x < L; x++) gravelTop[x] = (int16_t)(193 + 7 * vnoise(x / 20.0f, 0.5f, 71) + 3 * vnoise(x / 6.0f, 1.5f, 72));
  for (int x = 0; x < L; x++) for (int y = gravelTop[x]; y < L; y++) {
    if (!inDisc(x, y, 117)) continue;
    uint32_t h = hash2(x / 2, y / 2, 73);
    static const uint32_t PEB[5] = {0xb89a70, 0x8a7458, 0xd8c4a0, 0x6e5a46, 0xa48466};
    uint32_t c = PEB[h % 5];
    if (y == gravelTop[x]) c = lerpRGB(c, 0xf0e6cc, 0.3f);
    if ((hash2(x, y, 74) & 7) == 0) c = lerpRGB(c, 0x000000, 0.25f);
    base[y * L + x] = C(c); mat[y * L + x] = 1;
  }
  // zwei Steine
  struct { int x, rx, ry; } rocks[2] = {{58, 17, 12}, {183, 12, 9}};
  for (auto &r : rocks) {
    int cy = gravelTop[r.x] - r.ry / 2;
    for (int y = cy - r.ry; y <= cy + r.ry; y++) for (int x = r.x - r.rx; x <= r.x + r.rx; x++) {
      float u = (x - r.x) / (float)r.rx, v = (y - cy) / (float)r.ry;
      if (u * u + v * v > 1 || (unsigned)x >= (unsigned)L || (unsigned)y >= (unsigned)L) continue;
      float n = vnoise(x / 4.0f, y / 4.0f, 75);
      uint32_t c = lerpRGB(0x5c6670, 0x9aa4ac, n);
      if (v < -0.55f && u < 0.2f) c = lerpRGB(c, 0xd0d8dc, 0.35f);
      if (u > 0.5f || v > 0.5f) c = lerpRGB(c, 0x202830, 0.35f);
      base[y * L + x] = C(c); mat[y * L + x] = 1;
    }
  }
  // umgekippte Amphore (ein Gruss aus Mallorca)
  {
    int ax = 112, ay = gravelTop[112] - 6;
    for (int y = ay - 7; y <= ay + 7; y++) for (int x = ax - 13; x <= ax + 13; x++) {
      float u = (x - ax) / 11.0f, v = (y - ay) / 6.5f;
      bool body = u * u + v * v <= 1;
      bool neck = x > ax + 9 && x <= ax + 15 && abs(y - ay) <= 2;
      if (!body && !neck) continue;
      uint32_t c = v < -0.3f ? 0xd8784a : (v > 0.45f ? 0x8a4426 : 0xc0643a);
      if (neck && x == ax + 15) c = 0x6a3018;
      if (body && (hash2(x, y, 76) & 15) == 0) c = 0x9a5a3a;
      if ((unsigned)x < (unsigned)L && (unsigned)y < (unsigned)L) { base[y * L + x] = C(c); mat[y * L + x] = 1; }
    }
    for (int k = 0; k < 4; k++) { int i = (ay - 4 - k) * L + ax + 8 + k / 2; base[i] = C(0xa85634); mat[i] = 1; }   // Henkel
  }
  // Sprudelstein
  bubblerX = 156;
  for (int i = -3; i <= 3; i++) for (int j = -1; j <= 1; j++) {
    int x = bubblerX + i, y = gravelTop[bubblerX] - 1 + j;
    base[y * L + x] = C(j < 0 ? 0x5a5a64 : 0x3a3a44); mat[y * L + x] = 1;
  }
  // Pflanzen
  nPlants = 0;
  const int px[7] = {26, 36, 80, 92, 140, 198, 207};
  const int ph_[7] = {58, 44, 70, 50, 62, 48, 36};
  for (int k = 0; k < 7; k++)
    plants[nPlants++] = {(int16_t)px[k], (int16_t)(gravelTop[px[k]] + 1), (int16_t)ph_[k], frand(0, 6.28f),
                         (uint32_t)(k % 3 == 1 ? 0x2c6a3a : 0x3a7a30), (uint32_t)(k % 3 == 1 ? 0x56a85a : (k % 3 == 2 ? 0x8ab43c : 0x5c9a40))};
  // Bewohner
  nAfish = 0;
  addFish(0, -1); addFish(0, -1); addFish(1, -1); addFish(1, -1); addFish(2, -1);
  int lead = nAfish; addFish(3, -1);
  for (int k = 0; k < 6; k++) addFish(3, lead);
  for (auto &f : flakes) f.on = false;
  for (auto &b : bubbles) b.on = false;
  flakesEaten = 0; bubbleAcc = 0;
}
static void feed(float x, float y) {
  float d = depthAt(x, y);
  float sx = x - wgx * (d - 2), sy = y - wgy * (d - 2);   // senkrecht hoch zur Oberflaeche
  int made = 0;
  for (auto &f : flakes) if (!f.on && made < 6) {
    float o = frand(-9, 9);
    f = {true, false, sx + wpx * o, sy + wpy * o, frand(0, 3)};
    made++;
  }
}
static void updateAquarium(float dt) {
  // Schwappen: Wasserlinie folgt der Schwerkraft wie eine gedaempfte Feder
  float gl = sqrtf(gravX * gravX + gravY * gravY);
  if (gl > 0.35f) waterTarget = atan2f(gravX, gravY);
  float diff = waterTarget - waterA;
  while (diff > 3.1416f) diff -= 6.2832f;
  while (diff < -3.1416f) diff += 6.2832f;
  waterW += (diff * 9.0f - waterW * 1.25f) * dt;
  waterA += waterW * dt;
  wgx = sinf(waterA); wgy = cosf(waterA); wpx = wgy; wpy = -wgx;
  for (int i = 0; i < 360; i++) {
    float s = i - 180.0f;
    waveTab[i] = 1.1f * sinf(0.085f * s + 1.5f * t_) + 0.5f * sinf(0.21f * s - 2.2f * t_) + clampf(waterW, -2, 2) * s * 0.12f;
    float r = sinf(0.06f * s + 0.25f * t_ + 2 * sinf(0.017f * s - 0.11f * t_));
    rayTab[i] = r > 0 ? r * r * r * r * r * r : 0;
  }
  bool night = light.day < 0.3f;
  float pace = night ? 0.4f : 1.0f;
  for (int k = 0; k < nAfish; k++) {
    AqFish &f = afish[k];
    float tx = f.tx, ty = f.ty, sp = f.speed * pace;
    int food = -1; float fd = 150 * 150;
    for (int q = 0; q < 16; q++) if (flakes[q].on) { float d = sq(flakes[q].x - f.x) + sq(flakes[q].y - f.y); if (d < fd) { fd = d; food = q; } }
    if (food >= 0 && f.scared <= 0) {
      tx = flakes[food].x; ty = flakes[food].y; sp = f.speed * 2.2f * (night ? 0.6f : 1.0f);
      if (fd < 3.5f * 3.5f) { flakes[food].on = false; flakesEaten++; }
    } else if (f.leader >= 0) {
      const AqFish &l = afish[f.leader];
      tx = l.x - (l.vx > 0 ? 1 : -1) * (6 + (k % 3) * 5); ty = l.y + ((k % 4) - 1.5f) * 5;
      sp = f.speed * 1.2f * pace;
    } else if (sq(f.tx - f.x) + sq(f.ty - f.y) < 36 || !goodSpot(f.tx, f.ty, 10) || (rnd() % 1000) < (int)(dt * 1000 / 9)) {
      fishTarget(f);
    }
    if (f.scared > 0) { f.scared -= dt; sp *= 3.0f; }
    float dx = tx - f.x, dy = ty - f.y, d = sqrtf(dx * dx + dy * dy) + 0.01f;
    float wx = dx / d * sp, wy = dy / d * sp * 0.6f;
    if (d < 4) { wx *= d / 4; wy *= d / 4; }
    float k2 = fminf(1.0f, dt * 1.6f);
    f.vx += (wx - f.vx) * k2; f.vy += (wy - f.vy) * k2;
    f.x += f.vx * dt; f.y += f.vy * dt;
    // im Wasser und im Glas bleiben
    float dep = depthAt(f.x, f.y);
    if (dep < 6) { f.x += wgx * (6 - dep) * dt * 3; f.y += wgy * (6 - dep) * dt * 3; }
    float rx = f.x - CX, ry = f.y - CY, rr = sqrtf(rx * rx + ry * ry);
    if (rr > 104) { f.x = CX + rx / rr * 104; f.y = CY + ry / rr * 104; }
    int xi = (int)clampf(f.x, 0, L - 1);
    if (f.y > gravelTop[xi] - 5) f.y = gravelTop[xi] - 5;
    f.ph += dt * (3 + fabsf(f.vx) * 0.4f);
  }
  for (int q = 0; q < 16; q++) {
    Flake &f = flakes[q];
    if (!f.on) continue;
    f.t += dt;
    if (!f.rest) {
      float sink = depthAt(f.x, f.y) < 0 ? 25 : 5;
      float wob = sinf(f.t * 2 + q) * 3;
      f.x += (wgx * sink + wpx * wob) * dt; f.y += (wgy * sink + wpy * wob) * dt;
      int xi = (int)clampf(f.x, 0, L - 1);
      if (f.y >= gravelTop[xi] - 1 || !inDisc(f.x, f.y, 110)) { f.rest = true; f.t = 0; }
    } else if (f.t > 30) f.on = false;
  }
  bubbleAcc += dt;
  if (bubbleAcc > (night ? 0.8f : 0.3f)) {
    bubbleAcc = 0;
    for (auto &b : bubbles) if (!b.on) { b = {true, bubblerX + frand(-1.5f, 1.5f), (float)gravelTop[bubblerX] - 3, frand(0, 6.28f), rnd() % 3 == 0 ? 1.6f : 0.8f}; break; }
  }
  for (auto &b : bubbles) if (b.on) {
    b.ph += dt * 5;
    float w = sinf(b.ph) * 6;
    b.x += (-wgx * 20 + wpx * w) * dt; b.y += (-wgy * 20 + wpy * w) * dt;
    if (depthAt(b.x, b.y) < 0.5f || !inDisc(b.x, b.y, 112)) b.on = false;
  }
}
static void drawFish(const AqFish &f, float lamp) {
  const FishArt &A = FISH_ARTS[f.kind];
  bool flip = f.vx < 0;
  int x0 = (int)(f.x - A.w / 2), y0 = (int)(f.y - A.h / 2);
  int wig = sinf(f.ph) > 0.3f ? 1 : (sinf(f.ph) < -0.3f ? -1 : 0);
  for (int j = 0; j < A.h; j++) for (int i = 0; i < A.w; i++) {
    char ch = A.rows[j][flip ? A.w - 1 - i : i];
    if (ch == '.') continue;
    int X = x0 + i, Y = y0 + j + (tailPx(ch) ? wig : 0);
    if ((unsigned)X >= (unsigned)L || (unsigned)Y >= (unsigned)L || depthAt(X, Y) < 0) continue;
    uint32_t c = scaleRGB(fishCol(ch), lamp);
    lf[Y * L + X] = tailPx(ch) ? mix(lf[Y * L + X], C(c), 0.75f) : C(c);
  }
}
static void renderAquarium(float dt) {
  updateAquarium(dt);
  float day = light.day, lamp = 0.32f + 0.68f * day;
  uint32_t shallow = lerpRGB(0x0a2a46, 0x34aec8, day), deep = lerpRGB(0x030c1a, 0x0c4868, day);
  uint16_t depthCol[64];
  for (int k = 0; k < 64; k++) depthCol[k] = C(lerpRGB(shallow, deep, k / 63.0f));
  const uint16_t airCol = C(0x06090e), lineCol = C(lerpRGB(0x3a6a8a, 0xc8f6ff, day));
  int tick4 = (int)(t_ * 4);
  bool rays = day > 0.05f;
  for (int y = 0; y < L; y++) {
    int x0, x1; discRow(y, x0, x1);
    uint16_t *o = lf + y * L; const uint16_t *bs = base + y * L; const uint8_t *mt = mat + y * L;
    for (int x = 0; x < x0; x++) o[x] = 0;
    for (int x = x1 + 1; x < L; x++) o[x] = 0;
    float dy = y - CY, dx0 = x0 - CX;
    float s = dx0 * wpx + dy * wpy, dl = dx0 * wgx + dy * wgy + WATER_H;   // laufen pro Spalte weiter
    for (int x = x0; x <= x1; x++, s += wpx, dl += wgx) {
      float d = dl - waveAt(s);
      uint16_t c;
      if (d < 0) {
        c = mt[x] ? mix(lit(bs[x]), 0, 0.45f) : airCol;
      } else {
        int k = (int)(d * 0.42f); if (k > 63) k = 63;
        c = depthCol[k];
        if (rays && d < 120) {
          int si = (int)s + 180; si = si < 0 ? 0 : (si > 359 ? 359 : si);
          float r = rayTab[si] * (1 - d * (1.0f / 120)) * 0.22f * day;
          if (r > 0.01f) c = addRGB(c, 0xe8fbff, r);
          if (d < 24 && (hash2(x / 3, y / 2, tick4) & 15) == 0) c = addRGB(c, 0xd0f8ff, 0.10f * day * (1 - d / 24));
        }
        if (mt[x]) c = mix(lit(bs[x]), c, 0.18f + 0.25f * clampf(d / 150, 0, 1));
        if (d < 1.4f) c = lineCol;
      }
      o[x] = c;
    }
  }
  // Pflanzen
  for (int p = 0; p < nPlants; p++) {
    const Plant &P = plants[p];
    uint16_t c1 = C(scaleRGB(P.c1, lamp)), c2 = C(scaleRGB(P.c2, lamp));
    for (int k = 0; k < P.h; k++) {
      float v = (float)k / P.h;
      float sway = sinf(t_ * 0.9f + P.ph + v * 2.2f) * v * v * 6 + sinf(t_ * 0.37f + P.ph) * v * 2;
      int x = (int)(P.x + sway), y = P.y - k;
      if (depthAt(x, y) < -2) break;
      lp(x, y, c1); lp(x + 1, y, c1);
      if (k > 4 && k % 5 == 0) {
        int side = (k / 5) & 1 ? 1 : -1;
        for (int q = 1; q <= 4 - (int)(v * 2); q++) lp(x + side * q + (side > 0 ? 1 : 0), y - q / 2, c2);
      }
    }
  }
  // Blasen
  uint16_t bc = C(lerpRGB(0x6a8aa0, 0xdcf8ff, day));
  for (auto &b : bubbles) if (b.on) {
    int x = (int)b.x, y = (int)b.y;
    if (b.r > 1) { lpMix(x - 1, y, bc, 0.7f); lpMix(x + 1, y, bc, 0.7f); lpMix(x, y - 1, bc, 0.7f); lpMix(x, y + 1, bc, 0.7f); }
    else lpMix(x, y, bc, 0.8f);
  }
  // Futter
  for (int q = 0; q < 16; q++) if (flakes[q].on)
    lp((int)flakes[q].x, (int)flakes[q].y, C(scaleRGB(q & 1 ? 0xe0a858 : 0xc06838, lamp)));
  for (int k = 0; k < nAfish; k++) drawFish(afish[k], lamp);
  // Glas: Lichtreflex oben links
  for (float a = -2.55f; a < -1.85f; a += 0.006f) {
    float e = 1 - fabsf(a + 2.2f) / 0.35f;
    lpAdd((int)(CX + cosf(a) * 108), (int)(CY + sinf(a) * 108), 0xffffff, 0.22f * e);
    lpAdd((int)(CX + cosf(a) * 107), (int)(CY + sinf(a) * 107), 0xffffff, 0.12f * e);
  }
}
static void tapAquarium(int x, int y) {
  for (int k = 0; k < nAfish; k++) {
    AqFish &f = afish[k];
    const FishArt &A = FISH_ARTS[f.kind];
    if (fabsf(x - f.x) < A.w / 2 + 4 && fabsf(y - f.y) < A.h / 2 + 4) {   // Fisch erschrickt
      f.scared = 1.4f;
      for (int t = 0; t < 30; t++) { float tx = f.x + frand(-80, 80), ty = f.y + frand(-40, 40); if (goodSpot(tx, ty, 12)) { f.tx = tx; f.ty = ty; break; } }
      return;
    }
  }
  feed(x, y);
}

// ================================================================= MOND
// Mondphase nach Meeus (Astronomische Algorithmen, Kap. 48, verkuerzt): auf etwa
// 0,1 Prozent Beleuchtung genau. Die Rechnung laeuft alle paar Minuten.
static int dateY = 2026, dateM = 1, dateD = 1;
struct MoonState { float E, frac, toFull, toNew; };
static MoonState moon = {0, 0, 0, 0};
static float moonCalcAt = -1000;
static float moonInfoT = 0;
static float moonCloudX[2] = {-60, 120};
constexpr int MR = 70, MCX = 116, MCY = 108;

static double julianDay(int y, int m, int d, double hUT) {
  if (m <= 2) { y -= 1; m += 12; }
  int A = y / 100, B = 2 - A + A / 4;
  return floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1)) + d + B - 1524.5 + hUT / 24.0;
}
// Elongation des Mondes von der Sonne in Grad: 0 Neumond, 90 erstes Viertel, 180 Vollmond
static double moonElong(double jd) {
  const double R = 3.14159265358979 / 180, dd = jd - 2451545.0;
  double D = 297.8501921 + 12.190749114 * dd, M = 357.5291092 + 0.985600281 * dd, Mp = 134.9633964 + 13.064992951 * dd;
  D = fmod(D, 360.0); M = fmod(M, 360.0); Mp = fmod(Mp, 360.0);
  double E = D + 6.289 * sin(Mp * R) - 2.100 * sin(M * R) + 1.274 * sin((2 * D - Mp) * R) +
             0.658 * sin(2 * D * R) + 0.214 * sin(2 * Mp * R) + 0.110 * sin(D * R);
  E = fmod(E, 360.0);
  return E < 0 ? E + 360 : E;
}
// Ortszeit -> Weltzeit (Mitteleuropa, Sommerzeit ungefaehr; eine Stunde Fehler = 0,5 Grad)
static float tzOffset(int m, int d) {
  if (m >= 4 && m <= 9) return 2;
  if (m == 3 && d >= 25) return 2;
  if (m == 10 && d < 25) return 2;
  return 1;
}
static MoonState calcMoon(int y, int m, int d, float h) {
  double jd = julianDay(y, m, d, h - tzOffset(m, d));
  MoonState s;
  double E = moonElong(jd);
  s.E = (float)E;
  s.frac = (float)((1 - cos(E * 3.14159265358979 / 180)) / 2);
  s.toFull = -1; s.toNew = -1;
  double prev = E;
  const double step = 0.125;   // 3 Stunden, dazwischen linear
  for (int k = 1; k <= 31 * 8 && (s.toFull < 0 || s.toNew < 0); k++) {
    double e = moonElong(jd + k * step);
    if (s.toFull < 0 && prev < 180 && e >= 180) s.toFull = (float)((k - 1 + (180 - prev) / (e - prev)) * step);
    if (s.toNew < 0 && e < prev) s.toNew = (float)((k - 1 + (360 - prev) / (e + 360 - prev)) * step);
    prev = e;
  }
  return s;
}
static const char *moonPhaseName(float E) {
  if (E < 7 || E >= 353) return "NEUMOND";
  if (E < 83) return "ZUNEHMENDE SICHEL";
  if (E < 97) return "ERSTES VIERTEL";
  if (E < 173) return "ZUNEHMENDER MOND";
  if (E < 187) return "VOLLMOND";
  if (E < 263) return "ABNEHMENDER MOND";
  if (E < 277) return "LETZTES VIERTEL";
  return "ABNEHMENDE SICHEL";
}
// Mondscheibe: Albedo (Meere, Krater, Strahlen) einmal berechnen -> mat (Bytes);
// dazu zwei Wolkenformen. Das fertige Standbild (Himmel, Hof, beleuchtete Scheibe) liegt in base
// und wird nur neu gerechnet, wenn sich die Phase merklich aendert.
constexpr int MW = 2 * MR + 1, CLW = 104, CLH = 40;
static uint8_t *moonAlb() { return mat; }
static uint8_t *moonCloud(int c) { return mat + MW * MW + c * CLW * CLH; }
static float moonFrameE = -1000;
static void initMoon() {
  struct Mare { float x, y, rx, ry; };
  static const Mare MARIA[] = {   // grob wie auf der echten Vorderseite (Norden oben)
    {-0.55f, -0.02f, 0.26f, 0.45f}, {-0.27f, -0.42f, 0.26f, 0.19f}, {0.17f, -0.38f, 0.15f, 0.13f},
    {0.33f, -0.07f, 0.19f, 0.15f}, {0.69f, -0.30f, 0.10f, 0.08f}, {0.57f, 0.17f, 0.11f, 0.16f},
    {0.39f, 0.31f, 0.08f, 0.07f}, {-0.21f, 0.38f, 0.17f, 0.12f}, {-0.50f, 0.41f, 0.09f, 0.08f},
    {0.00f, -0.73f, 0.44f, 0.07f}, {0.04f, -0.14f, 0.09f, 0.07f}, {-0.05f, 0.12f, 0.08f, 0.08f}};
  struct Crater { float x, y, r, bright; };
  Crater cr[34]; int ncr = 0;
  cr[ncr++] = {-0.15f, 0.72f, 0.035f, 0.55f};   // Tycho
  cr[ncr++] = {-0.33f, -0.18f, 0.030f, 0.35f};  // Kopernikus
  cr[ncr++] = {-0.67f, -0.36f, 0.018f, 0.55f};  // Aristarch
  cr[ncr++] = {-0.58f, -0.12f, 0.016f, 0.30f};  // Kepler
  uint32_t r = 977;
  while (ncr < 34) {
    r = r * 1103515245u + 12345u; float x = ((r >> 8) & 1023) / 511.5f - 1;
    r = r * 1103515245u + 12345u; float y = ((r >> 8) & 1023) / 511.5f - 1;
    r = r * 1103515245u + 12345u; float rad = 0.02f + ((r >> 8) & 255) / 255.0f * 0.05f;
    if (x * x + y * y < 0.8f) cr[ncr++] = {x, y, rad, 0};
  }
  uint8_t *alb = moonAlb();
  for (int j = 0; j < MW; j++) for (int i = 0; i < MW; i++) {
    float nx = (i - MR) / (float)MR, ny = (j - MR) / (float)MR, r2 = nx * nx + ny * ny;
    if (r2 > 1.0f) { alb[j * MW + i] = 0; continue; }
    // Hochland: hell, fein gesprenkelt
    float a = 0.84f + 0.07f * (vnoise(nx * 7 + 11, ny * 7, 51) - 0.5f) * 2 + 0.05f * (vnoise(nx * 23, ny * 23, 52) - 0.5f) * 2;
    // Meere: weich auslaufend, Raender verwirbelt
    float wx = (vnoise(nx * 5, ny * 5, 54) - 0.5f) * 0.10f, wy = (vnoise(nx * 5 + 7, ny * 5, 55) - 0.5f) * 0.10f;
    float dark = 0;
    for (int k = 0; k < (int)(sizeof MARIA / sizeof MARIA[0]); k++) {
      const Mare &M = MARIA[k];
      float e = sq((nx + wx - M.x) / M.rx) + sq((ny + wy - M.y) / M.ry);
      dark = fmaxf(dark, smooth01(1.25f, 0.55f, e));
    }
    a *= 1 - 0.30f * dark * (0.85f + 0.3f * vnoise(nx * 11, ny * 11, 56));
    // Krater: kaum sichtbar, die hellen jungen Krater leuchten
    for (int k = 0; k < ncr; k++) {
      float d = sqrtf(sq(nx - cr[k].x) + sq(ny - cr[k].y)) / cr[k].r;
      if (cr[k].bright > 0) { if (d < 1.6f) a += cr[k].bright * smooth01(1.6f, 0.4f, d) * 0.35f; }
      else if (d < 0.85f) a *= 0.965f;
      else if (d < 1.15f) a *= 1.035f;
    }
    // Strahlen von Tycho und Kopernikus
    float tx = nx + 0.15f, ty = ny - 0.72f, td = sqrtf(tx * tx + ty * ty);
    if (td > 0.04f && td < 1.0f) {
      float ray = fabsf(sinf(atan2f(ty, tx) * 9.0f + 0.6f));
      if (ray > 0.96f) a += 0.05f * (1 - td);
    }
    float kx = nx + 0.33f, ky = ny + 0.18f, kd = sqrtf(kx * kx + ky * ky);
    if (kd > 0.03f && kd < 0.3f) { float ray = fabsf(sinf(atan2f(ky, kx) * 7.0f)); if (ray > 0.95f) a += 0.04f * (1 - kd / 0.3f); }
    alb[j * MW + i] = (uint8_t)(clampf(a, 0, 1) * 255);
  }
  for (int c = 0; c < 2; c++) {
    uint8_t *cl = moonCloud(c);
    for (int y = 0; y < CLH; y++) for (int x = 0; x < CLW; x++) {
      float u = x / (float)CLW, v = (y - CLH / 2) / (CLH / 2.0f);
      float shape = (1 - sq(u * 2 - 1)) * (1 - v * v);
      float n = vnoise(x / 14.0f + c * 7, y / 7.0f, 85 + c) * 0.7f + vnoise(x / 5.0f, y / 3.0f, 87 + c) * 0.3f;
      cl[y * CLW + x] = (uint8_t)(clampf((n * shape - 0.22f) * 2.2f, 0, 0.7f) * 255);
    }
  }
  moonCalcAt = -1000; moonFrameE = -1000; moonInfoT = 0;
}
static float hoursAbs() { return dateD * 24.0f + hours; }
// Standbild: Himmel, Hof und Scheibe fuer die aktuelle Phase -> base
static void buildMoonFrame() {
  float E = moon.E * 3.14159265f / 180, Lx = sinf(E), Lz = -cosf(E);
  for (int y = 0; y < L; y++) {
    uint32_t sky = lerpRGB(0x020308, 0x0a1026, y / 232.0f);
    for (int x = 0; x < L; x++) {
      float rr = sqrtf(sq(x - MCX) + sq(y - MCY));
      uint32_t c = sky;
      if (rr > MR && rr < MR + 60) {   // Hof, je voller desto heller
        float h = moon.frac * 0.26f * expf(-(rr - MR) / 14.0f);
        c = lerpRGB(c, 0x8a94b4, h);
      }
      base[y * L + x] = Cd(c, x, y);
    }
  }
  const uint8_t *alb = moonAlb();
  for (int j = 0; j < MW; j++) for (int i = 0; i < MW; i++) {
    uint8_t a8 = alb[j * MW + i];
    if (!a8) continue;
    float nx = (i - MR) / (float)MR, ny = (j - MR) / (float)MR;
    float nz = sqrtf(fmaxf(0, 1 - nx * nx - ny * ny));
    float ndl = nx * Lx + nz * Lz;
    float litK = smooth01(-0.02f, 0.09f, ndl);
    float a = a8 / 255.0f * (0.9f + 0.1f * nz);
    uint32_t c = scaleRGB(0xf6f1e4, a * litK);
    uint32_t earth = scaleRGB(0x4c566c, a * 0.07f * (1 - litK));
    c = ((c >> 16 & 255) + (earth >> 16 & 255)) << 16 | ((c >> 8 & 255) + (earth >> 8 & 255)) << 8 | ((c & 255) + (earth & 255));
    int X = MCX - MR + i, Y = MCY - MR + j;
    base[Y * L + X] = Cd(c, X, Y);
  }
  moonFrameE = moon.E;
}
static void updateMoonCalc() {
  float now = hoursAbs();
  if (fabsf(now - moonCalcAt) > 0.1f) { moon = calcMoon(dateY, dateM, dateD, hours); moonCalcAt = now; }
  if (fabsf(moon.E - moonFrameE) > 0.05f) buildMoonFrame();
}
static void renderMoon(float dt, bool nightlight) {
  updateMoonCalc();
  memcpy(lf, base, L * L * 2);
  float starK = nightlight ? 0.5f : 1.0f;
  for (int k = 0; k < 70; k++) {
    int x = hash2(k, 1, 81) % L, y = hash2(k, 2, 81) % L;
    if (sq(x - MCX) + sq(y - MCY) < (MR + 10) * (MR + 10)) continue;
    float tw = 0.6f + 0.4f * sinf(t_ * (1.5f + (k & 3) * 0.4f) + k);
    lpAdd(x, y, (k & 3) ? 0xdfe6ff : 0xfff0d0, (0.25f + 0.25f * (k % 3)) * tw * starK);
  }
  // Schleierwolken ziehen langsam vorbei
  uint16_t cc = C(lerpRGB(0x10141e, 0x6a7088, moon.frac * 0.8f));
  for (int c = 0; c < 2; c++) {
    moonCloudX[c] += dt * (1.6f + c * 0.7f);
    if (moonCloudX[c] > 300) moonCloudX[c] = -110;
    const uint8_t *cl = moonCloud(c);
    int ox = (int)moonCloudX[c], oy = (c ? 150 : 80) - CLH / 2;
    for (int y = 0; y < CLH; y++) for (int x = 0; x < CLW; x++) {
      uint8_t al = cl[y * CLW + x];
      if (al) lpMix(ox + x, oy + y, cc, al / 255.0f);
    }
  }
  if (moonInfoT > 0) moonInfoT -= dt;
}
static void tapMoon() { moonInfoT = 7; }
// Text unter dem Mond (in voller Aufloesung, nach dem Hochskalieren)
static void moonOverlay() {
  if (moonInfoT <= 0) return;
  float k = moonInfoT > 1 ? 1 : moonInfoT;
  uint16_t c1 = mix(0, C(0xfff4dc), k), c2 = mix(0, C(0xb9c8e8), k);
  textC(moonPhaseName(moon.E), 386, 2, c1);
  char buf[32];
  bool waxing = moon.E < 180;
  float days = waxing ? moon.toFull : moon.toNew;
  int n = (int)(days + 0.5f);
  const char *what = waxing ? "VOLLMOND" : "NEUMOND";
  if (moon.E >= 173 && moon.E < 187) snprintf(buf, sizeof buf, "%d%% BELEUCHTET", (int)(moon.frac * 100 + 0.5f));
  else if (n <= 0) snprintf(buf, sizeof buf, "HEUTE %s", what);
  else if (n == 1) snprintf(buf, sizeof buf, "MORGEN %s", what);
  else snprintf(buf, sizeof buf, "%s IN %d TAGEN", what, n);
  textC(buf, 412, 2, c2);
}
