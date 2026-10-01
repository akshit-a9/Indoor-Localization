#pragma once

#include <Arduino.h>

// Outcome of one scan -> /predict cycle. Lives in a header because the
// Arduino preprocessor hoists function prototypes above in-sketch structs.
struct Result {
  bool ok = false;
  String label;
  String error;
  int classIndex = -1;
  float confidence = -1;
  int apSeen = 0;
  int apDetected = 0;
  int apTotal = 0;
  uint32_t latencyMs = 0;
};
