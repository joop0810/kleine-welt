// Kumpel - interaktiver Kumpel mit Home-Assistant-Seiten (Schwester-Firmware von Kleine Welt)
// Board: Waveshare ESP32-S3-Touch-AMOLED-1.75 (466x466 AMOLED, CO5300 QSPI, Touch CST9217)
//
// Bibliotheken: GFX Library for Arduino 1.6.7, SensorLib 0.4.1, XPowersLib 0.3.3, ArduinoJson 7.4.2
// FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB
//
// Speicher: nur der NVS-Namensraum "kumpel" (WLAN, Home Assistant, Einstellungen).
// TamaPokes Spielstand (eigener Namensraum) und Ruhmeshalle (FFat) bleiben unberuehrt,
// solange beim Flashen "Erase device" nicht angehakt ist. Gleiche Partitionstabelle.
//
// Uhrzeit: aus dem Internet, sobald WLAN da ist; wird dann auch in die RTC geschrieben
// (Ortszeit, wie TamaPoke und Kleine Welt sie erwarten). Ohne WLAN zaehlt die RTC.

#include <Arduino.h>
#include <Wire.h>
#include <time.h>
#include "pin_config.h"          // definiert XPOWERS_CHIP_AXP2101, muss vor XPowersLib stehen
#include "Arduino_GFX_Library.h"
#include "TouchDrvCSTXXX.hpp"
#include <SensorPCF85063.hpp>
#include <SensorQMI8658.hpp>
#include <XPowersLib.h>
#include "kumpel.h"
#include "config_json.h"
#include "net.h"
#include "ha.h"
#include "web.h"

#define FW_VERSION "1.1.0"

Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *panel = new Arduino_CO5300(bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
TouchDrvCST92xx touch;
XPowersPMU pmu;
SensorPCF85063 rtc;
SensorQMI8658 imu;
static bool rtcOk = false, imuOk = false;
static uint16_t *fb = nullptr;

// Touch wie bei Kleine Welt: nur lesen, wenn der Chip per INT etwas meldet
static volatile bool touchIrq = false;
static void IRAM_ATTR touchIsr() { touchIrq = true; }
static kb::Touch fingers[2];
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

// Uhr: Internetzeit bevorzugt, sonst RTC. Internetzeit wird stuendlich in die RTC geschrieben.
static void syncClock() {
  static uint32_t lastRtcWrite = 0;
  if (net::timeValid()) {
    time_t t = time(nullptr);
    struct tm lt;
    localtime_r(&t, &lt);
    kb::setClock(lt.tm_hour, lt.tm_min, lt.tm_sec, true);
    if (rtcOk && (!lastRtcWrite || millis() - lastRtcWrite > 3600000)) {
      lastRtcWrite = millis();
      rtc.setDateTime(RTC_DateTime(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec));
    }
    return;
  }
  if (!rtcOk) return;
  RTC_DateTime t = rtc.getDateTime();
  bool valid = t.getYear() >= 2025 && t.getYear() <= 2120;
  kb::setClock(t.getHour(), t.getMinute(), t.getSecond(), valid);
}

// Schuetteln: mehrere kraeftige Ausschlaege innerhalb einer Sekunde
static void readImu() {
  static uint32_t last = 0, firstPeak = 0, quietUntil = 0;
  static int peaks = 0;
  if (!imuOk || millis() - last < 20) return;
  last = millis();
  float x, y, z;
  if (!imu.getDataReady() || !imu.getAccelerometer(x, y, z)) return;
  float g = sqrtf(x * x + y * y + z * z);
  if (millis() < quietUntil) return;
  if (fabsf(g - 1.0f) > 0.9f) {
    if (!peaks || millis() - firstPeak > 1000) { peaks = 0; firstPeak = millis(); }
    if (++peaks >= 3) { kb::shake(); peaks = 0; quietUntil = millis() + 2500; }
  }
}

// Verbindungszustand fuer den Startbildschirm
static void updateConn() {
  static String ipLine;
  net::Mode m = net::mode();
  if (m == net::SETUP_AP || (m != net::ONLINE && net::apActive())) {
    kb::setConn(kb::CN_AP, "WLAN verbinden:", net::AP_NAME, "dann 192.168.4.1 \xC3\xB6" "ffnen");
    return;
  }
  if (m == net::NO_WIFI) { kb::setConn(kb::CN_NO_WIFI); return; }
  if (m == net::CONNECTING) { kb::setConn(kb::CN_WIFI); return; }
  switch (ha::state()) {
    case ha::OFF:
      ipLine = net::ip();
      kb::setConn(kb::CN_NO_HA, "Einrichten im Browser:", "kumpel.local", ipLine.c_str());
      break;
    case ha::CONNECTING: kb::setConn(ha::recentlyReady() ? kb::CN_OK : kb::CN_HA); break;   // kurze Neuverbindung nicht anzeigen
    case ha::AUTH_FAILED: kb::setConn(kb::CN_AUTH); break;
    case ha::READY: kb::setConn(kb::CN_OK); break;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);   // ohne offenen Monitor nicht blockieren
  Serial.printf("Kumpel fw v%s\n", FW_VERSION);

  Wire.begin(IIC_SDA, IIC_SCL);
  Wire.setTimeOut(50);
  if (pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    pmu.setBLDO1Voltage(3300);
    pmu.enableBLDO1();
  } else {
    Serial.println("AXP2101 nicht gefunden");
  }
  rtcOk = rtc.begin(Wire, IIC_SDA, IIC_SCL);
  if (!rtcOk) Serial.println("RTC PCF85063 nicht gefunden");
  imuOk = imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (imuOk) {
    imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_62_5Hz, SensorQMI8658::LPF_MODE_0);
    imu.enableAccelerometer();
  } else {
    Serial.println("Lagesensor QMI8658 nicht gefunden - kein Schuetteln");
  }

  fb = (uint16_t *)ps_malloc(LCD_WIDTH * LCD_HEIGHT * 2);
  if (!fb) { Serial.println("PSRAM fehlt - PSRAM=opi eingestellt?"); while (true) delay(1000); }
  if (!panel->begin(80000000)) Serial.println("Display-Start fehlgeschlagen");
  panel->fillScreen(0x0000);
  panel->setBrightness(0);

  touch.setPins(TP_RESET, TP_INT);
  bool ok = false;
  for (int i = 0; i < 3 && !ok; i++) { ok = touch.begin(Wire, 0x5A, IIC_SDA, IIC_SCL); if (!ok) delay(150); }
  if (!ok) Serial.println("Touch CST9217 nicht gefunden");
  touch.reset();
  touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
  touch.setMirrorXY(true, true);       // Panel ist um 180 Grad gedreht verbaut
  pinMode(TP_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TP_INT), touchIsr, FALLING);

  kb::begin(fb, esp_random());
  net::load();
  kb::Config cfg;
  kb::defaultConfig(cfg);
  const String &js = net::settings().config;
  if (js.length() && !kb::configFromJson(js.c_str(), js.length(), cfg)) Serial.println("Einstellungen kaputt - Standard");
  kb::setConfig(cfg);

  net::begin(FW_VERSION);
  web::begin(FW_VERSION);
  ha::begin();
  ha::setServer(net::settings().haUrl.c_str(), net::settings().haToken.c_str());
  syncClock();
  updateConn();
}

void loop() {
  static uint32_t lastFrame = 0, lastClock = 0, lastConn = 0, fpsT = 0, frames = 0;
  static uint8_t brightNow = 0;
  net::loop();
  web::loop();
  ha::loop();
  readTouch();
  readImu();

  uint32_t now = millis();
  if (now - lastClock > 10000 || !lastClock) { lastClock = now; syncClock(); }
  if (now - lastConn > 500) { lastConn = now; updateConn(); }
  if (now - lastFrame < 40) { delay(1); return; }   // ~25 Bilder/s
  lastFrame = now;

  kb::tick(now, fingers, nFingers);
  panel->draw16bitRGBBitmap(0, 0, fb, LCD_WIDTH, LCD_HEIGHT);

  kb::Action a;
  while (kb::takeAction(a)) ha::call(a);

  uint8_t skin;
  if (kb::takeSkinChange(skin)) {
    static char buf[4096];
    kb::Config c = kb::config();
    c.skin = skin;
    if (kb::configToJson(c, buf, sizeof buf)) net::saveConfig(String(buf));
  }

  uint8_t want = kb::wantBrightness();
  if (want != brightNow) {
    int step = abs(want - brightNow) > 8 ? 4 : 1;   // sanft
    brightNow += want > brightNow ? (want - brightNow < step ? want - brightNow : step) : -(brightNow - want < step ? brightNow - want : step);
    panel->setBrightness(brightNow);
  }

  frames++;
  if (now - fpsT > 30000) {
    kb::Debug d = kb::debug();
    Serial.printf("fps %.1f  seite %d  stimmung %s  ha %d  frei %u/%u\n", frames * 1000.0f / (now - fpsT), d.page,
                  kb::moodName(d.mood), ha::state(), (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    fpsT = now; frames = 0;
  }
}
