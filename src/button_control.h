#pragma once
#include <Arduino.h>

struct ButtonChannel { int pin; bool active=false; uint32_t untilMs=0; };

class ButtonControl {
public:
  ButtonControl(int powerPin, int upPin, int downPin, int auxPin)
    : power_{powerPin}, up_{upPin}, down_{downPin}, aux_{auxPin} {}

  void begin() { init(power_); init(up_); init(down_); init(aux_); }

  void service() {
    serviceOne(power_); serviceOne(up_); serviceOne(down_); serviceOne(aux_);
    if (queuedRemaining_ && !pinsActive() && static_cast<int32_t>(millis()-nextQueueMs_) >= 0) {
      ButtonChannel *ch=channel(queuedName_);
      if (!ch) { queuedRemaining_=0; return; }
      pulse(*ch,queuedPulseMs_);
      queuedRemaining_--;
      nextQueueMs_=millis()+queuedPulseMs_+300;
    }
  }

  bool queue(const String &name,uint8_t count,uint32_t pulseMs) {
    if (!channel(name) || count<1 || count>20 || pulseMs<100 || pulseMs>4000 || anyActive()) return false;
    queuedName_=name; queuedRemaining_=count; queuedPulseMs_=pulseMs; nextQueueMs_=millis(); return true;
  }

  void cancelAll() { queuedRemaining_=0; release(power_); release(up_); release(down_); release(aux_); }
  bool anyActive() const { return pinsActive() || queuedRemaining_>0; }
  int powerPin() const { return power_.pin; } int upPin() const { return up_.pin; } int downPin() const { return down_.pin; } int auxPin() const { return aux_.pin; }

private:
  ButtonChannel power_,up_,down_,aux_;
  String queuedName_; uint8_t queuedRemaining_=0; uint32_t queuedPulseMs_=250; uint32_t nextQueueMs_=0;
  bool pinsActive() const { return power_.active||up_.active||down_.active||aux_.active; }
  static void init(ButtonChannel &ch){ pinMode(ch.pin,OUTPUT); digitalWrite(ch.pin,LOW); }
  static void release(ButtonChannel &ch){ digitalWrite(ch.pin,LOW); ch.active=false; ch.untilMs=0; }
  static void pulse(ButtonChannel &ch,uint32_t ms){ digitalWrite(ch.pin,HIGH); ch.active=true; ch.untilMs=millis()+ms; }
  static void serviceOne(ButtonChannel &ch){ if(ch.active && static_cast<int32_t>(millis()-ch.untilMs)>=0) release(ch); }
  ButtonChannel *channel(const String &name){ if(name=="power")return &power_; if(name=="up")return &up_; if(name=="down")return &down_; if(name=="aux")return &aux_; return nullptr; }
};
