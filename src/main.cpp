#include <Arduino.h>
#include "heater_protocol.h"

HardwareSerial BusSerial(2);
HeaterProtocol monitor(BusSerial);

void setup() {
  Serial.begin(115200);
  monitor.begin(16, 25000);
  Serial.println("ESP32 passive bus monitor ready");
}

void loop() {
  if (monitor.poll()) {
    const auto &t = monitor.telemetry();
    Serial.printf("frames=%lu state=%u error=%u voltage=%.1f rpm=%u\n",
      static_cast<unsigned long>(t.validFrames), t.runState, t.errorCode,
      t.supplyV, t.fanRpm);
  }
  delay(2);
}
