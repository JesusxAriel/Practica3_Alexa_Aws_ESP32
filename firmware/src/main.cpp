#include <Arduino.h>
#include "DoorHardware.h"
#include "CloudShadow.h"

DoorHardware hardware;
CloudShadow cloud(hardware);

void setup() {
  Serial.begin(115200);
  hardware.begin();
  cloud.begin();
}

void loop() {
  const bool changed = hardware.update();
  cloud.update(changed);
  delay(5);
}
