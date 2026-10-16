#include "ha.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

namespace ha {

// ArduinoJson soll grosse Dokumente in den PSRAM legen
struct PsramAlloc : ArduinoJson::Allocator {
  void *allocate(size_t n) override { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
  void deallocate(void *p) override { heap_caps_free(p); }
  void *reallocate(void *p, size_t n) override { return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
  static PsramAlloc *instance() { static PsramAlloc a; return &a; }
};

static const size_t MAX_MSG = 1024 * 1024;   // groesste angenommene Nachricht

struct Server {
  bool tls = false;
  String host;
  uint16_t port = 8123;
  String base;   // "http(s)://host:port"
};
static Server srv;
static String token;
static State st = OFF;

static WiFiClient plain;
static WiFiClientSecure secure;
static Client *cli = nullptr;
static bool wsOpen = false;
static uint32_t nextTry = 0, backoff = 2000, lastRx = 0, lastPing = 0;
static int msgId = 1, subId = 0;
static bool wantResub = false;
static uint8_t *rxBuf = nullptr;
static size_t rxLen = 0;

static bool parseUrl(const char *url, Server &s) {
  String u = url;
  u.trim();
  while (u.endsWith("/")) u.remove(u.length() - 1);
  s.tls = false;
  if (u.startsWith("https://")) { s.tls = true; u = u.substring(8); }
  else if (u.startsWith("http://")) u = u.substring(7);
  int slash = u.indexOf('/');
  if (slash >= 0) u = u.substring(0, slash);   // Pfade werden nicht unterstuetzt
  int colon = u.lastIndexOf(':');
  s.port = s.tls ? 443 : 8123;
  if (colon > 0) { s.port = u.substring(colon + 1).toInt(); u = u.substring(0, colon); }
  s.host = u;
  if (!s.host.length() || !s.port) return false;
  s.base = String(s.tls ? "https://" : "http://") + s.host + ":" + s.port;
  return true;
}

State state() { return st; }

static void closeWs() {
  if (cli) cli->stop();
  wsOpen = false;
  subId = 0;
  if (st == READY || st == CONNECTING) st = token.length() && srv.host.length() ? CONNECTING : OFF;
}

void begin() {
  secure.setInsecure();   // Heim-Server haben selten ein pruefbares Zertifikat
  secure.setHandshakeTimeout(8);
}

void setServer(const char *url, const char *tok) {
  closeWs();
  token = tok ? tok : "";
  if (!url || !url[0] || !parseUrl(url, srv) || !token.length()) { st = OFF; srv.host = ""; return; }
  st = CONNECTING;
  nextTry = 0;
  backoff = 2000;
}

void resubscribe() { if (wsOpen) wantResub = true; }

// ------------------------------------------------------------------ WebSocket
static void b64(const uint8_t *in, int n, char *out) {
  static const char *T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int o = 0;
  for (int i = 0; i < n; i += 3) {
    uint32_t v = in[i] << 16 | (i + 1 < n ? in[i + 1] << 8 : 0) | (i + 2 < n ? in[i + 2] : 0);
    out[o++] = T[(v >> 18) & 63];
    out[o++] = T[(v >> 12) & 63];
    out[o++] = i + 1 < n ? T[(v >> 6) & 63] : '=';
    out[o++] = i + 2 < n ? T[v & 63] : '=';
  }
  out[o] = 0;
}

static bool sendFrame(uint8_t opcode, const uint8_t *data, size_t len) {
  if (!cli || !cli->connected()) return false;
  uint8_t hdr[14];
  int h = 0;
  hdr[h++] = 0x80 | opcode;
  if (len < 126) hdr[h++] = 0x80 | len;
  else if (len < 65536) { hdr[h++] = 0x80 | 126; hdr[h++] = len >> 8; hdr[h++] = len & 255; }
  else { hdr[h++] = 0x80 | 127; for (int i = 7; i >= 0; i--) hdr[h++] = (i < 4) ? (len >> (i * 8)) & 255 : 0; }
  uint8_t mask[4];
  uint32_t r = esp_random();
  memcpy(mask, &r, 4);
  memcpy(hdr + h, mask, 4);
  h += 4;
  if (cli->write(hdr, h) != (size_t)h) return false;
  uint8_t chunk[256];
  for (size_t off = 0; off < len; off += sizeof chunk) {
    size_t n = len - off < sizeof chunk ? len - off : sizeof chunk;
    for (size_t i = 0; i < n; i++) chunk[i] = data[off + i] ^ mask[(off + i) & 3];
    if (cli->write(chunk, n) != n) return false;
  }
  return true;
}
static bool sendText(const String &s) { return sendFrame(1, (const uint8_t *)s.c_str(), s.length()); }

static bool readExact(uint8_t *dst, size_t n, uint32_t timeoutMs = 3000) {
  uint32_t t0 = millis();
  size_t got = 0;
  while (got < n) {
    int a = cli->available();
    if (a > 0) {
      int r = cli->read(dst + got, n - got < (size_t)a ? n - got : a);
      if (r > 0) { got += r; t0 = millis(); continue; }
    }
    if (!cli->connected() || millis() - t0 > timeoutMs) return false;
    delay(1);
  }
  return true;
}

static bool openWs() {
  cli = srv.tls ? (Client *)&secure : (Client *)&plain;
  if (!srv.tls) plain.setTimeout(5);
  if (!cli->connect(srv.host.c_str(), srv.port)) { Serial.printf("HA: keine Verbindung zu %s:%u\n", srv.host.c_str(), srv.port); return false; }
  uint8_t key[16];
  for (int i = 0; i < 16; i += 4) { uint32_t r = esp_random(); memcpy(key + i, &r, 4); }
  char k64[28];
  b64(key, 16, k64);
  String req = String("GET /api/websocket HTTP/1.1\r\nHost: ") + srv.host + ":" + srv.port +
               "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + k64 +
               "\r\nSec-WebSocket-Version: 13\r\nUser-Agent: Kumpel\r\n\r\n";
  cli->print(req);
  // Antwort-Kopf lesen
  String line;
  bool first = true, ok = false;
  uint32_t t0 = millis();
  while (millis() - t0 < 5000) {
    if (!cli->available()) { if (!cli->connected()) break; delay(2); continue; }
    char c = cli->read();
    if (c == '\r') continue;
    if (c != '\n') { if (line.length() < 200) line += c; continue; }
    if (first) { ok = line.indexOf(" 101") > 0; first = false; }
    else if (!line.length()) break;
    line = "";
  }
  if (!ok) { Serial.println("HA: WebSocket-Handshake abgelehnt"); cli->stop(); return false; }
  wsOpen = true;
  lastRx = lastPing = millis();
  rxLen = 0;
  return true;
}

// ------------------------------------------------------------------ Zustaende
static void applyAttrs(kb::Entity &e, JsonObjectConst a) {
  if (a["friendly_name"].is<const char *>()) snprintf(e.name, sizeof e.name, "%s", a["friendly_name"].as<const char *>());
  if (a["unit_of_measurement"].is<const char *>()) snprintf(e.unit, sizeof e.unit, "%s", a["unit_of_measurement"].as<const char *>());
  if (!a["current_temperature"].isNull()) e.cur = a["current_temperature"].as<float>();
  if (a["temperature"].is<float>()) e.target = a["temperature"].as<float>();
  if (a["min_temp"].is<float>()) e.tmin = a["min_temp"].as<float>();
  if (a["max_temp"].is<float>()) e.tmax = a["max_temp"].as<float>();
  if (a["target_temp_step"].is<float>()) e.step = a["target_temp_step"].as<float>();
  if (a["hvac_action"].is<const char *>()) snprintf(e.action, sizeof e.action, "%s", a["hvac_action"].as<const char *>());
  if (!a["brightness"].isUnbound()) e.brightness = a["brightness"].is<int>() ? a["brightness"].as<int>() : -1;
  if (a["rgb_color"].is<JsonArrayConst>()) {
    JsonArrayConst c = a["rgb_color"];
    e.rgb[0] = c[0]; e.rgb[1] = c[1]; e.rgb[2] = c[2];
    e.hasRgb = true;
  }
  if (a["supported_color_modes"].is<JsonArrayConst>()) {
    e.hasRgb = e.hasKelvin = e.hasBright = false;
    for (const char *m : a["supported_color_modes"].as<JsonArrayConst>()) {
      if (!m) continue;
      if (!strcmp(m, "color_temp")) e.hasKelvin = true;
      if (!strcmp(m, "rgb") || !strcmp(m, "rgbw") || !strcmp(m, "rgbww") || !strcmp(m, "hs") || !strcmp(m, "xy")) e.hasRgb = true;
      if (strcmp(m, "onoff")) e.hasBright = true;
    }
  }
  if (!a["battery_level"].isNull()) e.battery = a["battery_level"].as<int>();
}

static void resetEntity(kb::Entity &e) {
  char id[kb::ID_LEN];
  memcpy(id, e.id, sizeof id);
  memset(&e, 0, sizeof e);
  memcpy(e.id, id, sizeof id);
  e.cur = e.target = NAN;
  e.tmin = 7; e.tmax = 30; e.step = 0.5f;
  e.brightness = -1;
  e.battery = -1;
}

static bool firstEvent = true;

static void handleEvent(JsonObjectConst ev) {
  if (ev["a"].is<JsonObjectConst>()) {
    for (JsonPairConst p : ev["a"].as<JsonObjectConst>()) {
      kb::Entity *e = kb::findEntity(p.key().c_str());
      if (!e) continue;
      resetEntity(*e);
      e->known = true;
      snprintf(e->state, sizeof e->state, "%s", p.value()["s"] | "");
      applyAttrs(*e, p.value()["a"]);
    }
  }
  if (ev["c"].is<JsonObjectConst>()) {
    for (JsonPairConst p : ev["c"].as<JsonObjectConst>()) {
      kb::Entity *e = kb::findEntity(p.key().c_str());
      if (!e) continue;
      JsonObjectConst plus = p.value()["+"];
      if (plus["s"].is<const char *>()) snprintf(e->state, sizeof e->state, "%s", plus["s"].as<const char *>());
      applyAttrs(*e, plus["a"]);
      for (const char *k : p.value()["-"]["a"].as<JsonArrayConst>()) {
        if (!k) continue;
        if (!strcmp(k, "brightness")) e->brightness = -1;
        if (!strcmp(k, "hvac_action")) e->action[0] = 0;
        if (!strcmp(k, "current_temperature")) e->cur = NAN;
        if (!strcmp(k, "temperature")) e->target = NAN;
      }
      e->known = true;
    }
  }
  if (ev["r"].is<JsonArrayConst>()) {
    for (const char *id : ev["r"].as<JsonArrayConst>()) {
      kb::Entity *e = id ? kb::findEntity(id) : nullptr;
      if (e) { e->known = true; strcpy(e->state, "unavailable"); }
    }
  }
  if (firstEvent) {
    // Was in der ersten Meldung fehlt, gibt es in Home Assistant nicht
    firstEvent = false;
    for (int i = 0; i < kb::entityCount(); i++) {
      kb::Entity *e = kb::entityAt(i);
      if (!e->known) { e->known = true; strcpy(e->state, "unavailable"); Serial.printf("HA: %s nicht gefunden\n", e->id); }
    }
  }
}

static void subscribe() {
  if (kb::entityCount() == 0) { subId = 0; return; }
  JsonDocument d;
  subId = msgId++;
  d["id"] = subId;
  d["type"] = "subscribe_entities";
  JsonArray ids = d["entity_ids"].to<JsonArray>();
  for (int i = 0; i < kb::entityCount(); i++) ids.add(kb::entityAt(i)->id);
  String s;
  serializeJson(d, s);
  firstEvent = true;
  sendText(s);
}

static void onMessage(const char *msg, size_t len) {
  // Nur die Felder behalten, die der Kumpel braucht (Lichter haben z. B. riesige effect_lists)
  static JsonDocument filter;
  static bool fInit = false;
  if (!fInit) {
    fInit = true;
    JsonDocument attrs;
    for (const char *k : {"friendly_name", "unit_of_measurement", "current_temperature", "temperature", "min_temp", "max_temp",
                          "target_temp_step", "hvac_action", "brightness", "rgb_color", "supported_color_modes", "battery_level"})
      attrs[k] = true;
    filter["id"] = true;
    filter["type"] = true;
    filter["success"] = true;
    filter["error"]["message"] = true;
    filter["event"]["a"]["*"]["s"] = true;
    filter["event"]["a"]["*"]["a"] = attrs;
    filter["event"]["c"]["*"]["+"]["s"] = true;
    filter["event"]["c"]["*"]["+"]["a"] = attrs;
    filter["event"]["c"]["*"]["-"]["a"] = true;
    filter["event"]["r"] = true;
  }
  JsonDocument doc(PsramAlloc::instance());
  DeserializationError err = deserializeJson(doc, msg, len, DeserializationOption::Filter(filter));
  if (err) { Serial.printf("HA: JSON-Fehler %s (%u Byte)\n", err.c_str(), (unsigned)len); return; }
  const char *type = doc["type"] | "";
  if (!strcmp(type, "auth_required")) {
    JsonDocument a;
    a["type"] = "auth";
    a["access_token"] = token;
    String s;
    serializeJson(a, s);
    sendText(s);
  } else if (!strcmp(type, "auth_ok")) {
    Serial.println("HA: angemeldet");
    st = READY;
    backoff = 2000;
    subscribe();
  } else if (!strcmp(type, "auth_invalid")) {
    Serial.println("HA: Token ungueltig");
    st = AUTH_FAILED;
    closeWs();
    st = AUTH_FAILED;
    nextTry = millis() + 300000;   // in 5 Minuten nochmal (oder nach neuer Einstellung)
  } else if (!strcmp(type, "event") && doc["id"] == subId) {
    handleEvent(doc["event"]);
  } else if (!strcmp(type, "result") && !(doc["success"] | true)) {
    Serial.printf("HA: Fehler bei #%d: %s\n", doc["id"].as<int>(), doc["error"]["message"] | "?");
  }
}

// Liest alles, was anliegt. false = Verbindung weg.
static bool pump() {
  while (cli->available() >= 2) {
    uint8_t h[2];
    if (!readExact(h, 2)) return false;
    bool fin = h[0] & 0x80;
    uint8_t op = h[0] & 0x0F;
    uint64_t len = h[1] & 0x7F;
    if (len == 126) { uint8_t e[2]; if (!readExact(e, 2)) return false; len = (e[0] << 8) | e[1]; }
    else if (len == 127) { uint8_t e[8]; if (!readExact(e, 8)) return false; len = 0; for (int i = 0; i < 8; i++) len = (len << 8) | e[i]; }
    uint8_t mask[4] = {0, 0, 0, 0};
    bool masked = h[1] & 0x80;
    if (masked && !readExact(mask, 4)) return false;
    if (op >= 8) {   // Steuerframes: ping/pong/close
      uint8_t p[126];
      if (len > 125 || !readExact(p, len)) return false;
      if (op == 9) sendFrame(10, p, len);
      if (op == 8) return false;
      lastRx = millis();
      continue;
    }
    if (op != 0) rxLen = 0;   // neue Nachricht (Text/Binaer)
    if (rxLen + len > MAX_MSG) { Serial.println("HA: Nachricht zu gross"); return false; }
    uint8_t *nb = (uint8_t *)heap_caps_realloc(rxBuf, rxLen + len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!nb) return false;
    rxBuf = nb;
    if (!readExact(rxBuf + rxLen, len, 6000)) return false;
    if (masked) for (uint64_t i = 0; i < len; i++) rxBuf[rxLen + i] ^= mask[i & 3];
    rxLen += len;
    lastRx = millis();
    if (fin) {
      rxBuf[rxLen] = 0;
      onMessage((const char *)rxBuf, rxLen);
      rxLen = 0;
      if (!wsOpen) return false;
    }
  }
  return cli->connected();
}

void loop() {
  if (st == OFF || WiFi.status() != WL_CONNECTED) { if (wsOpen) closeWs(); return; }
  uint32_t now = millis();
  if (!wsOpen) {
    if (now < nextTry) return;
    if (st == AUTH_FAILED) st = CONNECTING;
    if (!openWs()) {
      nextTry = millis() + backoff;
      backoff = backoff < 30000 ? backoff * 2 : 30000;
    }
    return;
  }
  if (!pump()) {
    Serial.println("HA: Verbindung getrennt");
    State keep = st;
    closeWs();
    if (keep == AUTH_FAILED) st = AUTH_FAILED;
    nextTry = millis() + backoff;
    return;
  }
  if (st == READY && wantResub) {
    wantResub = false;
    // einfachster Weg: alte Verbindung schliessen, neu anmelden und abonnieren
    closeWs();
    nextTry = 0;
    return;
  }
  if (st == READY && now - lastPing > 30000) {
    lastPing = now;
    sendText(String("{\"id\":") + msgId++ + ",\"type\":\"ping\"}");
  }
  if (now - lastRx > 75000) {
    Serial.println("HA: keine Antwort mehr - neu verbinden");
    closeWs();
    nextTry = 0;
  }
}

bool call(const kb::Action &a) {
  if (st != READY) return false;
  char buf[320];
  snprintf(buf, sizeof buf,
           "{\"id\":%d,\"type\":\"call_service\",\"domain\":\"%s\",\"service\":\"%s\",\"target\":{\"entity_id\":\"%s\"},\"service_data\":{%s}}",
           msgId++, a.domain, a.service, a.entity, a.data);
  Serial.printf("HA: %s.%s %s {%s}\n", a.domain, a.service, a.entity, a.data);
  return sendFrame(1, (const uint8_t *)buf, strlen(buf));
}

// ------------------------------------------------------------------ REST
static bool restGet(const Server &s, const char *tok, const char *path, HTTPClient &http, WiFiClient &plainC,
                    WiFiClientSecure &secC, int &code) {
  String url = s.base + path;
  http.useHTTP10(true);   // kein chunked - dann kann direkt aus dem Strom gelesen werden
  http.setTimeout(8000);
  bool ok = s.tls ? http.begin(secC, url) : http.begin(plainC, url);
  if (!ok) return false;
  http.addHeader("Authorization", String("Bearer ") + tok);
  code = http.GET();
  return true;
}

bool test(const char *url, const char *tok, String &msg) {
  Server s;
  if (!parseUrl(url, s)) { msg = "Adresse ungültig"; return false; }
  if (!tok || !tok[0]) { msg = "Token fehlt"; return false; }
  WiFiClient pc;
  WiFiClientSecure sc;
  sc.setInsecure();
  HTTPClient http;
  int code = 0;
  if (!restGet(s, tok, "/api/", http, pc, sc, code)) { msg = "Adresse ungültig"; return false; }
  bool ok = code == 200;
  if (ok) msg = "Verbunden";
  else if (code == 401) msg = "Token wird abgelehnt";
  else if (code < 0) msg = String("Nicht erreichbar (") + http.errorToString(code) + ")";
  else msg = String("Antwort ") + code;
  http.end();
  return ok;
}

static char *listBuf = nullptr;

const char *fetchEntities(size_t &len, String &err) {
  len = 0;
  if (!srv.host.length() || !token.length()) { err = "Home Assistant ist noch nicht eingerichtet"; return nullptr; }
  WiFiClient pc;
  WiFiClientSecure sc;
  sc.setInsecure();
  HTTPClient http;
  int code = 0;
  if (!restGet(srv, token.c_str(), "/api/states", http, pc, sc, code) || code != 200) {
    err = code == 401 ? "Token wird abgelehnt" : String("Home Assistant antwortet nicht (") + code + ")";
    http.end();
    return nullptr;
  }
  JsonDocument filter;
  filter[0]["entity_id"] = true;
  filter[0]["state"] = true;
  filter[0]["attributes"]["friendly_name"] = true;
  filter[0]["attributes"]["unit_of_measurement"] = true;
  JsonDocument doc(PsramAlloc::instance());
  DeserializationError e = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (e) { err = String("Antwort nicht lesbar: ") + e.c_str(); return nullptr; }
  JsonDocument out(PsramAlloc::instance());
  JsonArray arr = out.to<JsonArray>();
  for (JsonObjectConst o : doc.as<JsonArrayConst>()) {
    JsonObject x = arr.add<JsonObject>();
    x["i"] = o["entity_id"];
    x["n"] = o["attributes"]["friendly_name"] | "";
    if (o["attributes"]["unit_of_measurement"].is<const char *>()) x["u"] = o["attributes"]["unit_of_measurement"];
    const char *s = o["state"] | "";
    char st[33];
    snprintf(st, sizeof st, "%s", s);   // lange Zustaende kuerzen
    x["s"] = st;
  }
  size_t need = measureJson(out);
  char *nb = (char *)heap_caps_realloc(listBuf, need + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!nb) { err = "Zu wenig Speicher"; return nullptr; }
  listBuf = nb;
  len = serializeJson(out, listBuf, need + 1);
  return listBuf;
}

}  // namespace ha
