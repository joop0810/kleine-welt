# Kleine Welt

Wohlfühl-Deko für das **Waveshare ESP32-S3-Touch-AMOLED-1.75** (466×466 rundes AMOLED):
zehn kleine Mallorca-Szenen (Cala, Felsbucht, Fischerhafen, Olivenfinca, Bergdorf, Steilküste,
Windmühlen, Palma, Strandbar, Felsentor), die alle 10 Minuten überblenden. Jede folgt der echten
Uhrzeit aus der eingebauten Uhr. Wischen = nächste Szene, lange drücken = Uhr stellen.

Der frühere Zelda-artige Prototyp liegt im Branch `zelda-prototyp`.

## Installieren

Installer-Seite (GitHub Pages) in Chrome/Edge öffnen, Board per USB-C anschließen,
*Installieren* → bei **„Erase device“ nichts anhaken**.

TamaPoke-Spielstand (NVS) und Ruhmeshalle (FFat) bleiben dabei erhalten: Kleine Welt
nutzt dieselbe Partitionstabelle und schreibt keinen dieser Bereiche. `tools/make_manifest.py`
bricht ab, falls sich die Partitionstabelle je ändern sollte.

## Aufbau

| Pfad | Inhalt |
|---|---|
| `firmware-src/KleineWelt/game.cpp` | Kern (ohne Arduino-Abhängigkeit): Tageslicht, Himmel, Boote, Wechsel, Eingabe |
| `firmware-src/KleineWelt/scenes.h` | die zehn Szenen (Bauplan + Animationen) |
| `firmware-src/KleineWelt/KleineWelt.ino` | Board: Display (CO5300/QSPI), Touch (CST9217), RTC (PCF85063), AXP2101, Helligkeit |
| `host/sim.cpp` | PC-Test: rendert jede Szene zu drei Tageszeiten, prüft Wechsel, Wischen, Tippen, Uhr |
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

## Pins für später (Taster-Matrix 2×3)

Zeilen GPIO43/44, Spalten GPIO16/17/18 (mit Dioden) – in `pin_config.h` vorgemerkt, noch nicht benutzt.
