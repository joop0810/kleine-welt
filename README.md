# Kleine Welt

Prototyp eines eigenen Action-Adventures für das **Waveshare ESP32-S3-Touch-AMOLED-1.75**
(466×466 rundes AMOLED). Das runde Display ist das Fenster in die Welt.

**Stand 0.2:** vier verbundene Räume (Eingang → Der Block → Zwei Platten → Schatzkammer),
Schleimlinge, Block-auf-Platte-Rätsel, Truhe als Ziel. Steuerung: Steuerkreuz links unten,
Schwert-Knopf rechts unten (zwei Finger gleichzeitig möglich).

## Installieren

Installer-Seite (GitHub Pages) in Chrome/Edge öffnen, Board per USB-C anschließen,
*Installieren* → bei **„Erase device“ nichts anhaken**.

TamaPoke-Spielstand (NVS) und Ruhmeshalle (FFat) bleiben dabei erhalten: Kleine Welt
nutzt dieselbe Partitionstabelle und schreibt keinen dieser Bereiche. `tools/make_manifest.py`
bricht ab, falls sich die Partitionstabelle je ändern sollte.

## Aufbau

| Pfad | Inhalt |
|---|---|
| `firmware-src/KleineWelt/game.cpp` | Spielkern (ohne Arduino-Abhängigkeit), Pixelkunst, Raum, Gegner, Rätsel |
| `firmware-src/KleineWelt/KleineWelt.ino` | Board: Display (CO5300/QSPI), Touch (CST9217), AXP2101, Dimmen |
| `host/sim.cpp` | PC-Test: spielt den Raum per simuliertem Touch durch, speichert Bilder |
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
