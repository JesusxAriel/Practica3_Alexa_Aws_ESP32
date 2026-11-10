#include "CloudShadow.h"
#include <WiFi.h>
#include <time.h>
#include "config.h"

CloudShadow::CloudShadow(DoorHardware& hardware) : hardware_(hardware), mqtt_(tls_) {}

void CloudShadow::begin() {
  shadowPrefix_ = String("$aws/things/") + THING_NAME + "/shadow";
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttempt_ = millis();
  tls_.setCACert(AWS_ROOT_CA);
  tls_.setCertificate(AWS_DEVICE_CERT);
  tls_.setPrivateKey(AWS_PRIVATE_KEY);
  mqtt_.setServer(AWS_IOT_ENDPOINT, 8883);
  mqtt_.setBufferSize(2048);
  mqtt_.setKeepAlive(60);
  mqtt_.setSocketTimeout(4);
  mqtt_.setCallback([this](char* t, byte* p, unsigned int n) { onMessage(t, p, n); });
  Serial.println("[AWS] Cliente Shadow inicializado.");
}

void CloudShadow::connectMqtt() {
  if (mqtt_.connected()) return;
  if (millis() - lastMqttAttempt_ < 3000) return;
  lastMqttAttempt_ = millis();
  Serial.println("[AWS] Conectando MQTT/TLS...");
  if (!mqtt_.connect(THING_NAME)) {
    Serial.printf("[AWS] Error MQTT=%d\n", mqtt_.state());
    return;
  }
  Serial.println("[AWS] Conectado.");
  // Subscribir ANTES de solicitar el Shadow; AWS responde de forma asíncrona.
  mqtt_.subscribe((shadowPrefix_ + "/update/delta").c_str(), 1);
  mqtt_.subscribe((shadowPrefix_ + "/get/accepted").c_str(), 1);
  mqtt_.subscribe((shadowPrefix_ + "/get/rejected").c_str(), 1);
  mqtt_.subscribe((shadowPrefix_ + "/update/rejected").c_str(), 1);
  waitingForGet_ = true;
  requestedGetAt_ = millis();
  mqtt_.publish((shadowPrefix_ + "/get").c_str(), "");
}

void CloudShadow::applyDesired(JsonVariantConst desired) {
  if (desired.isNull()) return;
  const char* requested = desired["cerrojo"] | "";
  if (strlen(requested) && !hardware_.requestLockState(String(requested))) {
    Serial.println("[AWS] Estado desired inválido; ignorado.");
  }
}

void CloudShadow::onMessage(char* topic, byte* payload, unsigned int length) {
  JsonDocument document;
  auto err = deserializeJson(document, payload, length);
  if (err) {
    Serial.printf("[AWS] JSON inválido: %s\n", err.c_str());
    return;
  }
  String receivedTopic(topic);
  if (receivedTopic.endsWith("/update/delta")) {
    applyDesired(document["state"].as<JsonVariantConst>());
  } else if (receivedTopic.endsWith("/get/accepted")) {
    // Importante: aplicar el desired pendiente ANTES de confirmar reported.
    applyDesired(document["state"]["desired"].as<JsonVariantConst>());
    waitingForGet_ = false;
    if (!hardware_.isServoMoving()) report();
  } else if (receivedTopic.endsWith("/get/rejected")) {
    waitingForGet_ = false; // Shadow inexistente (404) u otro error.
    Serial.println("[AWS] GET rechazado; revisar el código devuelto.");
    report(); // Crea Shadow inicial si falta. Para otros errores quedará en log.
  } else if (receivedTopic.endsWith("/update/rejected")) {
    Serial.println("[AWS] UPDATE rechazado; comprobar política del Thing.");
  }
}


void CloudShadow::report() {
  // Publicar solo con MQTT conectado, Shadow sincronizado
  // y sin un movimiento pendiente del servomotor.
  if (!mqtt_.connected() ||
      waitingForGet_ ||
      hardware_.isServoMoving()) {
    return;
  }

  JsonDocument document;

  JsonObject reported =
      document["state"]["reported"].to<JsonObject>();

  // Estado comandado del servomotor.
  // No representa una confirmacion mecanica.
  reported["cerrojo"] = hardware_.lockState();

  // Datos del lector RFID
  reported["tarjetasLeidas"] = hardware_.cardsRead();
  reported["ultimoRFID"] = hardware_.lastRFID();
  reported["rfid_available"] = hardware_.rfidAvailable();

  char buffer[512];

  size_t length =
      serializeJson(document, buffer, sizeof(buffer));

  if (length == 0 || length >= sizeof(buffer)) {
    Serial.println("[AWS] Error: JSON demasiado grande.");
    return;
  }

  const String topic = shadowPrefix_ + "/update";

  if (mqtt_.publish(topic.c_str(), buffer)) {
    lastReport_ = millis();

    Serial.println("[AWS] Estado enviado a Device Shadow:");
    Serial.println(buffer);
  } else {
    Serial.println("[AWS] Error publicando Device Shadow.");
  }
}

void CloudShadow::update(bool hardwareChanged) {
  const unsigned long now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWifiAttempt_ >= 5000) {
      lastWifiAttempt_ = now;
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      Serial.println("[WiFi] Reconectando...");
    }
    return;
  }
  // El reloj UTC correcto es necesario para validar los certificados TLS.
  if (time(nullptr) < 1700000000) {
    static unsigned long lastNtpAttempt = 0;
    if (now - lastNtpAttempt >= 4000) {
      lastNtpAttempt = now;
      configTime(0, 0, "pool.ntp.org", "time.google.com");
      Serial.println("[NTP] Sincronizando hora...");
    }
    return;
  }
  if (!mqtt_.connected()) connectMqtt();
  if (!mqtt_.connected()) return;
  mqtt_.loop();
  // Evitar quedar bloqueado por pérdida de respuesta GET.
  if (waitingForGet_ && now - requestedGetAt_ >= 8000) {
    mqtt_.publish((shadowPrefix_ + "/get").c_str(), "");
    requestedGetAt_ = now;
    Serial.println("[AWS] Reintentando sincronización Shadow...");
  }
  if (hardwareChanged || now - lastReport_ >= SHADOW_REPORT_INTERVAL_MS) report();
}
