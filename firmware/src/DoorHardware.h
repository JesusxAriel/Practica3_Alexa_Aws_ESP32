#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>
#include <MFRC522.h>

class DoorHardware {
 public:
  DoorHardware();

  void begin();
  bool update();

  // Control del servo desde AWS
  bool requestLockState(const String& state);

  // Información para Device Shadow
  String lockState() const { return lockState_; }
  unsigned long cardsRead() const { return cardsRead_; }
  String lastRFID() const { return lastRFID_; }
  bool rfidAvailable() const { return rfidAvailable_; }
  bool isServoMoving() const { return moving_; }

 private:
  MFRC522 rfid_;
  Servo servo_;

  unsigned long cardsRead_ = 0;
  unsigned long lastCardAt_ = 0;

  String lastRFID_ = "ninguno";
  bool rfidAvailable_ = false;

  String lockState_ = "desconocido";
  String targetLockState_ = "desconocido";

  bool moving_ = false;
  unsigned long moveStartedAt_ = 0;
};
