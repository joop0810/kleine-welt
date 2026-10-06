// Kleine Welt - zehn Mallorca-Szenen als Wohlfuehl-Deko fuer die Wand (Wechsel alle 10 Minuten)
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
//
// Uhrzeit: kommt aus der RTC (PCF85063), die wie bei TamaPoke die Ortszeit haelt.
// Nur wenn man am Geraet die Uhr stellt, werden Stunde und Minute in die RTC
// geschrieben (Datum bleibt) - genau wie TamaPokes eigenes Uhr-Menue es tut.

#include <Arduino.h>
#include <Wire.h>
#include "pin_config.h"          // definiert XPOWERS_CHIP_AXP2101, muss vor XPowersLib stehen
#include "Arduino_GFX_Library.h"
#include "TouchDrvCSTXXX.hpp"
#include <SensorPCF85063.hpp>
#include <XPowersLib.h>
#include "game.h"

#define FW_VERSION "0.4.0"

Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *panel = new Arduino_CO5300(bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
TouchDrvCST92xx touch;
XPowersPMU pmu;
SensorPCF85063 rtc;
static bool rtcOk = false;

static uint16_t *fb = nullptr, *work = nullptr;

// Touch: der CST9217 meldet per INT, wenn Daten da sind. Nur dann (oder solange
// ein Finger liegt) wird I2C gelesen - ein schlafender Chip blockiert sonst ~1 s.
static volatile bool touchIrq = false;
static void IRAM_ATTR touchIsr() { touchIrq = true; }
static kw::Touch fingers[2];
static int nFingers = 0;

static void readTouch() {
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll < 15) return;
  lastPoll = millis();
  if (!touchIrq && nFingers == 0) return;
  touchIrq = false;
  int16_t xs[2], ys[2];
  int n = touch.getPoint(xs, ys, 2);
  if (n > 2) n = 2;
  if (n < 0) n = 0;
  for (int i = 0; i < n; i++) { fingers[i].x = xs[i]; fingers[i].y = ys[i]; }
  nFingers = n;
}

// Uhr aus der RTC an den Spielkern geben (alle 10 s; dazwischen zaehlt der Kern selbst)
static void syncClock() {
  if (!rtcOk) return;
  RTC_DateTime t = rtc.getDateTime();
  if (t.getYear() < 2025 || t.getYear() > 2120) return;   // keine gueltige Zeit
  kw::setClock(t.getHour(), t.getMinute(), t.getSecond(), t.getMonth());
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
  rtcOk = rtc.begin(Wire, IIC_SDA, IIC_SCL);
  if (!rtcOk) Serial.println("RTC PCF85063 nicht gefunden - Uhr laeuft ab 12:00");

  fb = (uint16_t *)ps_malloc(LCD_WIDTH * LCD_HEIGHT * 2);
  work = (uint16_t *)ps_malloc(LCD_WIDTH * LCD_HEIGHT * 2);
  if (!fb || !work) { Serial.println("PSRAM fehlt - PSRAM=opi eingestellt?"); while (true) delay(1000); }

  if (!panel->begin(80000000)) Serial.println("Display-Start fehlgeschlagen");
  panel->fillScreen(0x0000);
  panel->setBrightness(150);

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
  kw::begin(fb, work, esp_random());
  Serial.printf("Start in %lu ms\n", (unsigned long)(millis() - t0));
  syncClock();
}

void loop() {
  // ~25 Bilder/s reichen fuer ruhige Deko und lassen das Panel kuehler
  static uint32_t lastFrame = 0, lastClock = 0, fpsT = 0, frames = 0;
  static uint8_t brightNow = 0;
  readTouch();
  uint32_t now = millis();
  if (now - lastClock > 10000) { lastClock = now; syncClock(); }
  if (now - lastFrame < 40) { delay(1); return; }
  lastFrame = now;

  kw::tick(now, fingers, nFingers);
  panel->draw16bitRGBBitmap(0, 0, fb, LCD_WIDTH, LCD_HEIGHT);

  // Helligkeit folgt der Tageszeit (sanft, damit es nicht springt)
  uint8_t want = kw::wantBrightness();
  if (want != brightNow) {
    brightNow += want > brightNow ? 1 : -1;
    panel->setBrightness(brightNow);
  }

  // Uhr am Geraet gestellt? -> Stunde/Minute in die RTC, Datum bleibt
  int h, m;
  if (kw::takeClockChange(h, m) && rtcOk) {
    RTC_DateTime t = rtc.getDateTime();
    uint16_t y = t.getYear(); uint8_t mo = t.getMonth(), d = t.getDay();
    if (y < 2025 || y > 2120) { y = 2026; mo = 1; d = 1; }
    rtc.setDateTime(RTC_DateTime(y, mo, d, h, m, 0));
    Serial.printf("Uhr gestellt: %02d:%02d\n", h, m);
    syncClock();
  }

  frames++;
  if (now - fpsT > 10000) {
    kw::Debug d = kw::debug();
    Serial.printf("fps %.1f  zeit %.2f h  szene %s  tag %.2f  hell %u\n",
                  frames * 1000.0f / (now - fpsT), d.hours, kw::sceneName(d.scene), d.daylight, brightNow);
    fpsT = now; frames = 0;
  }
}
