// Bench bring-up for the localizer hardware. Open Serial Monitor at 115200.
//
//  1. RGB LED cycles red -> green -> blue -> white.
//  2. Scans both I2C buses: expect 0x3C (OLED) on bus 0, 0x68 (MPU6050) on bus 1.
//  3. Prints live MPU6050 readings.
//  4. Cycles the OLED through 6 driver modes, 5 s each. Note the mode number
//     whose picture is solid (no stripes, no garbage column on the right) and
//     set OLED_MODE in localizer/config.h to it.
//
// Pins must match localizer/config.h.

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <driver/gpio.h>

#define PIN_OLED_SDA 21
#define PIN_OLED_SCL 22
#define PIN_IMU_SDA  19
#define PIN_IMU_SCL  18
#define PIN_LED_R 27
#define PIN_LED_G 26
#define PIN_LED_B 25
#define LED_COMMON_ANODE 0
#define LED_NO_RESISTORS 1   // weakest pin drive (~5 mA) + dimmer PWM when no series resistors

static const char *MODE_NAMES[] = {"", "SH1106 NONAME", "SH1106 seqCOM", "SSD1306 NONAME",
                                   "SSD1306 seqCOM", "SH1106 WINSTAR", "SH1106 VCOMH0"};

static U8G2 *oledForMode(int mode) {
  switch (mode) {
    case 3:
    case 4: { static U8G2_SSD1306_128X64_NONAME_F_HW_I2C d(U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA); return &d; }
    case 5: { static U8G2_SH1106_128X64_WINSTAR_F_HW_I2C d(U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA); return &d; }
    case 6: { static U8G2_SH1106_128X64_VCOMH0_F_HW_I2C d(U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA); return &d; }
    default: { static U8G2_SH1106_128X64_NONAME_F_HW_I2C d(U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA); return &d; }
  }
}

static uint8_t oledAddr = 0;
static bool imuFound = false;

static void led(uint8_t r, uint8_t g, uint8_t b) {
  auto w = [](int pin, uint8_t v) {
    if (LED_NO_RESISTORS) v = v * 60 / 255;
    ledcWrite(pin, LED_COMMON_ANODE ? 255 - v : v);
  };
  w(PIN_LED_R, r);
  w(PIN_LED_G, g);
  w(PIN_LED_B, b);
}

static void scanBus(TwoWire &bus, const char *name) {
  Serial.printf("I2C %s:", name);
  int found = 0;
  for (uint8_t a = 1; a < 127; ++a) {
    bus.beginTransmission(a);
    if (bus.endTransmission() == 0) {
      Serial.printf(" 0x%02X", a);
      ++found;
      if (&bus == &Wire && (a == 0x3C || a == 0x3D)) oledAddr = a;
      if (&bus == &Wire1 && (a == 0x68 || a == 0x69)) imuFound = true;
    }
  }
  Serial.println(found ? "" : " nothing found - check wiring/power");
}

static void imuPrint() {
  if (!imuFound) return;
  uint8_t raw[14];
  Wire1.beginTransmission(0x68);
  Wire1.write(0x3B);
  if (Wire1.endTransmission(false) != 0 || Wire1.requestFrom(0x68, 14) != 14) {
    Serial.println("MPU6050 read failed");
    return;
  }
  for (auto &b : raw) b = Wire1.read();
  auto s16 = [&](int i) { return (int16_t)((raw[i] << 8) | raw[i + 1]); };
  Serial.printf("MPU6050 accel g: %+.2f %+.2f %+.2f   gyro dps: %+.1f %+.1f %+.1f\n",
                s16(0) / 16384.0f, s16(2) / 16384.0f, s16(4) / 16384.0f,
                s16(8) / 131.0f, s16(10) / 131.0f, s16(12) / 131.0f);
}

static void showMode(int mode) {
  U8G2 *d = oledForMode(mode);
  d->setI2CAddress(oledAddr * 2);
  d->setBusClock(400000);
  d->begin();
  if (mode == 2 || mode == 4) d->sendF("ca", 0xDA, 0x02);
  d->clearBuffer();
  d->drawFrame(0, 0, 128, 64);
  d->setFont(u8g2_font_logisoso32_tn);
  char num[4];
  snprintf(num, sizeof num, "%d", mode);
  d->drawStr(6, 40, num);
  d->setFont(u8g2_font_6x10_tf);
  d->drawStr(40, 12, MODE_NAMES[mode]);
  d->drawStr(40, 26, "Solid box?");
  d->drawBox(40, 32, 80, 10);    // stripes here = every-other-line problem
  for (int y = 46; y < 62; y += 2) d->drawHLine(40, y, 80);  // fine line pattern
  d->drawVLine(126, 2, 60);      // missing/garbled = wrong column offset
  d->sendBuffer();
  Serial.printf("OLED mode %d: %s\n", mode, MODE_NAMES[mode]);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== hw_check ===");
  for (int pin : {PIN_LED_R, PIN_LED_G, PIN_LED_B}) {
    ledcAttach(pin, 5000, 8);
    if (LED_NO_RESISTORS) gpio_set_drive_capability((gpio_num_t)pin, GPIO_DRIVE_CAP_0);
  }

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL, 400000);
  Wire1.begin(PIN_IMU_SDA, PIN_IMU_SCL, 400000);
  scanBus(Wire, "bus0 (OLED, SDA21/SCL22)");
  scanBus(Wire1, "bus1 (IMU,  SDA19/SCL18)");

  if (imuFound) {
    Wire1.beginTransmission(0x68);
    Wire1.write(0x75);
    Wire1.endTransmission(false);
    Wire1.requestFrom(0x68, 1);
    Serial.printf("MPU6050 WHO_AM_I = 0x%02X (0x68 genuine; 0x70/0x72/0x98 are clones, still usable)\n", Wire1.read());
    Wire1.beginTransmission(0x68);  // wake up
    Wire1.write(0x6B);
    Wire1.write(0x00);
    Wire1.endTransmission();
  } else {
    Serial.println("MPU6050 NOT found on bus1");
  }
  if (!oledAddr) Serial.println("OLED NOT found on bus0 - display test skipped");
}

void loop() {
  const uint8_t colours[][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}, {0, 0, 0}};
  for (auto &c : colours) {
    led(c[0], c[1], c[2]);
    delay(250);
  }
  for (int mode = 1; mode <= 6; ++mode) {
    if (oledAddr) showMode(mode);
    for (int i = 0; i < 5; ++i) {
      imuPrint();
      delay(1000);
    }
  }
}
