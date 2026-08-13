#include "heater_protocol.h"

void HeaterProtocol::begin(int rxPin, uint32_t baud) {
  // RX-only: no ESP32 TX pin is attached to the heater bus in monitor mode.
  serial_.begin(baud, SERIAL_8N1, rxPin, -1);
  resetFrame();
}

void HeaterProtocol::resetFrame() {
  index_ = 0;
  waitingForStart_ = true;
}

bool HeaterProtocol::poll() {
  bool gotFrame = false;
  while (serial_.available() > 0) {
    const uint8_t b = static_cast<uint8_t>(serial_.read());
    if (waitingForStart_) {
      if (b == 0x76) {
        frame_[0] = b;
        index_ = 1;
        waitingForStart_ = false;
      }
      continue;
    }
    if (index_ >= sizeof(frame_)) {
      telemetry_.rejectedFrames++;
      resetFrame();
      continue;
    }
    frame_[index_++] = b;
    if (index_ == sizeof(frame_)) {
      if (parseFrame()) gotFrame = true;
      else telemetry_.rejectedFrames++;
      resetFrame();
    }
  }
  return gotFrame;
}

bool HeaterProtocol::parseFrame() {
  if (frame_[0] != 0x76 || frame_[24] != 0x76) return false;
  const uint8_t *cmd = &frame_[0];
  const uint8_t *rsp = &frame_[24];
  HeaterTelemetry next = telemetry_;
  next.valid = true;
  next.updatedMs = millis();
  next.currentTempC = cmd[3];
  next.desiredTempC = cmd[4];
  next.runState = rsp[2];
  next.on = rsp[3] == 1;
  next.supplyV = static_cast<float>((rsp[4] << 8) | rsp[5]) * 0.1f;
  next.fanRpm = static_cast<uint16_t>((rsp[6] << 8) | rsp[7]);
  next.fanV = static_cast<float>((rsp[8] << 8) | rsp[9]) * 0.1f;
  next.exchangerTempC = static_cast<float>((rsp[10] << 8) | rsp[11]);
  next.glowV = static_cast<float>((rsp[12] << 8) | rsp[13]) * 0.1f;
  next.glowA = static_cast<float>((rsp[14] << 8) | rsp[15]) * 0.01f;
  next.pumpHz = static_cast<float>(rsp[16]) * 0.1f;
  next.errorCode = rsp[17];
  next.validFrames = telemetry_.validFrames + 1;
  telemetry_ = next;
  updateHexDump();
  return true;
}

void HeaterProtocol::updateHexDump() {
  size_t p = 0;
  for (size_t i = 0; i < sizeof(frame_) && p + 4 < sizeof(telemetry_.rawHex); ++i) {
    const int n = snprintf(telemetry_.rawHex + p, sizeof(telemetry_.rawHex) - p,
                           i == sizeof(frame_) - 1 ? "%02X" : "%02X ", frame_[i]);
    if (n <= 0) break;
    p += static_cast<size_t>(n);
  }
}

const char *HeaterProtocol::runStateName(uint8_t state) {
  switch (state) {
    case 0: return "Standby";
    case 1: return "Starting";
    case 2: return "Preheat";
    case 3: return "Retry wait";
    case 4: return "Warmup";
    case 5: return "Running";
    case 6: return "Stopping";
    case 7: return "Post-run";
    case 8: return "Cooldown";
    default: return "Unknown";
  }
}

const char *HeaterProtocol::errorName(uint8_t error) {
  static char text[16];
  if (error == 0) return "No error";
  snprintf(text, sizeof(text), "Code %u", error);
  return text;
}
