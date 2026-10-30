#include <Arduino.h>
#include <driver/gpio.h>

#include "config.h"
#include "leds.h"

static void setupPin(int pin) {
  pinMode(pin, OUTPUT);
  // Weakest drive strength (~5 mA): keeps an LED wired without a resistor,
  // and the pin driving it, from being damaged.
  gpio_set_drive_capability((gpio_num_t)pin, GPIO_DRIVE_CAP_0);
}

static void rgbPinWrite(int pin, bool on) {
  if (RGB_COMMON_ANODE) on = !on;  // common anode LEDs light when the pin is LOW
  digitalWrite(pin, on ? HIGH : LOW);
}

static void rgbSet(bool red, bool green, bool blue) {
  rgbPinWrite(PIN_RGB_R, red);
  rgbPinWrite(PIN_RGB_G, green);
  rgbPinWrite(PIN_RGB_B, blue);
}

void ledsInit() {
  setupPin(PIN_PHONE_LED);
  setupPin(PIN_RGB_R);
  setupPin(PIN_RGB_G);
  setupPin(PIN_RGB_B);
  phoneLedSet(false);
  statusLedSet(STATUS_OFF);
}

void phoneLedSet(bool phoneConnected) {
  digitalWrite(PIN_PHONE_LED, phoneConnected ? HIGH : LOW);
}

void statusLedSet(Status status) {
  switch (status) {
    case STATUS_READY:     rgbSet(false, true,  false); break;  // green
    case STATUS_COUNTDOWN: rgbSet(true,  true,  false); break;  // yellow
    case STATUS_RECORDING: rgbSet(false, false, true);  break;  // blue
    case STATUS_TURN:      rgbSet(true,  true,  true);  break;  // white
    case STATUS_ERROR:     rgbSet(true,  false, false); break;  // red
    default:               rgbSet(false, false, false); break;  // off
  }
}
