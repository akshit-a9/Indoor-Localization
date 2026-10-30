#pragma once

#include <Arduino.h>

void displayInit();

// Title on the top line, then a message wrapped over the lines below.
void displayMessage(const char *title, const String &message);

// Idle screen: how to connect the phone, and progress.
void displayReady(int done, int total);

// Big countdown before a recording starts.
void displayCountdown(const char *label, int secondsLeft);

// Recording screen: seconds left, which of the 8 directions we face, and a
// "TURN RIGHT" banner when it is time to turn.
void displayRecording(const char *label, int secondsLeft, int heading, int scans, bool turnNow);
