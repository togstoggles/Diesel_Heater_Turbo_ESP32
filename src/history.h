#pragma once
#include <Arduino.h>
#include "heater_protocol.h"

struct HistorySample {
  uint32_t t;
  float room;
  float setpoint;
  float exchanger;
  float supply;
  float glowV;
  float glowA;
  float pump;
  uint16_t rpm;
  uint8_t state;
  uint8_t error;
};

class HistoryBuffer {
public:
  static constexpr size_t CAPACITY = 360;
  void add(const HeaterTelemetry &x) {
    if (!x.valid) return;
    HistorySample s;
    s.t = millis();
    s.room = x.currentTempC;
    s.setpoint = x.desiredTempC;
    s.exchanger = x.exchangerTempC;
    s.supply = x.supplyV;
    s.glowV = x.glowV;
    s.glowA = x.glowA;
    s.pump = x.pumpHz;
    s.rpm = x.fanRpm;
    s.state = x.runState;
    s.error = x.errorCode;
    samples_[head_] = s;
    head_ = (head_ + 1) % CAPACITY;
    if (count_ < CAPACITY) count_++;
  }
  size_t count() const { return count_; }
  HistorySample atOldestIndex(size_t i) const {
    const size_t start = count_ == CAPACITY ? head_ : 0;
    return samples_[(start + i) % CAPACITY];
  }
private:
  HistorySample samples_[CAPACITY] = {};
  size_t head_ = 0;
  size_t count_ = 0;
};
