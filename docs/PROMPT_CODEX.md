# Instrucciones de mantenimiento para Codex

Abrir la raíz del proyecto en VS Code y consultar `AGENTS.md` antes de trabajar.

> Revisa README.md, docs/GUIA_ARMADO.md, docs/GUIA_AWS_ALEXA.md, docs/informe_final.md y REGISTRO_LIMPIEZA.md. El prototipo usa ESP32 DevKit V1, RC522 SPI (SS21, SCK18, MOSI23, MISO19 y RST22) y SG90 en GPIO13, con fuente externa de 5 V y masa común. RFID registra lecturas y controla localmente el servo: tarjeta autorizada desbloquea y llavero autorizado bloquea; UID desconocido solo se registra. Conserva el control remoto mediante AWS IoT Core Classic Shadow y utiliza requestLockState() en ambos caminos. Una orden RFID pendiente prevalece hasta verificar desired y reported coincidentes; se conserva en RAM al reconectar, no tras reinicio. Después se aceptan órdenes remotas normalmente. Conserva la supresión de repeticiones por tarjeta sostenida y consulta docs/CONTROL_RFID_LOCAL.md. Conserva bloqueo a 60°, desbloqueo a 120°, UID, tópicos y propiedades públicas. La posición inicial de 90° y reported.cerrojo no constituyen confirmación mecánica. Alexa está preparada, pero pendiente de configuración y pruebas reales. No presentes EstadoPuertaIntent como una medición de apertura, porque el hardware actual no la realiza. Mantén las clases C++ y Python, incorpora regresiones si modificas Lambda y documenta las decisiones. No sustituyas AWS por MQTT público ni MCP. No accedas ni muestres credenciales, certificados, claves o config.h para tareas documentales. No modifiques recursos AWS ni hagas upload sin una tarea que lo requiera. Ejecuta comprobaciones proporcionales al cambio y distingue inspección de código, pruebas unitarias, resultados registrados, declaraciones del equipo y pruebas reales con evidencias. No declares cumplidos requisitos sin demostración. Cada integrante realiza sus propios commits; no crees commits ni hagas push en nombre de otros.

## Distribución sugerida de contribuciones reales

- Integrante A: montaje, firmware y evidencias de RFID y servo.
- Integrante B: AWS IoT Core, Lambda, DynamoDB y evidencias del backend.
- Integrante C: preparación de Alexa, pruebas, diagramas e informe.

Los nombres y las contribuciones efectivas deben completarse en el informe. No atribuir cambios ficticios ni registrar varios autores para simular trabajo colaborativo.
