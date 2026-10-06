// Kleine Welt - Prototyp
// Board: Waveshare ESP32-S3-Touch-AMOLED-1.75 (466x466 AMOLED, CO5300 QSPI, Touch CST9217)
//
// Bibliotheken (gleiche Versionen wie TamaPoke):
//   GFX Library for Arduino 1.6.7, SensorLib 0.4.1, XPowersLib 0.3.3
// FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB
//
// WICHTIG - Nachbarschaft mit TamaPoke:
// Diese Firmware schreibt NICHTS in den NVS-Speicher und bindet die FFat-Partition
// nicht ein. TamaPokes Spielstand (NVS) und Ruhmeshalle (FFat) bleiben unberuehrt,
// solange beim Flashen "Erase device" nicht angehakt ist. Die Partitionstabelle ist
// dieselbe wie bei TamaPoke (app3M_fat9M_16MB) - bitte nicht aendern.

#include <Arduino.h>
#include <Wire.h>
#include "pin_config.h"          // definiert XPOWERS_CHIP_AXP2101, muss vor XPowersLib stehen
#include "Arduino_GFX_Library.h"
#include "TouchDrvCSTXXX.hpp"
#include <XPowersLib.h>
#include "game.h"

#define FW_VERSION "0.1.0"

Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *panel = new Arduino_CO5300(bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
TouchDrvCST92xx touch;
XPowersPMU pmu;

static uint16_t *fb = nullptr, *bg = nullptr;

// Touch: der CST9217 meldet per INT, wenn Daten da sind. Nur dann (oder solange
// der Finger liegt) wird I2C gelesen - ein schlafender Chip blockiert sonst ~1 s.
static volatile bool touchIrq = false;
static void IRAM_ATTR touchIsr() { touchIrq = true; }
static bool fingerDown = false;
static int16_t fingerX = 0, fingerY = 0;

static void readTouch() {
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll < 15) return;
  lastPoll = millis();
  if (!touchIrq && !fingerDown) return;
  touchIrq = false;
  int16_t x, y;
  bool pressed = touch.getPoint(&x, &y, 1) > 0;
  if (pressed) { fingerX = x; fingerY = y; }
  fingerDown = pressed;
}

// AMOLED schonen: nach 60 s ohne Beruehrung dunkler, nach 3 min aus.
static const uint8_t BRIGHT = 190;
static uint32_t lastTouchMs = 0;
static uint8_t dimStage = 0;     // 0 an, 1 gedimmt, 2 aus
static bool swallow = false;     // Beruehrung zum Aufwecken nicht ans Spiel weitergeben

static void handleDimming() {
  uint32_t idle = millis() - lastTouchMs;
  uint8_t want = idle > 180000 ? 2 : (idle > 60000 ? 1 : 0);
  if (want != dimStage) {
    dimStage = want;
    panel->setBrightness(dimStage == 0 ? BRIGHT : (dimStage == 1 ? 40 : 0));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);   // ohne offenen Monitor nicht blockieren
  Serial.printf("Kleine Welt fw v%s\n", FW_VERSION);

  Wire.begin(IIC_SDA, IIC_SCL);
  Wire.setTimeOut(50);

  // Panel-Versorgung (BLDO1 = OLED VDD) einschalten, bevor das Display startet
  if (pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    pmu.setBLDO1Voltage(3300);
    pmu.enableBLDO1();
  } else {
    Serial.println("AXP2101 nicht gefunden");
  }

  fb = (uint16_t *)ps_malloc(LCD_WIDTH * LCD_HEIGHT * 2);
  bg = (uint16_t *)ps_malloc(LCD_WIDTH * LCD_HEIGHT * 2);
  if (!fb || !bg) { Serial.println("PSRAM fehlt - PSRAM=opi eingestellt?"); while (true) delay(1000); }

  // QSPI mit 80 MHz: das Uebertragen des ganzen Bildes ist der Engpass
  if (!panel->begin(80000000)) Serial.println("Display-Start fehlgeschlagen");
  panel->fillScreen(0x0000);
  panel->setBrightness(BRIGHT);

  touch.setPins(TP_RESET, TP_INT);
  bool ok = false;
  for (int i = 0; i < 3 && !ok; i++) { ok = touch.begin(Wire, 0x5A, IIC_SDA, IIC_SCL); if (!ok) delay(150); }
  if (!ok) Serial.println("Touch CST9217 nicht gefunden");
  touch.reset();                       // nach begin() im Kommandomodus -> Reset noetig
  touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
  touch.setMirrorXY(true, true);       // Panel ist um 180 Grad gedreht verbaut
  pinMode(TP_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TP_INT), touchIsr, FALLING);

  uint32_t t0 = millis();
  kw::begin(fb, bg, esp_random());
  Serial.printf("Raum vorgerendert in %lu ms\n", (unsigned long)(millis() - t0));
  lastTouchMs = millis();
}

void loop() {
  // ~40 Bilder/s Obergrenze; das Uebertragen braucht ohnehin den Grossteil der Zeit
  static uint32_t lastFrame = 0, fpsT = 0, frames = 0;
  readTouch();
  if (fingerDown) {
    if (dimStage > 0) swallow = true;   // erste Beruehrung weckt nur auf
    lastTouchMs = millis();
  }
  if (!fingerDown) swallow = false;
  handleDimming();
  if (dimStage == 2) { delay(30); return; }   // Bildschirm aus: Spiel pausiert

  uint32_t now = millis();
  if (now - lastFrame < 25) { delay(1); return; }
  lastFrame = now;

  kw::tick(now, fingerDown && !swallow, fingerX, fingerY);
  panel->draw16bitRGBBitmap(0, 0, fb, LCD_WIDTH, LCD_HEIGHT);

  frames++;
  if (now - fpsT > 5000) {
    kw::Debug d = kw::debug();
    Serial.printf("fps %.1f  held %.0f,%.0f  hp %d  block %d,%d  tuer %d  modus %d\n",
                  frames * 1000.0f / (now - fpsT), d.hx, d.hy, d.hp, d.bi, d.bj, d.doorOpen, d.mode);
    fpsT = now; frames = 0;
  }
}
