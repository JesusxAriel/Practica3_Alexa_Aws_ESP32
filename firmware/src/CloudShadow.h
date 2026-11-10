#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "DoorHardware.h"

class CloudShadow {
 public:
  explicit CloudShadow(DoorHardware& hardware);
  void begin();
  void update(bool hardwareChanged);

 private:
  void onMessage(char* topic, byte* payload, unsigned int length);
  void connectMqtt();
  void report();
  void applyDesired(JsonVariantConst desired);
  DoorHardware& hardware_;
  WiFiClientSecure tls_;
  PubSubClient mqtt_;
  String shadowPrefix_;
  unsigned long lastWifiAttempt_ = 0;
  unsigned long lastMqttAttempt_ = 0;
  unsigned long lastReport_ = 0;
  unsigned long requestedGetAt_ = 0;
  bool waitingForGet_ = true;
};
