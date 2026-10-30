#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Recordings ("takes") on the ESP32's flash, one file per take in /takes.
// Each take is also mirrored to Serial as it is written, as lines starting
// with '@', so a laptop on USB can save it too (see laptop_logger.py).
//
// The files use a compact format to fit ~50 takes in flash; they are turned
// into the phone collector's CSV layout when downloaded.

// Mounts the flash (formatting it on first boot) and deletes unfinished takes.
bool storageInit();

int storageTakeCount(int locationIndex);  // finished takes for LOCATIONS[locationIndex]
size_t storageUsedBytes();
size_t storageTotalBytes();

// Writing one take. Any write failing (flash full) returns false.
bool storageBeginTake(int locationIndex, uint64_t startEpochMs, const String &device);
bool storageAddScan(uint32_t offsetMs, int heading);
bool storageAddAp(const String &bssid, int rssi, int channel, const String &ssid);
bool storageEndTake();    // keep it
void storageAbortTake();  // throw it away

// [{"name": "10_2", "id": 10, "take": 2, "label": "D-152", "kb": 31}, ...]
void storageListTakes(JsonArray out);
bool storageDeleteTake(const String &name);

// Writes every take (or only those of one location id, if onlyId >= 0) as
// CSV, in pieces, through emit().
typedef void (*CsvSink)(const String &piece);
void storageExportCsv(int onlyId, CsvSink emit);
