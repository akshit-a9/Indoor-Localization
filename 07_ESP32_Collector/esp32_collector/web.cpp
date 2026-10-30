#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>

#include "config.h"
#include "floor_maps.h"
#include "locations.h"
#include "recorder.h"
#include "storage.h"
#include "web.h"
#include "web_page.h"

static WebServer server(80);
static DNSServer dns;

static void sendJson(const JsonDocument &doc, int code = 200) {
  String body;
  serializeJson(doc, body);
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

static void sendResult(bool ok, const String &error = "") {
  JsonDocument doc;
  doc["ok"] = ok;
  if (!ok) doc["error"] = error;
  sendJson(doc, ok ? 200 : 400);
}

static const char *stateName(RecorderState state) {
  switch (state) {
    case REC_COUNTDOWN: return "countdown";
    case REC_RECORDING: return "recording";
    case REC_DONE:      return "done";
    case REC_ERROR:     return "error";
    default:            return "idle";
  }
}

static void handleLocations() {
  JsonDocument doc;
  doc["map_w"] = MAP_WIDTH;
  doc["map_h"] = MAP_HEIGHT;
  doc["session_ms"] = SESSION_MS;
  doc["headings"] = HEADINGS;
  JsonArray list = doc["locations"].to<JsonArray>();
  for (int i = 0; i < LOCATION_COUNT; ++i) {
    JsonObject l = list.add<JsonObject>();
    l["i"] = i;
    l["id"] = LOCATIONS[i].id;
    l["label"] = LOCATIONS[i].label;
    l["file"] = LOCATIONS[i].file;
    l["floor"] = LOCATIONS[i].floor;
    l["x"] = LOCATIONS[i].x;
    l["y"] = LOCATIONS[i].y;
  }
  sendJson(doc);
}

static void handleStatus() {
  const RecorderInfo &rec = recorderInfo();
  JsonDocument doc;
  doc["state"] = stateName(rec.state);
  doc["loc"] = rec.location;
  doc["ms_left"] = rec.msLeft;
  doc["heading"] = rec.heading;
  doc["scans"] = rec.scans;
  doc["aps"] = rec.lastAps;
  doc["error"] = rec.error;
  doc["fs_used"] = storageUsedBytes();
  doc["fs_total"] = storageTotalBytes();
  JsonArray takes = doc["takes"].to<JsonArray>();
  for (int i = 0; i < LOCATION_COUNT; ++i) takes.add(storageTakeCount(i));
  sendJson(doc);
}

static void handleStart() {
  String error;
  int index = server.hasArg("i") ? server.arg("i").toInt() : -1;
  uint64_t epoch = strtoull(server.arg("epoch").c_str(), nullptr, 10);
  if (recorderInfo().state == REC_DONE || recorderInfo().state == REC_ERROR) recorderAcknowledge();
  sendResult(recorderStart(index, epoch, error), error);
}

static void handleTakes() {
  JsonDocument doc;
  storageListTakes(doc["takes"].to<JsonArray>());
  sendJson(doc);
}

static void handleDelete() {
  bool busy = recorderInfo().state == REC_COUNTDOWN || recorderInfo().state == REC_RECORDING;
  if (busy) return sendResult(false, "Wait for the recording to finish");
  sendResult(storageDeleteTake(server.arg("name")), "No such take");
}

static void sendPiece(const String &piece) { server.sendContent(piece); }

static void handleDownload() {
  int onlyId = server.hasArg("id") ? server.arg("id").toInt() : -1;
  String file = onlyId >= 0 ? "esp32_scans_" + String(onlyId) + ".csv" : "esp32_scans.csv";
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + file + "\"");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv", "");
  storageExportCsv(onlyId, sendPiece);
  server.sendContent("");  // end of the chunked response
}

static void sendMap(const uint8_t *data, size_t length) {
  server.sendHeader("Cache-Control", "max-age=86400");
  server.send_P(200, "image/jpeg", (const char *)data, length);
}

// Anything else (including phones checking for internet) goes to the page.
static void handleOther() {
  server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
  server.send(302, "text/plain", "");
}

void webInit() {
  dns.start(53, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/map0.jpg", HTTP_GET, [] { sendMap(MAP_0_JPG, MAP_0_JPG_LEN); });
  server.on("/map1.jpg", HTTP_GET, [] { sendMap(MAP_1_JPG, MAP_1_JPG_LEN); });
  server.on("/api/locations", HTTP_GET, handleLocations);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/start", HTTP_POST, handleStart);
  server.on("/api/cancel", HTTP_POST, [] { recorderCancel(); sendResult(true); });
  server.on("/api/ack", HTTP_POST, [] { recorderAcknowledge(); sendResult(true); });
  server.on("/api/takes", HTTP_GET, handleTakes);
  server.on("/api/delete", HTTP_POST, handleDelete);
  server.on("/download.csv", HTTP_GET, handleDownload);
  server.onNotFound(handleOther);
  server.begin();
}

void webLoop() {
  dns.processNextRequest();
  server.handleClient();
}
