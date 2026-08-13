#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include "heater_protocol.h"
#include "history.h"
#include "web_page.h"
#include "control_page.h"
#include "button_control.h"

static constexpr int HEATER_RX_PIN = 16;
static constexpr uint32_t HEATER_BAUD = 25000;
static constexpr uint32_t HISTORY_INTERVAL_MS = 5000;
static constexpr uint32_t CONTROL_FRESH_MS = 15000;
static constexpr uint32_t ARM_DURATION_MS = 30UL * 60UL * 1000UL;

static const char *AP_SSID = "Diesel-Heater-Lab";
static const char *AP_PASSWORD = "dieselheater";

HardwareSerial BusSerial(2);
HeaterProtocol monitor(BusSerial);
HistoryBuffer history;
WebServer server(80);
DNSServer dns;
ButtonControl buttons(25, 26, 27, 33);
uint32_t lastHistoryMs = 0;

bool controlArmed = false;
uint32_t armUntilMs = 0;
bool thermostatEnabled = false;
float thermostatTargetC = 20.0f;
float thermostatHystC = 1.0f;
uint32_t thermostatMinCycleMs = 5UL * 60UL * 1000UL;
uint32_t lastAutomaticActionMs = 0;
bool timerEnabled = false;
uint32_t timerEndMs = 0;

static String jsonNumber(float v, uint8_t decimals = 1) {
  if (!isfinite(v)) return "null";
  return String(v, static_cast<unsigned int>(decimals));
}

static bool freshTelemetry() {
  const auto &t = monitor.telemetry();
  return t.valid && (millis() - t.updatedMs) < CONTROL_FRESH_MS;
}

static void releaseControl() {
  controlArmed = false;
  thermostatEnabled = false;
  timerEnabled = false;
  buttons.cancelAll();
}

static uint32_t armSecondsRemaining() {
  if (!controlArmed) return 0;
  const int32_t remaining = static_cast<int32_t>(armUntilMs - millis());
  if (remaining <= 0) {
    releaseControl();
    return 0;
  }
  return static_cast<uint32_t>(remaining) / 1000UL;
}

static void sendState() {
  const auto &t = monitor.telemetry();
  String out;
  out.reserve(760);
  out += "{";
  out += "\"valid\":" + String(t.valid ? "true" : "false");
  out += ",\"ageMs\":" + String(t.valid ? millis() - t.updatedMs : 0);
  out += ",\"currentTemp\":" + jsonNumber(t.currentTempC);
  out += ",\"desiredTemp\":" + jsonNumber(t.desiredTempC);
  out += ",\"exchangerTemp\":" + jsonNumber(t.exchangerTempC, 0);
  out += ",\"supply\":" + jsonNumber(t.supplyV);
  out += ",\"fanRpm\":" + String(t.fanRpm);
  out += ",\"fanV\":" + jsonNumber(t.fanV);
  out += ",\"glowV\":" + jsonNumber(t.glowV);
  out += ",\"glowA\":" + jsonNumber(t.glowA, 2);
  out += ",\"pump\":" + jsonNumber(t.pumpHz);
  out += ",\"runState\":" + String(t.runState);
  out += ",\"runStateName\":\"" + String(HeaterProtocol::runStateName(t.runState)) + "\"";
  out += ",\"errorCode\":" + String(t.errorCode);
  out += ",\"errorName\":\"" + String(HeaterProtocol::errorName(t.errorCode)) + "\"";
  out += ",\"on\":" + String(t.on ? "true" : "false");
  out += ",\"validFrames\":" + String(t.validFrames);
  out += ",\"rejectedFrames\":" + String(t.rejectedFrames);
  out += ",\"raw\":\"" + String(t.rawHex) + "\"";
  out += "}";
  server.send(200, "application/json", out);
}

static void sendControlState() {
  const uint32_t armSeconds = armSecondsRemaining();
  uint32_t timerSeconds = 0;
  if (timerEnabled) {
    const int32_t remaining = static_cast<int32_t>(timerEndMs - millis());
    timerSeconds = remaining > 0 ? static_cast<uint32_t>(remaining) / 1000UL : 0;
  }
  String out;
  out.reserve(360);
  out += "{";
  out += "\"armed\":" + String(controlArmed ? "true" : "false");
  out += ",\"armSeconds\":" + String(armSeconds);
  out += ",\"outputsActive\":" + String(buttons.anyActive() ? "true" : "false");
  out += ",\"thermostat\":" + String(thermostatEnabled ? "true" : "false");
  out += ",\"target\":" + jsonNumber(thermostatTargetC);
  out += ",\"hyst\":" + jsonNumber(thermostatHystC);
  out += ",\"minCycleMinutes\":" + String(thermostatMinCycleMs / 60000UL);
  out += ",\"timerSeconds\":" + String(timerSeconds);
  out += ",\"powerPin\":" + String(buttons.powerPin());
  out += ",\"upPin\":" + String(buttons.upPin());
  out += ",\"downPin\":" + String(buttons.downPin());
  out += ",\"auxPin\":" + String(buttons.auxPin());
  out += "}";
  server.send(200, "application/json", out);
}

static void sendHistoryCsv() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.sendHeader("Content-Disposition", "attachment; filename=diesel-heater-history.csv");
  server.send(200, "text/csv", "ms,room_c,setpoint_c,exchanger_c,supply_v,glow_v,glow_a,pump_hz,rpm,state,error\n");
  for (size_t i = 0; i < history.count(); ++i) {
    const auto s = history.atOldestIndex(i);
    String row;
    row.reserve(120);
    row += String(s.t) + ",";
    row += String(s.room, 1) + ",";
    row += String(s.setpoint, 1) + ",";
    row += String(s.exchanger, 0) + ",";
    row += String(s.supply, 1) + ",";
    row += String(s.glowV, 1) + ",";
    row += String(s.glowA, 2) + ",";
    row += String(s.pump, 1) + ",";
    row += String(s.rpm) + ",";
    row += String(s.state) + ",";
    row += String(s.error) + "\n";
    server.sendContent(row);
  }
  server.sendContent("");
}

static void sendHistoryJson() {
  String out;
  out.reserve(16000);
  out = "[";
  for (size_t i = 0; i < history.count(); ++i) {
    if (i) out += ',';
    const auto s = history.atOldestIndex(i);
    out += "{\"t\":" + String(s.t);
    out += ",\"room\":" + jsonNumber(s.room);
    out += ",\"setpoint\":" + jsonNumber(s.setpoint);
    out += ",\"exchanger\":" + jsonNumber(s.exchanger, 0);
    out += ",\"supply\":" + jsonNumber(s.supply);
    out += ",\"glowV\":" + jsonNumber(s.glowV);
    out += ",\"glowA\":" + jsonNumber(s.glowA, 2);
    out += ",\"pump\":" + jsonNumber(s.pump);
    out += ",\"rpm\":" + String(s.rpm);
    out += ",\"state\":" + String(s.state);
    out += ",\"error\":" + String(s.error) + "}";
  }
  out += "]";
  server.send(200, "application/json", out);
}

static bool queueNormalStart() {
  if (!controlArmed || !freshTelemetry()) return false;
  const auto &t = monitor.telemetry();
  if ((t.errorCode != 0 && t.errorCode != 1) || t.runState != 0) return false;
  return buttons.queue("power", 1, 500);
}

static bool queueNormalStop() {
  if (!controlArmed || !freshTelemetry()) return false;
  const auto &t = monitor.telemetry();
  if (t.runState < 1 || t.runState > 5) return false;
  return buttons.queue("power", 1, 3000);
}

static bool queueManualButton(const String &name, uint8_t count, uint32_t ms) {
  if (!controlArmed || !freshTelemetry()) return false;
  const auto &t = monitor.telemetry();
  if (name == "up" || name == "down") {
    if ((t.errorCode != 0 && t.errorCode != 1) || (t.runState != 0 && t.runState != 5) || ms > 1000) return false;
  } else if (name == "power") {
    if (t.runState > 5 || ms > 4000) return false;
    if (t.runState == 0 && t.errorCode != 0 && t.errorCode != 1) return false;
  } else if (name == "aux") {
    if (ms > 1000) return false;
  } else {
    return false;
  }
  return buttons.queue(name, count, ms);
}

static void serviceAutomaticControls() {
  armSecondsRemaining();
  if (!controlArmed || !freshTelemetry() || buttons.anyActive()) return;
  const auto &t = monitor.telemetry();
  if (t.errorCode != 0 && t.errorCode != 1) return;

  if (timerEnabled && static_cast<int32_t>(millis() - timerEndMs) >= 0) {
    timerEnabled = false;
    if (t.runState >= 1 && t.runState <= 5) buttons.queue("power", 1, 3000);
    return;
  }

  if (!thermostatEnabled || !isfinite(t.currentTempC)) return;
  if (millis() - lastAutomaticActionMs < thermostatMinCycleMs) return;

  if (t.runState == 0 && t.currentTempC <= thermostatTargetC - thermostatHystC) {
    if (buttons.queue("power", 1, 500)) lastAutomaticActionMs = millis();
  } else if (t.runState == 5 && t.currentTempC >= thermostatTargetC + thermostatHystC) {
    if (buttons.queue("power", 1, 3000)) lastAutomaticActionMs = millis();
  }
}

static void setupUpdater() {
  server.on("/update", HTTP_GET, []() {
    static const char PAGE[] PROGMEM = R"HTML(<!doctype html><meta name="viewport" content="width=device-width,initial-scale=1"><body style="font-family:system-ui;background:#111;color:#eee;padding:18px"><h2>ESP32 firmware update</h2><p>Only update while the heater is off and fully cooled.</p><form method="POST" action="/update" enctype="multipart/form-data"><input type="file" name="firmware" accept=".bin" required><button style="margin:10px;padding:10px">Upload and reboot</button></form><p><a style="color:#9cf" href="/control">Back to controls</a></p></body>)HTML";
    server.send_P(200, "text/html", PAGE);
  });
  server.on("/update", HTTP_POST,
    []() {
      const bool ok = !Update.hasError();
      server.send(200, "text/plain", ok ? "Update complete. Rebooting..." : "Update failed.");
      delay(500);
      if (ok) ESP.restart();
    },
    []() {
      HTTPUpload &upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        releaseControl();
        Update.begin(UPDATE_SIZE_UNKNOWN);
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
      } else if (upload.status == UPLOAD_FILE_END) {
        if (!Update.end(true)) Update.printError(Serial);
      }
    });
}

static void setupWeb() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  const IPAddress ip = WiFi.softAPIP();
  dns.start(53, "*", ip);

  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", WEB_PAGE); });
  server.on("/control", HTTP_GET, []() { server.send_P(200, "text/html", CONTROL_PAGE); });
  server.on("/api/state", HTTP_GET, sendState);
  server.on("/api/history", HTTP_GET, sendHistoryJson);
  server.on("/api/history.csv", HTTP_GET, sendHistoryCsv);
  server.on("/api/control/state", HTTP_GET, sendControlState);

  server.on("/api/control/arm", HTTP_POST, []() {
    if (server.arg("confirm") != "I_ACCEPT_HEATER_CONTROL") { server.send(400, "text/plain", "Confirmation required."); return; }
    if (!freshTelemetry()) { server.send(409, "text/plain", "No fresh heater telemetry. Check blue-wire monitor first."); return; }
    controlArmed = true;
    armUntilMs = millis() + ARM_DURATION_MS;
    server.send(200, "text/plain", "Control armed for 30 minutes.");
  });
  server.on("/api/control/disarm", HTTP_POST, []() { releaseControl(); server.send(200, "text/plain", "Control locked and outputs released."); });

  server.on("/api/control/start", HTTP_POST, []() {
    const bool ok = queueNormalStart();
    server.send(ok ? 200 : 409, "text/plain", ok ? "Start queued." : "Start blocked by state, fault, stale telemetry, lock, or busy output.");
  });
  server.on("/api/control/stop", HTTP_POST, []() {
    const bool ok = queueNormalStop();
    server.send(ok ? 200 : 409, "text/plain", ok ? "Normal stop queued." : "Stop blocked by state, stale telemetry, lock, or busy output.");
  });

  server.on("/api/control/button", HTTP_POST, []() {
    String name = server.arg("name");
    uint8_t count = static_cast<uint8_t>(constrain(server.arg("count").toInt(), 1, 10));
    uint32_t ms = static_cast<uint32_t>(constrain(server.arg("ms").toInt(), 100, 4000));
    const bool ok = queueManualButton(name, count, ms);
    server.send(ok ? 200 : 409, "text/plain", ok ? "Button pulse queued." : "Button pulse blocked by state, fault, stale telemetry, lock, limits, or busy output.");
  });

  server.on("/api/control/thermostat", HTTP_POST, []() {
    const bool enable = server.arg("enable") == "1";
    if (!enable) { thermostatEnabled = false; server.send(200, "text/plain", "Thermostat disabled."); return; }
    if (!controlArmed) { server.send(423, "text/plain", "Control is locked."); return; }
    const float target = server.arg("target").toFloat();
    const float hyst = server.arg("hyst").toFloat();
    const int minCycle = server.arg("minCycle").toInt();
    if (target < 5 || target > 35 || hyst < 0.5f || hyst > 5.0f || minCycle < 2 || minCycle > 60) { server.send(400, "text/plain", "Thermostat values out of range."); return; }
    thermostatTargetC = target;
    thermostatHystC = hyst;
    thermostatMinCycleMs = static_cast<uint32_t>(minCycle) * 60000UL;
    thermostatEnabled = true;
    lastAutomaticActionMs = millis() - thermostatMinCycleMs;
    server.send(200, "text/plain", "Thermostat enabled.");
  });

  server.on("/api/control/timer", HTTP_POST, []() {
    if (server.arg("cancel") == "1") { timerEnabled = false; server.send(200, "text/plain", "Timer cancelled."); return; }
    if (!controlArmed) { server.send(423, "text/plain", "Control is locked."); return; }
    const int minutes = server.arg("minutes").toInt();
    if (minutes < 1 || minutes > 720) { server.send(400, "text/plain", "Timer must be 1 to 720 minutes."); return; }
    timerEndMs = millis() + static_cast<uint32_t>(minutes) * 60000UL;
    timerEnabled = true;
    server.send(200, "text/plain", "Stop timer set.");
  });

  setupUpdater();
  server.on("/generate_204", HTTP_GET, []() { server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); });
  server.on("/hotspot-detect.html", HTTP_GET, []() { server.send_P(200, "text/html", WEB_PAGE); });
  server.onNotFound([]() { server.sendHeader("Location", "http://192.168.4.1/", true); server.send(302, "text/plain", ""); });
  server.begin();

  Serial.printf("Phone Wi-Fi: %s\n", AP_SSID);
  Serial.printf("Password: %s\n", AP_PASSWORD);
  Serial.printf("Dashboard: http://%s/\n", ip.toString().c_str());
  Serial.printf("Controls: http://%s/control\n", ip.toString().c_str());
}

void setup() {
  Serial.begin(115200);
  delay(200);
  buttons.begin();
  monitor.begin(HEATER_RX_PIN, HEATER_BAUD);
  setupWeb();
  Serial.println("Heater monitor ready. Control outputs are locked until explicitly armed.");
}

void loop() {
  monitor.poll();
  buttons.service();
  serviceAutomaticControls();
  dns.processNextRequest();
  server.handleClient();

  const auto &t = monitor.telemetry();
  if (t.valid && millis() - lastHistoryMs >= HISTORY_INTERVAL_MS) {
    lastHistoryMs = millis();
    history.add(t);
  }
  delay(2);
}
