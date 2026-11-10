#include "DoorHardware.h"
#include <SPI.h>
#include "config.h"

namespace {
constexpr int SERVO_CENTRO = 90;
constexpr int SERVO_BLOQUEADO = 60;
constexpr int SERVO_DESBLOQUEADO = 120;
constexpr unsigned long SERVO_MOVIMIENTO_MS = 850;
}

DoorHardware::DoorHardware()
    : rfid_(PIN_RFID_SS, PIN_RFID_RST) {}

void DoorHardware::begin() {
  Serial.println("[Hardware] Inicializando...");

  // Inicializar RFID
  SPI.begin(PIN_RFID_SCK, PIN_RFID_MISO,
            PIN_RFID_MOSI, PIN_RFID_SS);

  rfid_.PCD_Init();
  delay(100);

  byte version = rfid_.PCD_ReadRegister(
      MFRC522::VersionReg);

  Serial.printf("[RFID] Version: 0x%02X\n", version);

  rfidAvailable_ = (version != 0x00 && version != 0xFF);

  // Inicializar servomotor
  servo_.setPeriodHertz(50);
  servo_.attach(PIN_SERVO, 500, 2400);
  servo_.write(SERVO_CENTRO);

  // No asumimos que existe un cerrojo físico confirmado
  lockState_ = "desconocido";
  targetLockState_ = "desconocido";

  Serial.println("[Servo] Posicion inicial solicitada: 90");
  Serial.println("[Hardware] Inicializacion finalizada.");
}

bool DoorHardware::requestLockState(const String& state) {
  if (state != "bloqueada" &&
      state != "desbloqueada") {
    return false;
  }

  if (!moving_ && state == lockState_) {
    return true;
  }

  targetLockState_ = state;

  if (!servo_.attached()) {
    servo_.attach(PIN_SERVO, 500, 2400);
  }

  const int angle =
      state == "bloqueada"
          ? SERVO_BLOQUEADO
          : SERVO_DESBLOQUEADO;

  servo_.write(angle);

  moving_ = true;
  moveStartedAt_ = millis();

  Serial.printf(
      "[Servo] Estado solicitado: %s, angulo: %d\n",
      state.c_str(), angle);

  return true;
}

bool DoorHardware::update() {
  unsigned long now = millis();
  bool changed = false;

  // Finalizar comando del servo
  if (moving_ &&
      now - moveStartedAt_ >= SERVO_MOVIMIENTO_MS) {

    moving_ = false;
    lockState_ = targetLockState_;

    // Deja de enviar pulsos; no corta la alimentacion.
    servo_.detach();

    changed = true;

    Serial.printf(
        "[Servo] Comando finalizado: %s\n",
        lockState_.c_str());
  }

  // Verificar nuevas tarjetas RFID
  if (rfidAvailable_ &&
      now - lastCardAt_ >= 1200 &&
      rfid_.PICC_IsNewCardPresent() &&
      rfid_.PICC_ReadCardSerial()) {

    lastCardAt_ = now;
    cardsRead_++;

    const byte TARJETA[] = {0x39, 0x61, 0xD7, 0x06};
    const byte LLAVERO[] = {0x9C, 0x31, 0x8B, 0x06};

    bool esTarjeta = (rfid_.uid.size == 4);
    bool esLlavero = (rfid_.uid.size == 4);

    if (rfid_.uid.size == 4) {
      for (byte i = 0; i < 4; i++) {
        if (rfid_.uid.uidByte[i] != TARJETA[i]) {
          esTarjeta = false;
        }
        if (rfid_.uid.uidByte[i] != LLAVERO[i]) {
          esLlavero = false;
        }
      }
    }

    if (esTarjeta) {
      lastRFID_ = "tarjeta";
    } else if (esLlavero) {
      lastRFID_ = "llavero";
    } else {
      lastRFID_ = "desconocido";
    }

    Serial.printf(
        "[RFID] Tipo: %s | Lecturas: %lu\n",
        lastRFID_.c_str(), cardsRead_);

    // RFID solo reporta; el servo lo controla AWS.
    changed = true;

    rfid_.PICC_HaltA();
    rfid_.PCD_StopCrypto1();
  }

  return changed;
}
