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
  haveVersion_ = false;
  shadowVersion_ = 0;
  requestShadow();
}

void CloudShadow::requestShadow() {
  getToken_ = String("get-") + String(millis()) + "-" + String(++tokenSequence_);
  JsonDocument request;
  request["clientToken"] = getToken_;
  char buffer[96];
  serializeJson(request, buffer, sizeof(buffer));
  waitingForGet_ = true;
  requestedGetAt_ = millis();
  mqtt_.publish((shadowPrefix_ + "/get").c_str(), buffer);
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
    // Una orden RFID pendiente prevalece hasta comprobar su convergencia en AWS.
    if (localPending_) return;
    const uint32_t version = document["version"] | 0U;
    if (haveVersion_ && version <= shadowVersion_) return;
    shadowVersion_ = version;
    haveVersion_ = true;
    if (waitingForGet_) return; // GET anterior a este delta se volvera a solicitar.
    applyDesired(document["state"].as<JsonVariantConst>());
  } else if (receivedTopic.endsWith("/get/accepted")) {
    if (!waitingForGet_ || getToken_ != (document["clientToken"] | "")) return;
    const uint32_t version = document["version"] | 0U;
    if (haveVersion_ && version < shadowVersion_) {
      requestShadow();
      return;
    }
    shadowVersion_ = version;
    haveVersion_ = true;
    waitingForGet_ = false;
    if (localPending_) {
      const String desired = document["state"]["desired"]["cerrojo"] | "";
      const String reported = document["state"]["reported"]["cerrojo"] | "";
      if (!hardware_.isServoMoving() && desired == localDesired_ &&
          reported == localDesired_) {
        localPending_ = false;
        Serial.println("[AWS] Orden RFID sincronizada (estado de software).");
      }
    } else {
      applyDesired(document["state"]["desired"].as<JsonVariantConst>());
    }
    if (!hardware_.isServoMoving()) report();
  } else if (receivedTopic.endsWith("/get/rejected")) {
    if (!waitingForGet_ || getToken_ != (document["clientToken"] | "")) return;
    if ((document["code"] | 0) == 404) {
      waitingForGet_ = false;
      haveVersion_ = false;
      shadowVersion_ = 0;
      report(); // Crear Shadow; para otros errores conservar espera y reintentar.
    }
    Serial.println("[AWS] GET rechazado; revisar permisos o existencia del Shadow.");
  } else if (receivedTopic.endsWith("/update/rejected")) {
    Serial.println("[AWS] UPDATE rechazado; resincronizar version/permisos.");
    if (localPending_ && !waitingForGet_) requestShadow();
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

  if (localPending_) {
    // Una sola escritura reconcilia la orden local y el estado comandado.
    // CAS evita sobrescribir silenciosamente un documento que cambio tras GET.
    if (millis() - lastReport_ < 1000) return;
    document["state"]["desired"]["cerrojo"] = localDesired_;
    if (haveVersion_) document["version"] = shadowVersion_;
  }

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
    // publish() no confirma persistencia. GET valida ambos campos antes de soltar
    // la prioridad local; si se pierde la conexion, se conserva en RAM.
    if (localPending_) requestShadow();
  } else {
    Serial.println("[AWS] Error publicando Device Shadow.");
  }
}

void CloudShadow::update(bool hardwareChanged) {
  const unsigned long now = millis();
  if (localRequestSeen_ != hardware_.localRequestId()) {
    localRequestSeen_ = hardware_.localRequestId();
    localDesired_ = hardware_.localLockRequest();
    localPending_ = true;
    if (mqtt_.connected()) requestShadow();
  }
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
  if (waitingForGet_ && millis() - requestedGetAt_ >= 8000) {
    requestShadow();
    Serial.println("[AWS] Reintentando sincronización Shadow...");
  }
  if (hardwareChanged || millis() - lastReport_ >= SHADOW_REPORT_INTERVAL_MS) report();
}
