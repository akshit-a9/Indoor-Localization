#pragma once

// ---------------------------------------------------------------------------
// Pins (ESP32 DevKit / WROOM-32). See ESP32/README.md for the wiring table.
// GPIO numbers; boards like the ESP32-32X print them as Dxx (21 = D21).
// ---------------------------------------------------------------------------
#define PIN_OLED_SDA 21   // I2C bus 0 (Wire)  -> OLED
#define PIN_OLED_SCL 22
#define PIN_IMU_SDA  19   // I2C bus 1 (Wire1) -> MPU6050, kept separate so
#define PIN_IMU_SCL  18   //   OLED refreshes never stall IMU sampling

#define PIN_LED_R 27
#define PIN_LED_G 26
#define PIN_LED_B 25

// 0 = common cathode (common pin to GND), 1 = common anode (common pin to 3V3)
#define LED_COMMON_ANODE 0
// 1 = LED wired straight to the pins with no series resistors: the pins are
// set to their weakest drive strength (~5 mA) so the LED and GPIO stay safe.
// Set to 0 once 100-1k resistors are fitted.
#define LED_NO_RESISTORS 1
#define LED_BRIGHTNESS   60    // 0-255, PWM duty
#define LED_PULSE_MS     800   // boot/health status flash
#define LED_SCAN_BLINK_MS 150  // blue blink at the start of each scan; the
                               // result colour (green/red) is held otherwise

// ---------------------------------------------------------------------------
// OLED. Run ESP32/hw_check first: it cycles through these modes and prints
// the number of each one. Put the number of the cleanest one here.
//   1 SH1106 NONAME      2 SH1106 NONAME + sequential COM
//   3 SSD1306 NONAME     4 SSD1306 NONAME + sequential COM
//   5 SH1106 WINSTAR     6 SH1106 VCOMH0
// ---------------------------------------------------------------------------
#define OLED_MODE 1
#define OLED_I2C_ADDR 0x3C   // 0x3D on some boards (hw_check prints it)
// Smear every lit pixel one row down so text survives a panel that only
// shows every other line. Set to 0 once the display is fixed.
#define OLED_ROW_DOUBLE 1

// ---------------------------------------------------------------------------
// Localization API
// ---------------------------------------------------------------------------
#define API_MODEL          "Random Forest 2.4GHz"  // ESP32 cannot hear 5 GHz APs
#define SCAN_INTERVAL_MS   4000   // min time between scan starts
#define SCAN_MS_PER_CHAN   120    // active-scan dwell per channel
#define HTTP_TIMEOUT_MS    8000
#define API_PORT           8000   // used when probing the subnet for the API
#define API_PROBE_MS       60     // TCP connect timeout per host while probing
#define RSSI_OFFSET_DB     0      // added to every RSSI (ESP32 vs phone calibration)
#define MIN_CONFIDENCE     0.0f   // >0: predictions below this light the LED red

// ---------------------------------------------------------------------------
// IMU (MPU6050) streaming over serial for the navigation dataset
// ---------------------------------------------------------------------------
#define IMU_ENABLED  1
#define IMU_RATE_HZ  100
#define IMU_ADDR     0x68   // 0x69 if AD0 is tied to 3V3

#define SERIAL_BAUD  460800
