#pragma once
// Einstellungen <-> JSON (fuer Einstellungsseite und Speicher). Nur ArduinoJson,
// kein Arduino - laeuft auch im PC-Test.
#include <stddef.h>
#include "kumpel.h"

namespace kb {

// Liest JSON in cfg (fehlende Felder = Standard). false bei kaputtem JSON.
bool configFromJson(const char *json, size_t len, Config &cfg);
// Schreibt cfg als JSON nach out. Liefert Laenge (0 = zu klein).
size_t configToJson(const Config &cfg, char *out, size_t outLen);

const char *skinKey(int s);

}  // namespace kb
