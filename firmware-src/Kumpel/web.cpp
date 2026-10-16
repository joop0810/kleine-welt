#include "web.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "kumpel.h"
#include "config_json.h"
#include "net.h"
#include "ha.h"
#include "web_page.h"

namespace web {

static WebServer server(80);
static const char *fw = "";
static uint32_t reconnectAt = 0;

static void sendJson(JsonDocument &d, int code = 200) {
  String s;
  serializeJson(d, s);
  server.send(code, "application/json", s);
}
static void reply(bool ok, const String &msg) {
  JsonDocument d;
  d["ok"] = ok;
  d["msg"] = msg;
  sendJson(d, 200);
}
static bool body(JsonDocument &d) {
  if (!server.hasArg("plain")) return false;
  return !deserializeJson(d, server.arg("plain"));
}

static void hStatus() {
  JsonDocument d;
  d["fw"] = fw;
  d["ip"] = net::ip();
  JsonObject w = d["wifi"].to<JsonObject>();
  w["online"] = net::mode() == net::ONLINE;
  w["ap"] = net::apActive();
  w["ssid"] = net::settings().ssid;
  w["rssi"] = WiFi.RSSI();
  JsonObject h = d["ha"].to<JsonObject>();
  h["url"] = net::settings().haUrl;
  h["token"] = net::settings().haToken.length() > 0;   // das Token selbst verlaesst das Geraet nie
  static const char *st[] = {"off", "connecting", "auth", "ready"};
  h["state"] = st[ha::state()];
  h["error"] = ha::lastError();
  d["tz"] = net::settings().tz;
  sendJson(d);
}

static void hGetConfig() {
  static char buf[4096];
  size_t n = kb::configToJson(kb::config(), buf, sizeof buf);
  server.send(200, "application/json", n ? buf : "{}");
}

static void hSetConfig() {
  String b = server.arg("plain");
  kb::Config c;
  if (!kb::configFromJson(b.c_str(), b.length(), c)) { reply(false, "Ungültige Daten"); return; }
  static char buf[4096];
  size_t n = kb::configToJson(c, buf, sizeof buf);
  if (!n) { reply(false, "Zu viele Einstellungen"); return; }
  net::saveConfig(String(buf));
  kb::setConfig(c);
  ha::resubscribe();
  reply(true, "Gespeichert");
}

static void hHa() {
  JsonDocument d;
  if (!body(d)) { reply(false, "Ungültige Daten"); return; }
  String url = d["url"] | "", tok = d["token"] | "";
  if (!tok.length()) tok = net::settings().haToken;   // leer = gespeichertes behalten
  if (net::mode() != net::ONLINE) { reply(false, "Erst WLAN verbinden"); return; }
  String msg;
  bool ok = ha::test(url.c_str(), tok.c_str(), msg);
  if (ok) {
    net::saveHa(url, tok);
    ha::setServer(url.c_str(), tok.c_str());
    msg = "Verbunden und gespeichert";
  }
  reply(ok, msg);
}

static void hWifi() {
  JsonDocument d;
  if (!body(d)) { reply(false, "Ungültige Daten"); return; }
  String ssid = d["ssid"] | "", pass = d["pass"] | "";
  if (!ssid.length()) { reply(false, "Netzwerk fehlt"); return; }
  if (!pass.length() && ssid == net::settings().ssid) pass = net::settings().pass;
  net::saveWifi(ssid, pass);
  reply(true, "Gespeichert – der Kumpel verbindet sich neu. Danach unter http://kumpel.local erreichbar.");
  reconnectAt = millis() + 800;   // erst antworten, dann umschalten
}

static void hTz() {
  JsonDocument d;
  if (!body(d)) { reply(false, "Ungültige Daten"); return; }
  net::saveTz(d["tz"] | "");
  reply(true, "Gespeichert");
}

static void hScan() {
  int n = WiFi.scanNetworks();
  JsonDocument d;
  JsonArray a = d.to<JsonArray>();
  for (int i = 0; i < n && i < 25; i++) {
    if (!WiFi.SSID(i).length()) continue;
    JsonObject o = a.add<JsonObject>();
    o["s"] = WiFi.SSID(i);
    o["r"] = WiFi.RSSI(i);
    o["l"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  sendJson(d);
}

static void hEntities() {
  size_t len;
  String err;
  const char *list = ha::fetchEntities(len, err);
  if (!list) {
    JsonDocument d;
    d["error"] = err;
    sendJson(d);
    return;
  }
  server.setContentLength(len);
  server.send(200, "application/json", "");
  WiFiClient c = server.client();
  for (size_t off = 0; off < len; off += 4096) c.write((const uint8_t *)list + off, len - off < 4096 ? len - off : 4096);
}

void begin(const char *fwVersion) {
  fw = fwVersion;
  server.on("/", HTTP_GET, [] { server.send(200, "text/html; charset=utf-8", WEB_PAGE); });
  server.on("/api/status", HTTP_GET, hStatus);
  server.on("/api/config", HTTP_GET, hGetConfig);
  server.on("/api/config", HTTP_POST, hSetConfig);
  server.on("/api/ha", HTTP_POST, hHa);
  server.on("/api/wifi", HTTP_POST, hWifi);
  server.on("/api/tz", HTTP_POST, hTz);
  server.on("/api/scan", HTTP_GET, hScan);
  server.on("/api/entities", HTTP_GET, hEntities);
  // Einrichtungs-WLAN: Handys pruefen bestimmte Adressen -> alles auf die Startseite
  server.onNotFound([] {
    server.sendHeader("Location", String("http://") + net::ip() + "/", true);
    server.send(302, "text/plain", "");
  });
  server.begin();
}

void loop() {
  server.handleClient();
  if (reconnectAt && millis() > reconnectAt) { reconnectAt = 0; net::reconnect(); }
}

}  // namespace web
