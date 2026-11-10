# Práctica 3 — Puerta inteligente IoT

Prototipo académico de SIS-234 con **ESP32, RFID-RC522 y servomotor SG90**, conectado a AWS IoT Core mediante MQTT/TLS y Device Shadow clásico. Lambda consulta y modifica el Shadow; DynamoDB vincula cada usuario con su Thing. La integración con **Alexa Custom Skill es-ES está preparada, pero pendiente de configuración y pruebas reales**.

El RFID registra lecturas y permite control local: la tarjeta autorizada solicita **desbloqueo a 120°** y el llavero autorizado solicita **bloqueo a 60°**. Un UID desconocido se registra sin accionar el servo. Se conserva el control remoto por Shadow y ambos caminos reutilizan `requestLockState()`. Al arrancar se solicita 90° y el cerrojo permanece `desconocido` hasta finalizar un comando válido. Los ángulos son consignas del software, sin confirmación física.

La orden RFID pendiente tiene prioridad hasta comprobar mediante GET que `desired.cerrojo` y `reported.cerrojo` coinciden. Se conserva en RAM durante desconexiones, no tras reinicio. Después se aceptan órdenes remotas normalmente. Véanse la [política de sincronización y el plan de pruebas](docs/CONTROL_RFID_LOCAL.md), incluidos conflictos y tarjeta mantenida ante el lector.

## Estado y evidencias

- `REGISTRO_LIMPIEZA.md` registra compilación PlatformIO exitosa, ocho pruebas Python aprobadas y validación del JSON Alexa.
- El equipo ha declarado pruebas satisfactorias de Wi-Fi, autenticación AWS, RFID, intercambio con Shadow, órdenes SG90, consultas y control desde Lambda y vinculación de un usuario de prueba en DynamoDB. Las capturas y los registros correspondientes deben incorporarse al informe; no se consideran evidencias adjuntas.
- Reconexión, estabilidad y latencias requieren ensayos documentados. Alexa no se presenta como terminada.
- El hardware actual no incluye sensor de apertura física. `EstadoPuertaIntent` se conserva por compatibilidad y no demuestra si la puerta está abierta o cerrada.

## Integrantes

- [Completar integrante 1 y contribución]
- [Completar integrante 2 y contribución]
- [Completar integrante 3 y contribución]

## Estructura

```text
firmware/             PlatformIO C++: DoorHardware, CloudShadow y main
lambda/               Backend Python: vinculación, consultas y órdenes
alexa/                Modelo de interacción es-ES pendiente de integración
aws/                  Plantillas locales; no acreditan configuración desplegada
docs/                 Guías e informe académico
tests/                Pruebas unitarias aisladas de AWS
REGISTRO_LIMPIEZA.md   Decisiones y comprobaciones de la limpieza final
```

## Uso y comprobaciones locales

1. Consultar la [guía de armado](docs/GUIA_ARMADO.md) y la [guía AWS y Alexa](docs/GUIA_AWS_ALEXA.md).
2. Para preparar un dispositivo nuevo, copiar localmente `firmware/include/config.example.h` a `config.h` y completar la configuración privada. No sobrescribir una configuración existente ni compartirla.
3. Desde `firmware/`, ejecutar `pio run -e esp32doit-devkit-v1`. Solo se conserva ese entorno de producción.
4. Desde la raíz, ejecutar `python -m unittest discover -s tests -v`. Las pruebas emplean dobles de servicios y no invocan AWS.
5. La carga con `pio run -t upload` y el monitor `pio device monitor -b 115200` requieren hardware y configuración local; no forman parte de la validación documental.
6. Incorporar evidencias anonimizadas en los espacios previstos en [docs/informe_final.md](docs/informe_final.md).

En la limpieza se utilizó Python 3.11 del entorno de PlatformIO porque el comando `python` del sistema no era ejecutable. Las guías de despliegue no implican que se hayan ejecutado sus pasos.

## Contrato actual del Shadow

El prefijo MQTT es `$aws/things/THING_NAME/shadow`. El [ejemplo local](aws/shadow-example.json) es ilustrativo y no contiene un estado observado.

| Campo | Tipo y valores | Origen y significado |
|---|---|---|
| `state.desired.cerrojo` | `bloqueada` / `desbloqueada` | Backend remoto o reconciliación de una orden RFID local |
| `state.reported.cerrojo` | `desconocido`, `bloqueada` / `desbloqueada` | Estado comandado por software; no confirma posición física |
| `state.reported.tarjetasLeidas` | Entero no negativo | Contador de lecturas desde el reinicio |
| `state.reported.ultimoRFID` | `ninguno`, `tarjeta`, `llavero`, `desconocido` | Clasificación local, sin publicar el UID |
| `state.reported.rfid_available` | Booleano | Comprobación del registro de versión al iniciar, no diagnóstico continuo |
| `state.cerrojo` del mensaje `/update/delta` | `bloqueada` / `desbloqueada` | Diferencia notificada al ESP32 para su aplicación |

El ESP32 mantiene los mismos tópicos. Al conectar solicita el Shadow y aplica `desired` si no hay una orden RFID pendiente; en ese caso reconcilia la orden local primero. Usa `clientToken` para asociar las respuestas GET y `version` para condicionar la actualización local. Después comprueba ambos estados mediante GET, sin añadir suscripciones ni permisos. Reporta por cambios y con respaldo configurado de 5 s, condicionado a conexión MQTT, sincronización y ausencia de movimiento pendiente. Ese intervalo no garantiza una latencia máxima.

No se publica `reported.puerta`. Un valor antiguo de esa propiedad podría permanecer en AWS porque actualizar otras propiedades no lo elimina; las respuestas de `EstadoPuertaIntent` no deben usarse como medición actual.

## Hardware y dependencias

RC522: SS GPIO21, RST GPIO22, SCK GPIO18, MISO GPIO19 y MOSI GPIO23. SG90: señal GPIO13, alimentación externa de 5 V y masa común según la guía. El RFID es el único sensor del montaje actual.

`platformio.ini` declara Espressif32 6.5.0, ESP32Servo 1.1.0, PubSubClient `^2.8`, ArduinoJson `^7.0.0` y MFRC522 `^1.4.10`. Los ángulos efectivos están en `DoorHardware.cpp`. No se han cambiado el core ni las bibliotecas durante la documentación.

## Seguridad y colaboración

Es una simulación académica sin realimentación mecánica. No constituye una cerradura de seguridad. No publicar `config.h`, certificados de dispositivo, claves privadas, contraseñas, tokens ni copias de esos archivos. No incluir secretos en evidencias. `.gitignore` reduce inclusiones accidentales, pero no elimina secretos previamente versionados.

Cada integrante debe usar su cuenta para sus contribuciones reales. No se crean commits ni se hace push automáticamente. La limpieza se realizó en una carpeta sin `.git`, por lo que no se pudo auditar el historial.

## Documentación

- [Informe académico y evidencias pendientes](docs/informe_final.md)
- [Guía de armado](docs/GUIA_ARMADO.md)
- [Guía AWS y Alexa](docs/GUIA_AWS_ALEXA.md)
- [Registro de limpieza](REGISTRO_LIMPIEZA.md)
- [Control RFID local, conflictos y pruebas](docs/CONTROL_RFID_LOCAL.md)
- [Instrucciones para mantenimiento](docs/PROMPT_CODEX.md)
