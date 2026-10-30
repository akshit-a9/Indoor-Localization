#pragma once

// ---------------------------------------------------------------------------
// Pins: same wiring as the WayPoint device (GPIO numbers, "D21" = GPIO21)
// ---------------------------------------------------------------------------
#define PIN_OLED_SDA 21
#define PIN_OLED_SCL 22

// Phone LED: ON while a phone is connected to the collector's WiFi.
// GPIO2 is also the small blue LED built into most ESP32 DevKits.
#define PIN_PHONE_LED 2

// Status LED: RGB LED showing what the collector is doing.
#define PIN_RGB_R 27
#define PIN_RGB_G 26
#define PIN_RGB_B 25

// 0 = common cathode (long leg to GND), 1 = common anode (long leg to 3V3)
#define RGB_COMMON_ANODE 0

// Our panel only lights every other pixel row; 1 copies each row onto the
// one below. Set to 0 on a healthy display.
#define OLED_ROW_DOUBLE 1

// ---------------------------------------------------------------------------
// The collector's own WiFi network, which the phone joins
// ---------------------------------------------------------------------------
#define AP_SSID     "WayPoint-Collect"
#define AP_PASSWORD "waypoint"  // at least 8 characters
#define AP_CHANNEL  1

// ---------------------------------------------------------------------------
// Recording protocol (matches the phone collection: 90 s, one full turn)
// ---------------------------------------------------------------------------
#define SESSION_MS     90000  // length of one recording
#define HEADINGS       8      // turn 45 degrees right this many times per recording
#define COUNTDOWN_MS   5000   // "get ready" time after pressing Start
#define TURN_CUE_MS    3000   // how long the "turn now" cue stays up

// Same scan settings as the WayPoint firmware, so the recorded RSSIs match
// what the device will see when it localizes.
#define SCAN_MS_PER_CHANNEL 120

// Refuse to start a recording with less free flash than this (one take is ~40 KB).
#define MIN_FREE_BYTES (96 * 1024)

#define SERIAL_BAUD 115200
