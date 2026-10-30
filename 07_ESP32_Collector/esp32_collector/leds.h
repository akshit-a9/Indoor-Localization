#pragma once

// What the RGB status LED is showing.
enum Status {
  STATUS_OFF,        // off
  STATUS_READY,      // green: waiting for the phone / recording saved
  STATUS_COUNTDOWN,  // yellow: get ready, recording starts in a few seconds
  STATUS_RECORDING,  // blue: scanning, stand still
  STATUS_TURN,       // white: turn 45 degrees right now
  STATUS_ERROR,      // red
};

void ledsInit();
void phoneLedSet(bool phoneConnected);
void statusLedSet(Status status);
