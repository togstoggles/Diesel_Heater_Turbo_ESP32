#pragma once
#include <Arduino.h>

struct HeaterTelemetry {
  bool valid = false;
  uint32_t updatedMs = 0;
  float currentTempC = NAN;
  float desiredTempC = NAN;
  float supplyV = NAN;
  float exchangerTempC = NAN;
  float glowV = NAN;
  float glowA = NAN;
  float pumpHz = NAN;
  float fanV = NAN;
  uint16_t fanRpm = 0;
  uint8_t runState = 255;
  uint8_t errorCode = 255;
  bool on = false;
  uint32_t validFrames = 0;
  uint32_t rejectedFrames = 0;
  char rawHex[145] = {0};
};

class HeaterProtocol {
public:
  explicit HeaterProtocol(HardwareSerial &serial) : serial_(serial) {}
  void begin(int rxPin, uint32_t baud = 25000);
  bool poll();
  const HeaterTelemetry &telemetry() const { return telemetry_; }
  static const char *runStateName(uint8_t state);
  static const char *errorName(uint8_t error);
private:
  HardwareSerial &serial_;
  HeaterTelemetry telemetry_;
  uint8_t frame_[48] = {0};
  size_t index_ = 0;
  bool waitingForStart_ = true;
  void resetFrame();
  bool parseFrame();
  void updateHexDump();
};
