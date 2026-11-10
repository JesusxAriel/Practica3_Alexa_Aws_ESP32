# Instrucciones para Codex — Práctica 3 SIS-234

## Objetivo
Mantener y completar un prototipo académico de puerta inteligente con ESP32, RC522 como único sensor, SG90, AWS IoT Core Classic Shadow, Lambda Python y DynamoDB. Alexa Custom Skill en español está preparada, con configuración y pruebas reales pendientes.

## Reglas esenciales
- Leer primero README.md, docs/GUIA_ARMADO.md y docs/GUIA_AWS_ALEXA.md.
- Actuador: acepta AWS IoT Device Shadow y control RFID local autorizado, ambos mediante `requestLockState()`. Tarjeta autorizada: desbloqueo a 120°; llavero autorizado: bloqueo a 60°; UID desconocido: solo registro. El posicionamiento inicial de 90° no confirma seguridad ni posición física.
- Una orden RFID pendiente prevalece hasta verificar desired y reported coincidentes; conservarla en RAM durante reconexiones y documentar que no persiste tras reinicio. Conservar el control remoto posterior y la supresión de repeticiones por tarjeta sostenida. No usar MQTT público/HiveMQ/MCP como sustituto de AWS IoT.
- Mantener pines RC522: SS21, RST22, SCK18, MISO19, MOSI23; servo GPIO13. El montaje actual no incluye Reed ni medición de apertura física. Cambios de pines requieren actualizar guía y configuración.
- Un ESP32 por Thing; cada usuario de Alexa resuelve su propio Thing mediante DynamoDB.
- No versionar `firmware/include/config.h`, certificados PEM, claves, contraseñas, tokens, archivos .env reales ni credenciales AWS. No imprimir secretos en logs.
- Conservar módulos y clases; incorporar pruebas de regresión para cambios de Lambda y documentación de las decisiones.
- No declarar compilación PlatformIO, pruebas de hardware, tiempos medidos, reconexión o integración en AWS como exitosos si no se ejecutaron efectivamente.
- No crear commits ni efectuar push en nombre de otros integrantes; cada cual usa su cuenta para cambios reales. No hacer push sin la aprobación del equipo.

## Comprobaciones
Desde la raíz: `python -m unittest discover -s tests -v`.
Desde `firmware/`: `pio run`; `pio run -t upload`; `pio device monitor -b 115200` (solo con hardware y configuración local).
Antes de cada commit: revisar `git status`, `git diff --check` y `git ls-files`, asegurando que no haya secretos.

## Entregables y fuentes de verdad
- `docs/informe_final.md`: informe técnico, trazabilidad del alcance RF1-RF9 sin atribuir cumplimiento no demostrado, diagramas, despliegue, plan de pruebas, resultados reales, conclusiones, recomendaciones y anexos.
- `alexa/interaction-model.es-ES.json`: intents y utterances.
- `aws/*.example.json`: plantillas de políticas, estado Shadow y persistencia.
- `docs/GUIA_ARMADO.md` y `docs/GUIA_AWS_ALEXA.md`: instrucciones verificables.

Priorizar primero un montaje estable y el flujo extremo a extremo, después pulir el informe y las evidencias.
