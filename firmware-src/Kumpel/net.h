#pragma once
// WLAN, Einrichtungs-Hotspot, Improv (WLAN direkt aus dem Web-Installer),
// Uhrzeit aus dem Internet und der Speicher fuer die Einstellungen.
//
// Gespeichert wird im NVS unter dem eigenen Namensraum "kumpel". Andere
// Firmwares auf dem Board (z. B. TamaPokes Spielstand) liegen in eigenen
// Namensraeumen und werden nicht angefasst.
#include <Arduino.h>

namespace net {

struct Settings {
  String ssid, pass;
  String haUrl, haToken;
  String tz;       // POSIX-Zeitzone, z. B. "CET-1CEST,M3.5.0,M10.5.0/3"
  String config;   // Kumpel-Einstellungen als JSON
};
Settings &settings();
void load();
void saveWifi(const String &ssid, const String &pass);
void saveHa(const String &url, const String &token);
void saveTz(const String &tz);
void saveConfig(const String &json);

enum Mode : uint8_t { NO_WIFI, CONNECTING, ONLINE, SETUP_AP };
void begin(const char *fwVersion);
void loop();       // inkl. Improv ueber USB
Mode mode();
bool apActive();
String ip();
bool timeValid();  // Uhrzeit aus dem Internet vorhanden
void reconnect();  // nach neuen Zugangsdaten

extern const char *HOSTNAME;   // kumpel -> http://kumpel.local
extern const char *AP_NAME;    // Einrichtungs-Hotspot

}  // namespace net
