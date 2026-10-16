#pragma once
// Verbindung zu Home Assistant:
//  - WebSocket-API (eigener kleiner Client) fuer Live-Zustaende per subscribe_entities
//    und zum Schalten per call_service,
//  - REST-API fuer die Entity-Liste der Einstellungsseite und den Verbindungstest.
#include <Arduino.h>
#include "kumpel.h"

namespace ha {

enum State : uint8_t { OFF = 0, CONNECTING, AUTH_FAILED, READY };

void begin();
// URL wie "http://homeassistant.local:8123" oder "https://xyz.ui.nabu.casa"
void setServer(const char *url, const char *token);
void resubscribe();       // nach Aenderung der Seiten/Hinweise
void loop();
State state();
bool call(const kb::Action &a);

// Blockierend (1-5 s), nur fuer die Einstellungsseite.
bool test(const char *url, const char *token, String &msg);
// Holt alle Entities als JSON [{"i":"light.x","n":"Name","u":"°C","s":"on"},...]
// in einen Puffer im PSRAM. Liefert nullptr bei Fehler (err gesetzt).
const char *fetchEntities(size_t &len, String &err);

}  // namespace ha
