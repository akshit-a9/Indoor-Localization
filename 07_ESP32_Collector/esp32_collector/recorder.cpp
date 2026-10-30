#include <WiFi.h>

#include "config.h"
#include "locations.h"
#include "recorder.h"
#include "storage.h"

static RecorderInfo info;
static uint32_t phaseStart;     // millis() when the countdown / recording began
static uint64_t epochAtStart;   // phone time when the countdown began
static bool scanRunning;
static uint32_t scanStartedMs;  // ms into the recording
static int scanHeading;

const RecorderInfo &recorderInfo() { return info; }

static void fail(const String &error) {
  Serial.printf("Recording failed: %s\n", error.c_str());
  storageAbortTake();
  info.state = REC_ERROR;
  info.error = error;
}

bool recorderStart(int locationIndex, uint64_t phoneEpochMs, String &error) {
  if (info.state == REC_COUNTDOWN || info.state == REC_RECORDING) {
    error = "A recording is already running";
    return false;
  }
  if (locationIndex < 0 || locationIndex >= LOCATION_COUNT) {
    error = "Unknown location";
    return false;
  }
  if (storageTotalBytes() - storageUsedBytes() < MIN_FREE_BYTES) {
    error = "Storage full. Download the data, then delete old takes.";
    return false;
  }

  info = RecorderInfo();
  info.state = REC_COUNTDOWN;
  info.location = locationIndex;
  info.msLeft = COUNTDOWN_MS;
  phaseStart = millis();
  epochAtStart = phoneEpochMs;
  Serial.printf("Countdown for %s\n", LOCATIONS[locationIndex].label);
  return true;
}

void recorderCancel() {
  if (info.state == REC_RECORDING) storageAbortTake();
  if (info.state == REC_COUNTDOWN || info.state == REC_RECORDING) Serial.println("Recording cancelled");
  info = RecorderInfo();
}

void recorderAcknowledge() {
  if (info.state == REC_DONE || info.state == REC_ERROR) info = RecorderInfo();
}

static void beginRecording() {
  uint64_t epoch = epochAtStart + (millis() - phaseStart);
  if (!storageBeginTake(info.location, epoch, WiFi.macAddress())) {
    fail("Could not create the file. Is the storage full?");
    return;
  }
  info.state = REC_RECORDING;
  phaseStart = millis();
  Serial.printf("Recording %s\n", LOCATIONS[info.location].label);
}

// Saves the scan that just finished. Returns false if writing failed.
static bool saveScan(int found) {
  if (!storageAddScan(scanStartedMs, scanHeading)) return false;
  for (int i = 0; i < found; ++i) {
    String bssid = WiFi.BSSIDstr(i);
    bssid.toLowerCase();  // the phone data uses lowercase
    if (!storageAddAp(bssid, WiFi.RSSI(i), WiFi.channel(i), WiFi.SSID(i))) return false;
  }
  info.scans++;
  info.lastAps = found;
  return true;
}

static void startScan() {
  // async, include hidden networks, active scan: same as WayPoint's scanner
  if (WiFi.scanNetworks(true, true, false, SCAN_MS_PER_CHANNEL) == WIFI_SCAN_FAILED) return;
  scanRunning = true;
  scanStartedMs = millis() - phaseStart;
  scanHeading = info.heading;
}

static void finish() {
  if (!storageEndTake()) {
    fail("Could not save the file");
    return;
  }
  info.state = REC_DONE;
  info.msLeft = 0;
  Serial.printf("Saved %d scans of %s\n", info.scans, LOCATIONS[info.location].label);
}

void recorderLoop() {
  // Results of a scan we no longer want (cancelled or finished meanwhile)
  int found = WiFi.scanComplete();
  if (info.state != REC_RECORDING) {
    if (found >= 0) WiFi.scanDelete();
    scanRunning = found == WIFI_SCAN_RUNNING;
  }

  uint32_t elapsed = millis() - phaseStart;

  if (info.state == REC_COUNTDOWN) {
    if (elapsed >= COUNTDOWN_MS) beginRecording();
    else info.msLeft = COUNTDOWN_MS - elapsed;
    return;
  }
  if (info.state != REC_RECORDING) return;

  if (elapsed >= SESSION_MS) {
    finish();
    return;
  }
  info.msLeft = SESSION_MS - elapsed;

  int heading = elapsed * HEADINGS / SESSION_MS;
  if (heading != info.heading) {
    info.heading = heading;
    info.turnedAt = millis();
    Serial.printf("Turn 45 degrees right (facing %d/%d)\n", heading + 1, HEADINGS);
  }

  if (scanRunning && found >= 0) {
    scanRunning = false;
    bool saved = saveScan(found);
    WiFi.scanDelete();
    if (!saved) {
      fail("Storage full while recording");
      return;
    }
    Serial.printf("Scan %d: %d APs\n", info.scans, found);
  } else if (scanRunning && found == WIFI_SCAN_FAILED) {
    scanRunning = false;  // try again below
  }

  if (!scanRunning) startScan();
}
