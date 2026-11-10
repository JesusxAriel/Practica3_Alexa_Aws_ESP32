#pragma once
// Copiar a config.h, completar SOLO localmente. Nunca subir config.h al GitHub.
#define WIFI_SSID "REEMPLAZAR_WIFI"
#define WIFI_PASSWORD "REEMPLAZAR_PASSWORD"
#define AWS_IOT_ENDPOINT "REEMPLAZAR-ats.iot.us-east-1.amazonaws.com"
#define THING_NAME "puerta-sis234-g1-01"
// La CA es PUBLICA; el certificado de dispositivo y la clave privada son SECRETOS.
// Usar bloques PEM completos, incluidos BEGIN / END y saltos de linea.
static const char AWS_ROOT_CA[] = R"PEM(
-----BEGIN CERTIFICATE-----
PEGAR_AQUI_AMAZON_ROOT_CA_1
-----END CERTIFICATE-----
)PEM";
static const char AWS_DEVICE_CERT[] = R"PEM(
-----BEGIN CERTIFICATE-----
PEGAR_AQUI_CERTIFICADO_DEL_THING
-----END CERTIFICATE-----
)PEM";
static const char AWS_PRIVATE_KEY[] = R"PEM(
-----BEGIN RSA PRIVATE KEY-----
PEGAR_AQUI_CLAVE_PRIVADA_Y_SU_ENCABEZADO_REAL
-----END RSA PRIVATE KEY-----
)PEM";

// Conexion para ESP32 DevKit V1 y lector MFRC522/RC522 SPI.
static constexpr int PIN_RFID_SS = 21;
static constexpr int PIN_RFID_RST = 22;
static constexpr int PIN_RFID_SCK = 18;
static constexpr int PIN_RFID_MISO = 19;
static constexpr int PIN_RFID_MOSI = 23;
static constexpr int PIN_SERVO = 13;
static constexpr unsigned long SHADOW_REPORT_INTERVAL_MS = 5000;

// Angulos del SG90 definidos en DoorHardware.cpp: 60 y 120 grados.
