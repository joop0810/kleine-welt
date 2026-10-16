#include "net.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <time.h>

namespace net {

const char *HOSTNAME = "kumpel";
const char *AP_NAME = "Kumpel-Setup";
static const char *DEFAULT_TZ = "CET-1CEST,M3.5.0,M10.5.0/3";

static Settings S;
static Preferences prefs;
static Mode md = NO_WIFI;
static bool ap = false, mdnsOn = false, ntpOn = false;
static uint32_t connectStart = 0, onlineSince = 0;
static DNSServer dns;
static const char *fw = "";

Settings &settings() { return S; }

void load() {
  prefs.begin("kumpel", true);
  S.ssid = prefs.getString("ssid", "");
  S.pass = prefs.getString("pass", "");
  S.haUrl = prefs.getString("haurl", "");
  S.haToken = prefs.getString("token", "");
  S.tz = prefs.getString("tz", DEFAULT_TZ);
  S.config = prefs.getString("cfg", "");
  prefs.end();
}
static void put(const char *k, const String &v) {
  prefs.begin("kumpel", false);
  prefs.putString(k, v);
  prefs.end();
}
void saveWifi(const String &s, const String &p) { S.ssid = s; S.pass = p; put("ssid", s); put("pass", p); }
void saveHa(const String &u, const String &t) { S.haUrl = u; S.haToken = t; put("haurl", u); put("token", t); }
void saveTz(const String &tz) {
  S.tz = tz.length() ? tz : String(DEFAULT_TZ);
  put("tz", S.tz);
  if (ntpOn) configTzTime(S.tz.c_str(), "pool.ntp.org", "time.google.com");
}
void saveConfig(const String &json) { S.config = json; put("cfg", json); }

Mode mode() { return md; }
bool apActive() { return ap; }
String ip() { return md == ONLINE ? WiFi.localIP().toString() : (ap ? WiFi.softAPIP().toString() : String("")); }
bool timeValid() { return ntpOn && time(nullptr) > 1735689600; }   // nach 1.1.2025

static void startAp() {
  if (ap) return;
  WiFi.mode(WIFI_AP_STA);   // STA bleibt an: WLAN-Suche und Neuversuche laufen weiter
  WiFi.softAP(AP_NAME);
  dns.start(53, "*", WiFi.softAPIP());   // jede Adresse -> Einstellungsseite
  ap = true;
  if (md != ONLINE) md = SETUP_AP;
  Serial.printf("Einrichtungs-WLAN \"%s\" aktiv, %s\n", AP_NAME, WiFi.softAPIP().toString().c_str());
}
static void stopAp() {
  if (!ap) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  ap = false;
}

void reconnect() {
  WiFi.disconnect(false, false);
  if (!S.ssid.length()) { md = NO_WIFI; startAp(); return; }
  if (!ap) WiFi.mode(WIFI_STA);
  WiFi.begin(S.ssid.c_str(), S.pass.c_str());
  md = CONNECTING;
  connectStart = millis();
}

// ------------------------------------------------------------------ Improv
// Protokoll: https://www.improv-wifi.com/serial/ - der Web-Installer fragt damit
// direkt nach dem Flashen die WLAN-Daten ab und zeigt danach "Zum Geraet".
enum { IMP_CURRENT_STATE = 1, IMP_ERROR = 2, IMP_RPC = 3, IMP_RPC_RESULT = 4 };
enum { IMP_READY = 2, IMP_PROVISIONING = 3, IMP_PROVISIONED = 4 };
enum { IMP_ERR_NONE = 0, IMP_ERR_INVALID = 1, IMP_ERR_UNKNOWN_CMD = 2, IMP_ERR_CONNECT = 3 };
static uint8_t ib[256];
static int ipos = 0;
static bool impWaiting = false;
static uint32_t impDeadline = 0;

static void impSend(uint8_t type, const uint8_t *data, uint8_t len) {
  uint8_t p[300];
  memcpy(p, "IMPROV", 6);
  p[6] = 1;
  p[7] = type;
  p[8] = len;
  memcpy(p + 9, data, len);
  uint8_t sum = 0;
  for (int i = 0; i < 9 + len; i++) sum += p[i];
  p[9 + len] = sum;
  p[10 + len] = '\n';
  Serial.write(p, 11 + len);
}
static void impState(uint8_t s) { impSend(IMP_CURRENT_STATE, &s, 1); }
static void impError(uint8_t e) { impSend(IMP_ERROR, &e, 1); }
static void impResult(uint8_t cmd, const char *const *strs, int n) {
  uint8_t d[250];
  int k = 2;
  for (int i = 0; i < n; i++) {
    int l = strlen(strs[i]);
    if (k + 1 + l > (int)sizeof d) break;
    d[k++] = l;
    memcpy(d + k, strs[i], l);
    k += l;
  }
  d[0] = cmd;
  d[1] = k - 2;
  impSend(IMP_RPC_RESULT, d, k);
}
static void impUrl(uint8_t cmd) {
  String u = String("http://") + WiFi.localIP().toString() + "/";
  const char *s[1] = {u.c_str()};
  impResult(cmd, s, 1);
}

static void impCommand(const uint8_t *d, int len) {
  if (len < 2) { impError(IMP_ERR_INVALID); return; }
  uint8_t cmd = d[0], dl = d[1];
  const uint8_t *x = d + 2;
  if (dl + 2 > len) { impError(IMP_ERR_INVALID); return; }
  switch (cmd) {
    case 1: {   // WLAN-Daten
      if (dl < 1 || x[0] + 1 > dl) { impError(IMP_ERR_INVALID); return; }
      String ssid((const char *)x + 1, x[0]);
      int pl = x[1 + x[0]];
      String pass((const char *)x + 2 + x[0], pl);
      saveWifi(ssid, pass);
      impState(IMP_PROVISIONING);
      reconnect();
      impWaiting = true;
      impDeadline = millis() + 20000;
      break;
    }
    case 2:   // aktueller Zustand
      if (md == ONLINE) { impState(IMP_PROVISIONED); impUrl(2); }
      else impState(IMP_READY);
      break;
    case 3: {   // Geraete-Info
      const char *s[4] = {"Kumpel", fw, "ESP32-S3", "Kumpel"};
      impResult(3, s, 4);
      break;
    }
    case 4: {   // WLANs suchen
      int n = WiFi.scanNetworks();
      for (int i = 0; i < n && i < 20; i++) {
        String ss = WiFi.SSID(i), rs = String(WiFi.RSSI(i));
        if (!ss.length()) continue;
        const char *s[3] = {ss.c_str(), rs.c_str(), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "NO" : "YES"};
        impResult(4, s, 3);
      }
      impResult(4, nullptr, 0);
      WiFi.scanDelete();
      break;
    }
    default: impError(IMP_ERR_UNKNOWN_CMD);
  }
}

static void improvLoop() {
  while (Serial.available()) {
    uint8_t c = Serial.read();
    if (ipos < 6) {
      if (c == (uint8_t)"IMPROV"[ipos]) ib[ipos++] = c;
      else ipos = (c == 'I') ? (ib[0] = c, 1) : 0;
      continue;
    }
    ib[ipos++] = c;
    if (ipos >= 9 && ipos == 10 + ib[8]) {
      uint8_t sum = 0;
      for (int i = 0; i < ipos - 1; i++) sum += ib[i];
      bool ok = sum == ib[ipos - 1] && ib[6] == 1;
      if (ok && ib[7] == IMP_RPC) impCommand(ib + 9, ib[8]);
      else if (!ok) impError(IMP_ERR_INVALID);
      ipos = 0;
    }
    if (ipos >= (int)sizeof ib) ipos = 0;
  }
  if (impWaiting) {
    if (md == ONLINE) { impWaiting = false; impState(IMP_PROVISIONED); impUrl(1); }
    else if (millis() > impDeadline) { impWaiting = false; impError(IMP_ERR_CONNECT); impState(IMP_READY); }
  }
}

// ------------------------------------------------------------------ Ablauf
void begin(const char *fwVersion) {
  fw = fwVersion;
  WiFi.persistent(false);       // WLAN-Daten nur in unserem Namensraum speichern
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);         // schnellere Reaktion beim Schalten
  if (S.ssid.length()) reconnect();
  else startAp();
}

void loop() {
  improvLoop();
  if (ap) dns.processNextRequest();
  bool up = WiFi.status() == WL_CONNECTED;
  if (up && md != ONLINE) {
    md = ONLINE;
    onlineSince = millis();
    Serial.printf("WLAN verbunden: %s\n", WiFi.localIP().toString().c_str());
    if (!mdnsOn && MDNS.begin(HOSTNAME)) { MDNS.addService("http", "tcp", 80); mdnsOn = true; }
    configTzTime(S.tz.c_str(), "pool.ntp.org", "time.google.com");
    ntpOn = true;
  }
  if (!up && md == ONLINE) { md = ap ? SETUP_AP : CONNECTING; connectStart = millis(); }
  // Hotspot nur solange noetig: nach 30 s ohne WLAN an, nach 2 Minuten online wieder aus
  if (md == CONNECTING && millis() - connectStart > 30000) startAp();
  if (ap && md == ONLINE && millis() - onlineSince > 120000) stopAp();
}

}  // namespace net
