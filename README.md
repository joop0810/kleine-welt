# Kleine Welt

Zwei Firmwares für das **Waveshare ESP32-S3-Touch-AMOLED-1.75**, auswählbar auf der Installer-Seite:

- **Deko-Welten** (`firmware-src/KleineWelt`) – Mallorca, Kunst, Aquarium und Mond, siehe unten.
- **Kumpel** (`firmware-src/Kumpel`) – ein Kumpel mit Gefühlen (Figur wählbar: Leuchtaugen,
  Tröpfchen, Kiesel) und selbst zusammengestellten Home-Assistant-Seiten zum Wischen.

## Deko-Welten

Wohlfühl-Deko für das **Waveshare ESP32-S3-Touch-AMOLED-1.75** (466×466 rundes AMOLED) mit vier Welten:

- **Mallorca** – zehn kleine Szenen (Cala, Felsbucht, Fischerhafen, Olivenfinca, Bergdorf, Steilküste,
  Windmühlen, Palma, Strandbar, Felsentor). Jede folgt der echten Uhrzeit aus der eingebauten Uhr.
- **Kunst** – Strömung (Teilchen im Wirbelfeld), Lavalampe, Sternenhimmel (dreht sich um den Polarstern),
  Game of Life auf rundem Feld.
- **Aquarium** – das runde Display ist das Glas; die Wasserlinie folgt dem Lagesensor und schwappt nach.
  Antippen streut Futter. Tagsüber Licht, nachts gedämpft.
- **Mond** – echte Mondphase aus Datum und Uhrzeit (Meeus, gegen PyEphem geprüft), Antippen zeigt Phase und
  Tage bis Voll-/Neumond. Optional als **Nachtmond**: ab einer wählbaren Stunde bis Sonnenaufgang, gedimmt.

Alle 10 Minuten (einstellbar) blendet das nächste Bild über. Waagerecht wischen = nächstes Bild,
hoch/runter wischen = nächste Welt, lange drücken = Menü (Uhr, Wechsel, Welten, Nachtmond).
Die Menü-Einstellungen liegen im eigenen NVS-Namensraum `welt`.

Der frühere Zelda-artige Prototyp liegt im Branch `zelda-prototyp`.

## Installieren

Installer-Seite (GitHub Pages) in Chrome/Edge öffnen, Board per USB-C anschließen,
*Installieren* → bei **„Erase device“ nichts anhaken**.

TamaPoke-Spielstand (NVS) und Ruhmeshalle (FFat) bleiben dabei erhalten: Kleine Welt
nutzt dieselbe Partitionstabelle und schreibt keinen dieser Bereiche (nur den eigenen NVS-Namensraum `welt`). `tools/make_manifest.py`
bricht ab, falls sich die Partitionstabelle je ändern sollte.

## Aufbau

| Pfad | Inhalt |
|---|---|
| `firmware-src/KleineWelt/game.cpp` | Kern (ohne Arduino-Abhängigkeit): Tageslicht, Himmel, Boote, Wechsel, Welten, Menü, Eingabe |
| `firmware-src/KleineWelt/scenes.h` | die zehn Mallorca-Szenen (Bauplan + Animationen) |
| `firmware-src/KleineWelt/welten.h` | Kunst, Aquarium, Mond (inkl. Mondphasen-Rechnung) |
| `firmware-src/KleineWelt/KleineWelt.ino` | Board: Display (CO5300/QSPI), Touch (CST9217), RTC (PCF85063), Lagesensor (QMI8658), AXP2101, Helligkeit, Einstellungen |
| `host/sim.cpp` | PC-Test: rendert jedes Bild, prüft Wechsel, Wischen, Menü, Uhr, Nachtmond, Mondphase, Aquarium, Tippen |
| `tools/make_manifest.py` | erzeugt `manifest.json`, prüft Partitionstabelle |
| `.github/workflows/firmware.yml` | Cloud-Build bei jeder Änderung am Quellcode |

## Selbst bauen

```
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" --export-binaries firmware-src/KleineWelt
```

Arduino-Core esp32 3.3.11, GFX Library for Arduino 1.6.7, SensorLib 0.4.1, XPowersLib 0.3.3.

PC-Test:

```
g++ -O2 -std=c++17 -Ifirmware-src/KleineWelt host/sim.cpp firmware-src/KleineWelt/game.cpp -o sim && mkdir -p out && ./sim out
```

## Kumpel

Startseite: der Kumpel mit Uhrzeit. Er freut sich beim Antippen, ist beim Schütteln überrascht,
friert unter einer einstellbaren Temperatur, schläft nachts und zeigt Hinweise in einer
Sprechblase (z. B. Müllabfuhr). Wischen blättert durch bis zu acht Seiten:
Heizung/Klima, Licht (Helligkeit + Farben), Schalter, Anzeige, Szene/Skript/Taste, Saugroboter.

Einrichtung: WLAN direkt im Web-Installer (Improv) oder über das Einrichtungs-WLAN
`Kumpel-Setup`; danach `http://kumpel.local` → Home-Assistant-Adresse + langlebiges Token,
Seiten und Reaktionen per Auswahlliste.
Das Bild richtet sich über den Lagesensor automatisch aus (z. B. im Figuren-Ständer mit
USB-C nach unten); liegt das Gerät flach, bleibt die letzte Lage. Unter *Anzeige → Bild drehen*
lässt sich stattdessen eine feste Lage (0/90/180/270°) wählen. Touch und Wischen drehen mit. Gespeichert wird nur im NVS-Namensraum `kumpel`.

| Pfad | Inhalt |
|---|---|
| `firmware-src/Kumpel/kumpel.cpp` | Kern (ohne Arduino): Figuren, Stimmungen, Seiten, Gesten |
| `firmware-src/Kumpel/gfx.cpp`, `font_data.cpp` | geglättete Formen und Schrift (Poppins, SIL OFL) |
| `firmware-src/Kumpel/ha.cpp` | Home Assistant: WebSocket (`subscribe_entities`, `call_service`) + REST |
| `firmware-src/Kumpel/net.cpp` | WLAN, Einrichtungs-WLAN, Improv, Internetzeit, Speicher |
| `firmware-src/Kumpel/web.cpp`, `web_page.h` | Einstellungsseite und JSON-Schnittstelle |
| `host/kumpel_sim.cpp` | PC-Test: alle Figuren/Stimmungen, alle Seiten, Gesten, Aktionen, JSON |
| `tools/make_font.py` | erzeugt `font_data.cpp` aus Poppins |
| `.github/workflows/kumpel.yml` | Cloud-Build → `firmware-kumpel/` + `manifest-kumpel.json` |

Selbst bauen (zusätzlich ArduinoJson 7.4.2):

```
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" --export-binaries firmware-src/Kumpel
```

PC-Test:

```
K=firmware-src/Kumpel; g++ -O2 -std=c++17 -I$K -I<ArduinoJson>/src host/kumpel_sim.cpp $K/kumpel.cpp $K/gfx.cpp $K/font_data.cpp $K/config_json.cpp -o kumpel_sim && mkdir -p kumpel-out && ./kumpel_sim kumpel-out
```

## Pins für später (Taster-Matrix 2×3)

Zeilen GPIO43/44, Spalten GPIO16/17/18 (mit Dioden) – in `pin_config.h` vorgemerkt, noch nicht benutzt.
