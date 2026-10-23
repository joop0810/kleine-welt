#include "config_json.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

namespace kb {

static const char *SKIN_KEYS[] = {"eyes", "drop", "pebble", "soft", "face", "cat", "robot"};
const char *skinKey(int s) { return s >= 0 && s < SKINS ? SKIN_KEYS[s] : "eyes"; }

static void copy(char *dst, size_t n, JsonVariantConst v) {
  const char *s = v.as<const char *>();
  snprintf(dst, n, "%s", s ? s : "");
}
static int16_t parseTime(JsonVariantConst v, int16_t def) {
  const char *s = v.as<const char *>();
  int h, m;
  if (s && sscanf(s, "%d:%d", &h, &m) == 2 && h >= 0 && h < 24 && m >= 0 && m < 60) return h * 60 + m;
  return def;
}
static int clampInt(JsonVariantConst v, int lo, int hi, int def) {
  if (!v.is<int>()) return def;
  int x = v.as<int>();
  return x < lo ? lo : (x > hi ? hi : x);
}

bool configFromJson(const char *json, size_t len, Config &c) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return false;
  defaultConfig(c);
  const char *sk = doc["skin"] | "eyes";
  for (int i = 0; i < SKINS; i++) if (!strcmp(sk, SKIN_KEYS[i])) c.skin = i;

  for (JsonObjectConst p : doc["pages"].as<JsonArrayConst>()) {
    if (c.nPages >= MAX_PAGES) break;
    PageCfg &pc = c.pages[c.nPages];
    pc.type = pageTypeFromKey(p["type"] | "");
    copy(pc.entity, sizeof pc.entity, p["entity"]);
    copy(pc.title, sizeof pc.title, p["title"]);
    if (pc.type != P_NONE && pc.entity[0]) c.nPages++;
  }
  copy(c.coldEntity, sizeof c.coldEntity, doc["cold"]["entity"]);
  c.coldBelow = doc["cold"]["below"] | 18.0f;
  copy(c.weatherEntity, sizeof c.weatherEntity, doc["weather"]["entity"]);
  c.hotAbove = doc["weather"]["hot"] | 28.0f;
  if (doc["sleep"]["on"] | true) {
    c.sleepFrom = parseTime(doc["sleep"]["from"], c.sleepFrom);
    c.sleepTo = parseTime(doc["sleep"]["to"], c.sleepTo);
  } else {
    c.sleepFrom = c.sleepTo = -1;
  }
  c.nightOff = doc["sleep"]["off"] | true;
  for (JsonObjectConst h : doc["hints"].as<JsonArrayConst>()) {
    if (c.nHints >= MAX_HINTS) break;
    HintCfg &hc = c.hints[c.nHints];
    copy(hc.entity, sizeof hc.entity, h["entity"]);
    hc.cond = condFromKey(h["cond"] | "eq");
    copy(hc.value, sizeof hc.value, h["value"]);
    copy(hc.text, sizeof hc.text, h["text"]);
    if (hc.entity[0] && hc.text[0]) c.nHints++;
  }
  c.brightDay = clampInt(doc["bright"]["day"], 10, 255, c.brightDay);
  c.brightNight = clampInt(doc["bright"]["night"], 0, 255, c.brightNight);
  c.homeAfter = clampInt(doc["homeAfter"], 10, 250, c.homeAfter);
  // "rotate": "auto" (Lagesensor) oder Grad 0/90/180/270; fehlt es, gilt der Standard (auto)
  JsonVariantConst rv = doc["rotate"];
  if (rv.is<const char *>()) {
    c.autoRotate = !strcmp(rv.as<const char *>(), "auto");
    c.rotate = 0;
  } else if (rv.is<int>()) {
    c.autoRotate = 0;
    c.rotate = (clampInt(rv, 0, 270, 0) / 90) & 3;
  }
  return true;
}

size_t configToJson(const Config &c, char *out, size_t outLen) {
  JsonDocument doc;
  doc["skin"] = skinKey(c.skin);
  JsonArray pages = doc["pages"].to<JsonArray>();
  for (int i = 0; i < c.nPages; i++) {
    JsonObject p = pages.add<JsonObject>();
    p["type"] = pageTypeKey(c.pages[i].type);
    p["entity"] = c.pages[i].entity;
    p["title"] = c.pages[i].title;
  }
  doc["cold"]["entity"] = c.coldEntity;
  doc["cold"]["below"] = c.coldBelow;
  doc["weather"]["entity"] = c.weatherEntity;
  doc["weather"]["hot"] = c.hotAbove;
  char t[12];
  bool on = c.sleepFrom >= 0 && c.sleepTo >= 0;
  doc["sleep"]["on"] = on;
  snprintf(t, sizeof t, "%02d:%02d", on ? c.sleepFrom / 60 : 23, on ? c.sleepFrom % 60 : 0);
  doc["sleep"]["from"] = t;
  snprintf(t, sizeof t, "%02d:%02d", on ? c.sleepTo / 60 : 7, on ? c.sleepTo % 60 : 0);
  doc["sleep"]["to"] = t;
  doc["sleep"]["off"] = (bool)c.nightOff;
  JsonArray hints = doc["hints"].to<JsonArray>();
  for (int i = 0; i < c.nHints; i++) {
    JsonObject h = hints.add<JsonObject>();
    h["entity"] = c.hints[i].entity;
    h["cond"] = condKey(c.hints[i].cond);
    h["value"] = c.hints[i].value;
    h["text"] = c.hints[i].text;
  }
  doc["bright"]["day"] = c.brightDay;
  doc["bright"]["night"] = c.brightNight;
  doc["homeAfter"] = c.homeAfter;
  if (c.autoRotate) doc["rotate"] = "auto";
  else doc["rotate"] = (c.rotate & 3) * 90;
  size_t need = measureJson(doc);
  if (need + 1 > outLen) return 0;
  return serializeJson(doc, out, outLen);
}

}  // namespace kb
