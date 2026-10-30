#include <LittleFS.h>
#include <time.h>
#include <algorithm>
#include <vector>

#include "storage.h"
#include "locations.h"

// Take file format, one record per line:
//   H,<location id>,<take>,<start epoch ms>,<device>   header, first line
//   S,<ms since start>,<heading>                       a scan begins
//   A,<bssid>,<rssi>,<channel>,<ssid>                  one AP of that scan
// The SSID is last so commas inside it need no escaping.
static const char *TAKES_DIR = "/takes";

// The phone collector's columns, then ours. Preprocessing only needs BSSID/RSSI.
static const char *CSV_HEADER =
    "Timestamp,SSID,BSSID,RSSI,Frequency,Activity Label,Location Label,Take,Scan,Heading,Device\n";

static int takeCounts[LOCATION_COUNT];

// The take being written
static File current;
static String currentPath;  // /takes/<id>_<take>.tmp until it is finished
static int currentIndex, currentTake, currentScan, currentHeading;
static uint64_t currentEpochMs, currentScanEpochMs;
static String currentDevice;

static int indexForId(int id) {
  for (int i = 0; i < LOCATION_COUNT; ++i)
    if (LOCATIONS[i].id == id) return i;
  return -1;
}

// "10_2.txt" -> id 10, take 2
static bool parseName(const String &name, int &id, int &take, const char *ext = ".txt") {
  int sep = name.indexOf('_');
  if (sep <= 0 || !name.endsWith(ext)) return false;
  id = name.substring(0, sep).toInt();
  take = name.substring(sep + 1).toInt();
  return id > 0 && take > 0;
}

static std::vector<String> fileNames() {
  std::vector<String> names;
  File dir = LittleFS.open(TAKES_DIR);
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) names.push_back(f.name());
  return names;
}

// Finished take files sorted by location id, then take.
static std::vector<String> takeNames() {
  std::vector<String> names;
  int id, take;
  for (const String &name : fileNames())
    if (parseName(name, id, take)) names.push_back(name);
  std::sort(names.begin(), names.end(), [](const String &a, const String &b) {
    int ida, ta, idb, tb;
    parseName(a, ida, ta);
    parseName(b, idb, tb);
    return ida != idb ? ida < idb : ta < tb;
  });
  return names;
}

bool storageInit() {
  if (!LittleFS.begin(true)) return false;  // true: format if it won't mount
  LittleFS.mkdir(TAKES_DIR);

  int id, take;
  for (const String &name : fileNames()) {
    if (parseName(name, id, take, ".tmp")) {
      Serial.printf("Deleting unfinished take %s\n", name.c_str());
      LittleFS.remove(String(TAKES_DIR) + "/" + name);
    } else if (parseName(name, id, take) && indexForId(id) >= 0) {
      takeCounts[indexForId(id)]++;
    }
  }
  return true;
}

int storageTakeCount(int locationIndex) { return takeCounts[locationIndex]; }
size_t storageUsedBytes() { return LittleFS.usedBytes(); }
size_t storageTotalBytes() { return LittleFS.totalBytes(); }

// ---------------------------------------------------------------------------
// CSV rows, shared by the download and the Serial mirror
// ---------------------------------------------------------------------------

static String csvField(const String &text) {
  if (text.indexOf(',') < 0 && text.indexOf('"') < 0) return text;
  String quoted = text;
  quoted.replace("\"", "\"\"");
  return "\"" + quoted + "\"";
}

// Local time as the phone wrote it, e.g. 2026-03-16T14:43:19.061000
static String isoTime(uint64_t epochMs) {
  time_t seconds = epochMs / 1000;
  struct tm t;
  gmtime_r(&seconds, &t);  // the phone already sent its local time as the epoch
  char text[32];
  snprintf(text, sizeof text, "%04d-%02d-%02dT%02d:%02d:%02d.%03d000", t.tm_year + 1900, t.tm_mon + 1,
           t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, (int)(epochMs % 1000));
  return text;
}

static int frequencyMhz(int channel) { return channel == 14 ? 2484 : 2407 + 5 * channel; }

static String csvRow(uint64_t epochMs, const String &ssid, const String &bssid, int rssi, int channel,
                     int locationIndex, int take, int scan, int heading, const String &device) {
  String row;
  row.reserve(128);
  row += isoTime(epochMs);
  row += ',' + csvField(ssid) + ',' + bssid + ',' + rssi + ',' + frequencyMhz(channel) + ",STATIONARY,";
  row += csvField(LOCATIONS[locationIndex].label);
  row += ',' + String(take) + ',' + scan + ',' + heading + ',' + device + '\n';
  return row;
}

// ---------------------------------------------------------------------------
// Writing a take
// ---------------------------------------------------------------------------

static bool writeLine(const String &line) {
  return current.print(line) == line.length();
}

bool storageBeginTake(int locationIndex, uint64_t startEpochMs, const String &device) {
  int id = LOCATIONS[locationIndex].id;
  int take = 1, existingId, existingTake;
  for (const String &name : takeNames())
    if (parseName(name, existingId, existingTake) && existingId == id) take = max(take, existingTake + 1);

  currentPath = String(TAKES_DIR) + "/" + id + "_" + take + ".tmp";
  current = LittleFS.open(currentPath, "w");
  if (!current) return false;

  currentIndex = locationIndex;
  currentTake = take;
  currentScan = 0;
  currentEpochMs = startEpochMs;
  currentDevice = device;
  Serial.printf("@take_start,%d,%d\n", id, take);
  return writeLine("H," + String(id) + "," + take + "," + String(startEpochMs) + "," + device + "\n");
}

bool storageAddScan(uint32_t offsetMs, int heading) {
  currentScan++;
  currentHeading = heading;
  currentScanEpochMs = currentEpochMs + offsetMs;
  return writeLine("S," + String(offsetMs) + "," + heading + "\n");
}

bool storageAddAp(const String &bssid, int rssi, int channel, const String &ssid) {
  String cleanSsid = ssid;
  cleanSsid.replace("\n", " ");
  cleanSsid.replace("\r", " ");
  Serial.print("@row," + csvRow(currentScanEpochMs, cleanSsid, bssid, rssi, channel, currentIndex, currentTake,
                                currentScan, currentHeading, currentDevice));
  return writeLine("A," + bssid + "," + rssi + "," + channel + "," + cleanSsid + "\n");
}

bool storageEndTake() {
  current.close();
  String finalPath = currentPath.substring(0, currentPath.length() - 4) + ".txt";
  if (!LittleFS.rename(currentPath, finalPath)) return false;
  takeCounts[currentIndex]++;
  Serial.printf("@take_end,%d,%d,saved\n", LOCATIONS[currentIndex].id, currentTake);
  return true;
}

void storageAbortTake() {
  if (current) current.close();
  LittleFS.remove(currentPath);
  Serial.printf("@take_end,%d,%d,cancelled\n", LOCATIONS[currentIndex].id, currentTake);
}

// ---------------------------------------------------------------------------
// Listing, deleting, exporting
// ---------------------------------------------------------------------------

void storageListTakes(JsonArray out) {
  int id, take;
  for (const String &name : takeNames()) {
    parseName(name, id, take);
    int index = indexForId(id);
    File f = LittleFS.open(String(TAKES_DIR) + "/" + name);
    JsonObject entry = out.add<JsonObject>();
    entry["name"] = name.substring(0, name.length() - 4);
    entry["id"] = id;
    entry["take"] = take;
    entry["label"] = index >= 0 ? LOCATIONS[index].label : "?";
    entry["kb"] = (f.size() + 1023) / 1024;
  }
}

bool storageDeleteTake(const String &name) {
  int id, take;
  String file = name + ".txt";
  if (!parseName(file, id, take) || !LittleFS.remove(String(TAKES_DIR) + "/" + file)) return false;
  int index = indexForId(id);
  if (index >= 0 && takeCounts[index] > 0) takeCounts[index]--;
  return true;
}

// Splits "a,b,c" at the first `count` commas; the last field keeps the rest.
static int splitFields(const String &line, String *fields, int count) {
  int start = 0, n = 0;
  while (n < count - 1) {
    int comma = line.indexOf(',', start);
    if (comma < 0) break;
    fields[n++] = line.substring(start, comma);
    start = comma + 1;
  }
  fields[n++] = line.substring(start);
  return n;
}

void storageExportCsv(int onlyId, CsvSink emit) {
  String out = CSV_HEADER;
  int id, take;
  for (const String &name : takeNames()) {
    parseName(name, id, take);
    int index = indexForId(id);
    if (index < 0 || (onlyId >= 0 && id != onlyId)) continue;

    File f = LittleFS.open(String(TAKES_DIR) + "/" + name);
    uint64_t startEpochMs = 0, scanEpochMs = 0;
    String device;
    int scan = 0, heading = 0;
    String fields[5];
    while (f.available()) {
      String line = f.readStringUntil('\n');
      if (line.startsWith("H,") && splitFields(line, fields, 5) == 5) {
        startEpochMs = strtoull(fields[3].c_str(), nullptr, 10);
        device = fields[4];
      } else if (line.startsWith("S,") && splitFields(line, fields, 3) == 3) {
        scan++;
        scanEpochMs = startEpochMs + fields[1].toInt();
        heading = fields[2].toInt();
      } else if (line.startsWith("A,") && splitFields(line, fields, 5) == 5) {
        out += csvRow(scanEpochMs, fields[4], fields[1], fields[2].toInt(), fields[3].toInt(), index, take, scan,
                      heading, device);
      }
      if (out.length() > 2048) {
        emit(out);
        out = "";
      }
    }
  }
  if (out.length()) emit(out);
}
