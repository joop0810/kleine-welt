// PC-Test fuer den Kumpel: rendert alle Figuren in allen Stimmungen und jede
// Seitenart, spielt Wischen, Tippen, Regler und Zeitablauf durch und prueft die
// Aktionen, die an Home Assistant gehen wuerden.
//   g++ -O2 -std=c++17 -Ifirmware-src/Kumpel -I<ArduinoJson>/src host/kumpel_sim.cpp firmware-src/Kumpel/kumpel.cpp
//       firmware-src/Kumpel/gfx.cpp firmware-src/Kumpel/font_data.cpp firmware-src/Kumpel/config_json.cpp -o kumpel_sim
//   ./kumpel_sim out
#include "kumpel.h"
#include "config_json.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

using namespace kb;
static uint16_t fbuf[SCR * SCR];
static std::string outDir = "out/";
static uint32_t now = 1000;
static int fails = 0;

static void check(bool ok, const char *what) {
  printf("%s %s\n", ok ? "ok  " : "FEHLER", what);
  if (!ok) fails++;
}
static void save(const std::string &name) {
  std::string p = outDir + name + ".ppm";
  FILE *f = fopen(p.c_str(), "wb");
  fprintf(f, "P6 %d %d 255\n", SCR, SCR);
  for (int i = 0; i < SCR * SCR; i++) {
    uint16_t c = fbuf[i];
    unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)};
    int x = i % SCR, y = i / SCR;
    float dx = x - 232.5f, dy = y - 232.5f;
    if (dx * dx + dy * dy > 233 * 233) rgb[0] = rgb[1] = rgb[2] = 40;   // rundes Display
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}
static void run(float sec, const Touch *t = nullptr, int n = 0) {
  for (int f = 0; f < sec * 25; f++) { now += 40; tick(now, t, n); }
}
static void tap(int x, int y) {
  Touch t{(int16_t)x, (int16_t)y};
  now += 40; tick(now, &t, 1);
  now += 40; tick(now, &t, 1);
  now += 40; tick(now, nullptr, 0);
}
static void hold(int x, int y, float sec) { Touch t{(int16_t)x, (int16_t)y}; run(sec, &t, 1); now += 40; tick(now, nullptr, 0); }
static void drag(int x0, int y0, int x1, int y1) {
  for (int k = 0; k <= 10; k++) {
    Touch t{(int16_t)(x0 + (x1 - x0) * k / 10), (int16_t)(y0 + (y1 - y0) * k / 10)};
    now += 40; tick(now, &t, 1);
  }
  now += 40; tick(now, nullptr, 0);
}
static void swipeLeft() { drag(340, 233, 110, 240); run(0.5f); }
static void swipeRight() { drag(126, 233, 356, 240); run(0.5f); }

static Entity &ent(const char *id) { return *findEntity(id); }
static int drainActions(Action *out, int max) {
  int n = 0; Action a;
  while (takeAction(a)) if (n < max) out[n++] = a;
  return n;
}

int main(int argc, char **argv) {
  if (argc > 1) outDir = std::string(argv[1]) + "/";
  setvbuf(stdout, nullptr, _IONBF, 0);
  begin(fbuf, 7);

  Config c;
  defaultConfig(c);
  c.nPages = 6;
  c.pages[0] = {P_CLIMATE, "climate.wohnzimmer", ""};
  c.pages[1] = {P_LIGHT, "light.led_streifen", "LED-Licht"};
  c.pages[2] = {P_SWITCH, "switch.kaffeemaschine", ""};
  c.pages[3] = {P_SENSOR, "sensor.aussen_temperatur", ""};
  c.pages[4] = {P_BUTTON, "scene.filmabend", ""};
  c.pages[5] = {P_VACUUM, "vacuum.s7_max_ultra", ""};
  strcpy(c.coldEntity, "sensor.aussen_temperatur");
  c.coldBelow = 5;
  c.nHints = 2;
  c.hints[0] = {"sensor.abfall_morgen", C_EQ, "Papier", "Morgen kommt Papier!"};
  c.hints[1] = {"sensor.abfall_morgen", C_CONTAINS, "bio", "Biotonne rausstellen"};
  setConfig(c);
  check(entityCount() == 7, "7 Entities beobachtet (6 Seiten + Hinweis, Kaeltesensor doppelt)");

  // Zustaende wie von Home Assistant
  Entity &k = ent("climate.wohnzimmer");
  k.known = true; strcpy(k.state, "heat"); strcpy(k.name, "Wohnzimmer"); strcpy(k.action, "heating");
  k.cur = 20.3f; k.target = 21.5f; k.tmin = 7; k.tmax = 30; k.step = 0.5f;
  Entity &l = ent("light.led_streifen");
  l.known = true; strcpy(l.state, "on"); strcpy(l.name, "LED Streifen"); l.brightness = 180;
  l.hasBright = l.hasRgb = l.hasKelvin = true; l.rgb[0] = 255; l.rgb[1] = 140; l.rgb[2] = 60;
  Entity &s = ent("switch.kaffeemaschine");
  s.known = true; strcpy(s.state, "off"); strcpy(s.name, "Kaffeemaschine");
  Entity &t = ent("sensor.aussen_temperatur");
  t.known = true; strcpy(t.state, "12.4"); strcpy(t.name, "Au\xC3\x9F" "entemperatur"); strcpy(t.unit, "\xC2\xB0" "C");
  Entity &b = ent("scene.filmabend");
  b.known = true; strcpy(b.state, "2026-10-16T19:00:00"); strcpy(b.name, "Filmabend");
  Entity &v = ent("vacuum.s7_max_ultra");
  v.known = true; strcpy(v.state, "docked"); strcpy(v.name, "Roborock"); v.battery = 100;
  Entity &h = ent("sensor.abfall_morgen");
  h.known = true; strcpy(h.state, "keine");
  setConn(CN_OK);
  testSetHours(14.5f);

  // --- Figuren x Stimmungen
  for (int sk = 0; sk < SKINS; sk++) {
    c.skin = sk; setConfig(c);
    run(1.0f); save(std::string("figur") + std::to_string(sk) + "_neutral");
    tap(233, 240); run(0.3f);
    check(debug().mood == M_HAPPY, "Antippen macht froehlich");
    save(std::string("figur") + std::to_string(sk) + "_froehlich");
    run(3);
    shake(); run(0.3f);
    check(debug().mood == M_SURPRISED, "Schuetteln macht ueberrascht");
    save(std::string("figur") + std::to_string(sk) + "_ueberrascht");
    run(3);
    strcpy(t.state, "2.0"); run(0.5f);
    check(debug().mood == M_COLD, "unter 5 Grad friert er");
    save(std::string("figur") + std::to_string(sk) + "_friert");
    strcpy(t.state, "12.4");
    now += 30000; testSetHours(23.7f); run(0.6f);
    check(debug().mood == M_SLEEPY, "nachts schlaeft er");
    check(wantBrightness() == c.brightNight, "nachts gedimmt");
    save(std::string("figur") + std::to_string(sk) + "_muede");
    testSetHours(14.5f); run(0.2f);
  }
  c.skin = SK_EYES; setConfig(c);

  // --- Hinweis
  strcpy(h.state, "Papier"); run(1);
  check(debug().bubble, "Sprechblase bei Papier");
  save("hinweis");
  strcpy(h.state, "Biotonne"); run(0.2f);
  check(debug().bubble, "Sprechblase bei 'enthaelt bio'");
  strcpy(h.state, "keine"); run(0.2f);
  check(!debug().bubble, "keine Sprechblase sonst");

  // --- lange druecken = naechste Figur
  hold(233, 240, 1.0f);
  uint8_t sk;
  check(takeSkinChange(sk) && sk == SK_DROP, "lange druecken wechselt zu Troepfchen");
  c.skin = SK_EYES; setConfig(c);

  // --- Seiten durchwischen und rendern
  const char *names[] = {"klima", "licht", "schalter", "anzeige", "szene", "sauger"};
  for (int i = 0; i < 6; i++) {
    swipeLeft();
    check(debug().page == i + 1, (std::string("Wischen zu Seite ") + names[i]).c_str());
    save(std::string("seite_") + names[i]);
  }
  swipeLeft();
  check(debug().page == 6, "am Ende bleibt es stehen");
  // Uebergang mittendrin speichern
  drag(126, 233, 356, 240); now += 100; tick(now, nullptr, 0); save("wischen");
  run(0.5f);
  check(debug().page == 5, "zurueckwischen");

  Action acts[16];
  // --- Sauger: Start
  while (debug().page != 6) swipeLeft();
  drainActions(acts, 16);
  tap(141, 290);
  int na = drainActions(acts, 16);
  check(na == 1 && !strcmp(acts[0].service, "start") && !strcmp(acts[0].domain, "vacuum"), "Sauger starten");

  // --- Klima: + + und Regler
  while (debug().page != 1) swipeRight();
  drainActions(acts, 16);
  tap(233 + 82, 322); tap(233 + 82, 322); run(0.2f);
  check(fabsf(debug().shownTarget - 22.5f) < 0.01f, "zweimal + zeigt 22,5");
  check(drainActions(acts, 16) == 0, "noch nichts gesendet (sammelt)");
  run(1.2f);
  na = drainActions(acts, 16);
  check(na == 1 && !strcmp(acts[0].service, "set_temperature") && strstr(acts[0].data, "22.5"), "Solltemperatur 22.5 gesendet");
  // Regler ganz nach rechts oben ziehen (Winkel 0 Grad = rechts)
  drag(233 + 178, 233, 233 + 125, 233 + 125); run(0.4f);
  na = drainActions(acts, 16);
  check(na == 1 && strstr(acts[0].data, "30.0"), "Regler bis zum Ende = 30 Grad");
  k.target = 21.5f; save("seite_klima_nach");
  tap(233, 326);
  na = drainActions(acts, 16);
  check(na == 1 && !strcmp(acts[0].service, "turn_off"), "Klima ausschalten");
  run(0.2f); save("seite_klima_aus");

  // --- Licht: an/aus, Helligkeit, Farbe
  swipeLeft();
  drainActions(acts, 16);
  tap(233, 214);
  na = drainActions(acts, 16);
  check(na == 1 && !strcmp(acts[0].service, "toggle") && !strcmp(l.state, "off"), "Licht umschalten (sofort aus angezeigt)");
  run(0.2f); save("seite_licht_aus");
  drag(233 - 125, 233 + 125, 233, 233 - 178); run(0.2f);
  na = drainActions(acts, 16);
  check(na == 1 && strstr(acts[0].data, "brightness"), "Helligkeit per Regler");
  tap(233 - 80, 338);
  na = drainActions(acts, 16);
  check(na == 1 && strstr(acts[0].data, "color_temp_kelvin\":2700"), "Farbe Warmweiss");

  // --- Szene
  while (debug().page != 5) swipeLeft();
  drainActions(acts, 16);
  tap(233, 222); run(0.4f); save("seite_szene_erledigt");
  na = drainActions(acts, 16);
  check(na == 1 && !strcmp(acts[0].domain, "scene") && !strcmp(acts[0].service, "turn_on"), "Szene ausloesen");

  // --- automatisch zurueck zum Kumpel
  run(c.homeAfter + 2);
  check(debug().page == 0, "nach Ruhezeit zurueck zum Kumpel");

  // --- Einstellungen als JSON hin und zurueck
  {
    static char js[4096];
    size_t n = configToJson(c, js, sizeof js);
    Config r;
    bool ok = n > 0 && configFromJson(js, n, r);
    check(ok && r.nPages == 6 && r.pages[1].type == P_LIGHT && !strcmp(r.pages[1].title, "LED-Licht") &&
          !strcmp(r.pages[5].entity, "vacuum.s7_max_ultra"), "JSON: Seiten bleiben erhalten");
    check(ok && r.nHints == 2 && r.hints[1].cond == C_CONTAINS && !strcmp(r.hints[0].text, "Morgen kommt Papier!"), "JSON: Hinweise bleiben erhalten");
    check(ok && r.sleepFrom == 23 * 60 && r.sleepTo == 7 * 60 && fabsf(r.coldBelow - 5) < 0.01f, "JSON: Schlafenszeit und Kaelte");
    const char *bad = "{\"pages\":[{\"type\":\"quatsch\",\"entity\":\"x.y\"},{\"type\":\"light\",\"entity\":\"\"}],\"skin\":\"pebble\"}";
    check(configFromJson(bad, strlen(bad), r) && r.nPages == 0 && r.skin == SK_PEBBLE, "JSON: unbrauchbare Seiten werden verworfen");
    check(!configFromJson("{kaputt", 7, r), "JSON: kaputte Daten abgelehnt");
  }

  // --- Einrichtungsbildschirm
  setConn(CN_AP, "WLAN verbinden:", "Kumpel-Setup", "dann 192.168.4.1 \xC3\xB6" "ffnen");
  run(0.5f); save("einrichten");
  setConn(CN_HA); run(0.2f); save("verbinde");

  printf(fails ? "FEHLER: %d\n" : "ALLES OK\n", fails);
  return fails ? 1 : 0;
}
