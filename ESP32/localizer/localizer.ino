// ESP32 client for localization_api.
//
// Every SCAN_INTERVAL_MS: scan 2.4 GHz WiFi -> POST /predict -> show the
// predicted location on the OLED and pulse the RGB LED (blue while scanning,
// green on a prediction, red on any error).
//
// In parallel an MPU6050 task streams accelerometer + gyro samples over serial
// for the navigation dataset. Serial output is line-based CSV, one record type
// per prefix (see ESP32/README.md); ESP32/tools/serial_logger.py records it.

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <ArduinoJson.h>
#include <driver/gpio.h>

#include "config.h"
#include "result.h"
#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "secrets.h not found - using secrets.example.h placeholders"
#include "secrets.example.h"
#endif

// ---------------------------------------------------------------------------
// RGB LED
// ---------------------------------------------------------------------------
static uint32_t ledOffAt = 0;
static uint8_t ledHeld[3] = {0, 0, 0};  // colour of the latest result, shown between blinks

static void ledWriteChannel(int pin, uint8_t value) {
  uint32_t duty = (uint32_t)value * LED_BRIGHTNESS / 255;
  ledcWrite(pin, LED_COMMON_ANODE ? 255 - duty : duty);
}

static void ledSet(uint8_t r, uint8_t g, uint8_t b) {
  ledWriteChannel(PIN_LED_R, r);
  ledWriteChannel(PIN_LED_G, g);
  ledWriteChannel(PIN_LED_B, b);
}

// Brief colour, then back to the held result colour (see loop()).
static void ledPulse(uint8_t r, uint8_t g, uint8_t b) {
  ledSet(r, g, b);
  ledOffAt = millis() + LED_PULSE_MS;
}

// Steady colour that stays on until the next result.
static void ledHold(uint8_t r, uint8_t g, uint8_t b) {
  ledHeld[0] = r;
  ledHeld[1] = g;
  ledHeld[2] = b;
  ledOffAt = 0;
  ledSet(r, g, b);
}

static void ledRestore() { ledSet(ledHeld[0], ledHeld[1], ledHeld[2]); }

static void ledInit() {
  for (int pin : {PIN_LED_R, PIN_LED_G, PIN_LED_B}) {
    ledcAttach(pin, 5000, 8);
    if (LED_NO_RESISTORS) gpio_set_drive_capability((gpio_num_t)pin, GPIO_DRIVE_CAP_0);
  }
  const uint8_t seq[][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
  for (auto &c : seq) {
    ledSet(c[0], c[1], c[2]);
    delay(150);
  }
  ledSet(0, 0, 0);
}

// ---------------------------------------------------------------------------
// OLED (mode numbers match ESP32/hw_check)
// ---------------------------------------------------------------------------
static U8G2 *oled = nullptr;

static U8G2 *oledForMode(int mode) {
  switch (mode) {
    case 3:
    case 4: { static U8G2_SSD1306_128X64_NONAME_F_HW_I2C d(U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA); return &d; }
    case 5: { static U8G2_SH1106_128X64_WINSTAR_F_HW_I2C d(U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA); return &d; }
    case 6: { static U8G2_SH1106_128X64_VCOMH0_F_HW_I2C d(U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA); return &d; }
    default: { static U8G2_SH1106_128X64_NONAME_F_HW_I2C d(U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA); return &d; }
  }
}

static void oledInit() {
  oled = oledForMode(OLED_MODE);
  oled->setI2CAddress(OLED_I2C_ADDR * 2);
  oled->setBusClock(400000);
  oled->begin();
  if (OLED_MODE == 2 || OLED_MODE == 4) oled->sendF("ca", 0xDA, 0x02);  // sequential COM pins
  oled->setFont(u8g2_font_6x10_tf);
  oled->setFontPosTop();
}

// Copy each lit pixel one row down (page-based buffer, bit 0 = top row).
static void oledRowDouble() {
  uint8_t *buf = oled->getBufferPtr();
  const int w = oled->getBufferTileWidth() * 8;
  for (int p = oled->getBufferTileHeight() - 1; p >= 0; --p) {
    for (int x = 0; x < w; ++x) {
      uint8_t b = buf[p * w + x];
      uint8_t carry = p > 0 ? buf[(p - 1) * w + x] >> 7 : 0;
      buf[p * w + x] = b | (b << 1) | carry;
    }
  }
}

static void oledShow() {
  if (OLED_ROW_DOUBLE) oledRowDouble();
  oled->sendBuffer();
}

// Greedy word wrap into at most maxLines lines of maxChars.
static int wrapText(const String &text, String *lines, int maxLines, int maxChars) {
  int n = 0;
  String cur;
  int i = 0;
  while (i < (int)text.length() && n < maxLines) {
    int sp = text.indexOf(' ', i);
    String word = text.substring(i, sp < 0 ? text.length() : sp);
    i = sp < 0 ? text.length() : sp + 1;
    while ((int)word.length() > maxChars) {  // hard-break very long words
      if (cur.length()) { lines[n++] = cur; cur = ""; if (n >= maxLines) return n; }
      lines[n++] = word.substring(0, maxChars);
      word = word.substring(maxChars);
      if (n >= maxLines) return n;
    }
    String next = cur.length() ? cur + " " + word : word;
    if ((int)next.length() <= maxChars) {
      cur = next;
    } else {
      lines[n++] = cur;
      cur = word;
    }
  }
  bool truncated = i < (int)text.length();
  if (cur.length()) {
    if (n < maxLines) lines[n++] = cur;
    else truncated = true;
  }
  if (truncated) lines[n - 1].setCharAt(lines[n - 1].length() - 1, '~');
  return n;
}

static void oledMessage(const char *title, const String &body) {
  oled->clearBuffer();
  oled->drawStr(0, 0, title);
  oled->drawHLine(0, 11, 128);
  String lines[4];
  int n = wrapText(body, lines, 4, 21);
  for (int i = 0; i < n; ++i) oled->drawStr(0, 14 + i * 12, lines[i].c_str());
  oledShow();
}

// ---------------------------------------------------------------------------
// MPU6050 on Wire1, sampled by its own FreeRTOS task
// ---------------------------------------------------------------------------
static bool imuOk = false;

static bool imuWrite(uint8_t reg, uint8_t val) {
  Wire1.beginTransmission(IMU_ADDR);
  Wire1.write(reg);
  Wire1.write(val);
  return Wire1.endTransmission() == 0;
}

static bool imuRead(uint8_t reg, uint8_t *buf, int n) {
  Wire1.beginTransmission(IMU_ADDR);
  Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0) return false;
  if (Wire1.requestFrom((int)IMU_ADDR, n) != n) return false;
  for (int i = 0; i < n; ++i) buf[i] = Wire1.read();
  return true;
}

static bool imuInit() {
  Wire1.begin(PIN_IMU_SDA, PIN_IMU_SCL, 400000);
  uint8_t who = 0;
  if (!imuRead(0x75, &who, 1)) return false;
  Serial.printf("# IMU WHO_AM_I=0x%02X\n", who);  // 0x68 genuine; clones differ
  return imuWrite(0x6B, 0x01)     // wake, PLL on gyro X
      && imuWrite(0x1A, 0x03)     // DLPF ~44 Hz
      && imuWrite(0x19, 0x09)     // 1 kHz / (1 + 9) = 100 Hz internal rate
      && imuWrite(0x1B, 0x08)     // gyro +-500 dps  -> 65.5 LSB/dps
      && imuWrite(0x1C, 0x08);    // accel +-4 g     -> 8192 LSB/g
}

static void imuTask(void *) {
  TickType_t last = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(1000 / IMU_RATE_HZ);
  uint8_t raw[14];
  for (;;) {
    vTaskDelayUntil(&last, period);
    uint64_t t = esp_timer_get_time();
    if (!imuRead(0x3B, raw, 14)) {
      Serial.printf("ERR,%llu,imu_read\n", (unsigned long long)t);
      continue;
    }
    auto s16 = [&](int i) { return (int16_t)((raw[i] << 8) | raw[i + 1]); };
    Serial.printf("IMU,%llu,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f\n", (unsigned long long)t,
                  s16(0) / 8192.0f, s16(2) / 8192.0f, s16(4) / 8192.0f,
                  s16(8) / 65.5f, s16(10) / 65.5f, s16(12) / 65.5f);
  }
}

// ---------------------------------------------------------------------------
// Localization cycle
// ---------------------------------------------------------------------------
static uint32_t cycle = 0;
static uint32_t lastScanStart = 0;
static Result last;

static unsigned long long nowUs() { return (unsigned long long)esp_timer_get_time(); }

static void drawResult(const Result &r, bool scanning) {
  char buf[32];
  oled->clearBuffer();
  snprintf(buf, sizeof buf, "#%lu %s", (unsigned long)cycle, scanning ? "scanning" : (r.ok ? "ok" : "ERROR"));
  oled->drawStr(0, 0, buf);
  if (r.ok && r.confidence >= 0) {
    snprintf(buf, sizeof buf, "%d%%", (int)lroundf(r.confidence * 100));
    oled->drawStr(128 - oled->getStrWidth(buf), 0, buf);
  }
  oled->drawHLine(0, 11, 128);

  String lines[3];
  int n = wrapText(r.ok ? r.label : r.error, lines, 3, 21);
  for (int i = 0; i < n; ++i) oled->drawStr(0, 14 + i * 12, lines[i].c_str());

  snprintf(buf, sizeof buf, "AP %d/%d %lums%s", r.apDetected, r.apTotal,
           (unsigned long)r.latencyMs, imuOk ? " IMU" : "");
  oled->drawStr(0, 54, buf);
  oledShow();
}

static Result localize() {
  Result r;
  int n = WiFi.scanNetworks(false, true, false, SCAN_MS_PER_CHAN);
  if (n < 0) {
    r.error = "WiFi scan failed";
    return r;
  }
  r.apSeen = n;

  JsonDocument req;
  req["model"] = API_MODEL;
  JsonArray scans = req["scans"].to<JsonArray>();
  unsigned long long t = nowUs();
  Serial.printf("SCAN,%llu,%lu,%d\n", t, (unsigned long)cycle, n);
  for (int i = 0; i < n; ++i) {
    String bssid = WiFi.BSSIDstr(i);
    int rssi = WiFi.RSSI(i) + RSSI_OFFSET_DB;
    JsonObject o = scans.add<JsonObject>();
    o["bssid"] = bssid;
    o["rssi"] = rssi;
    Serial.printf("AP,%llu,%lu,%s,%d,%d\n", t, (unsigned long)cycle, bssid.c_str(), rssi, WiFi.channel(i));
  }
  WiFi.scanDelete();
  String body;
  serializeJson(req, body);

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.begin(String(API_BASE_URL) + "/predict");
  http.addHeader("Content-Type", "application/json");
  uint32_t t0 = millis();
  int code = http.POST(body);
  r.latencyMs = millis() - t0;
  String payload = code > 0 ? http.getString() : "";
  http.end();

  if (code != 200) {
    r.error = code > 0 ? "API HTTP " + String(code) : "API unreachable: " + HTTPClient::errorToString(code);
    return r;
  }

  JsonDocument res;
  if (deserializeJson(res, payload)) {
    r.error = "Bad JSON from API";
    return r;
  }
  r.label = res["label"].as<String>();
  r.classIndex = res["class_index"] | -1;
  r.apDetected = res["ap_detected"] | 0;
  r.apTotal = res["ap_total"] | 0;
  JsonArrayConst probs = res["probabilities"];
  if (!probs.isNull() && r.classIndex >= 0) r.confidence = probs[r.classIndex];

  if (r.apDetected == 0) {
    r.error = "No known APs in scan";
  } else if (MIN_CONFIDENCE > 0 && r.confidence >= 0 && r.confidence < MIN_CONFIDENCE) {
    r.error = "Low confidence: " + r.label;
  } else {
    r.ok = true;
  }
  return r;
}

static void apiHealthCheck() {
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.begin(String(API_BASE_URL) + "/health");
  int code = http.GET();
  String payload = code > 0 ? http.getString() : "";
  http.end();

  JsonDocument doc;
  if (code != 200 || deserializeJson(doc, payload)) {
    Serial.printf("# API health failed: %d\n", code);
    oledMessage("API not reachable", String(API_BASE_URL));
    ledPulse(255, 0, 0);
    delay(2000);
    return;
  }
  bool hasModel = false;
  for (JsonVariantConst m : doc["loaded_models"].as<JsonArrayConst>()) hasModel |= m == API_MODEL;
  String msg = String(doc["label_count"] | 0) + " locations, " + String(doc["feature_count"] | 0) + " APs. " +
               (hasModel ? "Model OK" : "MISSING " API_MODEL);
  Serial.printf("# API health: %s\n", msg.c_str());
  oledMessage(hasModel ? "API ok" : "API: model missing", msg);
  ledPulse(hasModel ? 0 : 255, hasModel ? 255 : 0, 0);
  delay(1500);
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.setTxBufferSize(4096);
  Serial.begin(SERIAL_BAUD);
  delay(200);
  Serial.println("# localizer boot");
  Serial.println("# IMU,t_us,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps");
  Serial.println("# SCAN,t_us,cycle,n_aps | AP,t_us,cycle,bssid,rssi,channel");
  Serial.println("# PRED,t_us,cycle,ok,class_index,confidence,ap_detected,ap_seen,latency_ms,\"label_or_error\"");

  ledInit();
  oledInit();
  oledMessage("Indoor Localizer", "Starting...");

  if (IMU_ENABLED) {
    imuOk = imuInit();
    Serial.printf("# IMU %s\n", imuOk ? "ok" : "NOT FOUND");
    if (imuOk) xTaskCreatePinnedToCore(imuTask, "imu", 4096, nullptr, 3, nullptr, 1);
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  oledMessage("Connecting WiFi", WIFI_SSID);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(250);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("# WiFi ok, IP %s\n", WiFi.localIP().toString().c_str());
    oledMessage("WiFi ok", WiFi.localIP().toString());
    delay(800);
    apiHealthCheck();
  } else {
    Serial.println("# WiFi connect timed out");
  }
}

void loop() {
  if (ledOffAt && (int32_t)(millis() - ledOffAt) >= 0) {
    ledRestore();
    ledOffAt = 0;
  }

  if (WiFi.status() != WL_CONNECTED) {
    static uint32_t lastWarn = 0;
    if (millis() - lastWarn > 3000) {
      lastWarn = millis();
      Serial.printf("ERR,%llu,wifi_disconnected\n", nowUs());
      oledMessage("WiFi disconnected", String("Retrying ") + WIFI_SSID);
      ledHold(255, 0, 0);
    }
    delay(50);
    return;
  }

  if (cycle > 0 && millis() - lastScanStart < SCAN_INTERVAL_MS) {
    delay(10);
    return;
  }
  lastScanStart = millis();
  ++cycle;

  // Short blue blink marks a new scan; the previous result colour stays on
  // while the scan runs (~3.5 s when connected), then switches to the new one.
  ledSet(0, 0, 255);
  delay(LED_SCAN_BLINK_MS);
  ledRestore();
  drawResult(last, true);
  last = localize();
  drawResult(last, false);
  if (last.ok) ledHold(0, 255, 0);
  else ledHold(255, 0, 0);

  Serial.printf("PRED,%llu,%lu,%d,%d,%.4f,%d,%d,%lu,\"%s\"\n", nowUs(), (unsigned long)cycle, last.ok,
                last.classIndex, last.confidence, last.apDetected, last.apSeen,
                (unsigned long)last.latencyMs, (last.ok ? last.label : last.error).c_str());
}
