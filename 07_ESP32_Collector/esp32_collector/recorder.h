#pragma once

#include <Arduino.h>

// One recording: countdown, then SESSION_MS of back-to-back WiFi scans while
// the person turns 45 degrees right every SESSION_MS / HEADINGS.
enum RecorderState {
  REC_IDLE,       // waiting for the phone to start one
  REC_COUNTDOWN,  // "get ready"
  REC_RECORDING,  // scanning
  REC_DONE,       // saved; shown until the phone moves on
  REC_ERROR,      // something failed; shown until the phone moves on
};

struct RecorderInfo {
  RecorderState state = REC_IDLE;
  int location = -1;      // index into LOCATIONS
  uint32_t msLeft = 0;    // of the countdown or of the recording
  int heading = 0;        // 0 .. HEADINGS-1: how many times they have turned
  uint32_t turnedAt = 0;  // millis() of the last turn cue, 0 if none yet
  int scans = 0;          // scans saved in this recording
  int lastAps = 0;        // APs heard by the last scan
  String error;
};

const RecorderInfo &recorderInfo();

// phoneEpochMs: the phone's local time, so the CSV timestamps make sense.
bool recorderStart(int locationIndex, uint64_t phoneEpochMs, String &error);
void recorderCancel();       // throws the recording away
void recorderAcknowledge();  // DONE / ERROR -> IDLE
void recorderLoop();         // call on every loop()
