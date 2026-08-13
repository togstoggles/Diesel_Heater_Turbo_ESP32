#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "heater_protocol.h"
#include "history.h"
#include "web_page.h"

static constexpr int HEATER_RX_PIN = 16;
static constexpr uint32_t HEATER_BAUD = 25000;
static constexpr uint32_t HISTORY_INTERVAL_MS = 5000;

static const char *AP_SSID = "Diesel-Heater-Lab";
static const char *AP_PASSWORD = "dieselheater";

HardwareSerial BusSerial(2);
HeaterProtocol monitor(BusSerial);
HistoryBuffer history;
WebServer server(80);
DNSServer dns;
uint32_t lastHistoryMs = 0;

static String jsonNumber(float v, uint8_t decimals = 1) {
  if (!isfinite(v)) return "null";
  return String(v, decimals);
}

static void sendState() {
  const auto &t = monitor.telemetry();
  String out;
  out.reserve(700);
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

static void setupWeb() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  const IPAddress ip = WiFi.softAPIP();
  dns.start(53, "*", ip);

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", WEB_PAGE);
  });
  server.on("/api/state", HTTP_GET, sendState);
  server.on("/api/history", HTTP_GET, sendHistoryJson);
  server.on("/api/history.csv", HTTP_GET, sendHistoryCsv);
  server.on("/generate_204", HTTP_GET, []() { server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); });
  server.on("/hotspot-detect.html", HTTP_GET, []() { server.send_P(200, "text/html", WEB_PAGE); });
  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });
  server.begin();

  Serial.printf("Phone Wi-Fi: %s\n", AP_SSID);
  Serial.printf("Password: %s\n", AP_PASSWORD);
  Serial.printf("Dashboard: http://%s/\n", ip.toString().c_str());
}

void setup() {
  Serial.begin(115200);
  delay(200);
  monitor.begin(HEATER_RX_PIN, HEATER_BAUD);
  setupWeb();
  Serial.println("Passive heater monitor ready. Factory LCD remains in control.");
}

void loop() {
  monitor.poll();
  dns.processNextRequest();
  server.handleClient();

  const auto &t = monitor.telemetry();
  if (t.valid && millis() - lastHistoryMs >= HISTORY_INTERVAL_MS) {
    lastHistoryMs = millis();
    history.add(t);
  }
  delay(2);
}
