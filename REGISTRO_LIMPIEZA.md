# Registro de limpieza — 10/11/2026

Se priorizaron cambios mínimos sobre el firmware existente. No se modificaron
`docs/`, el informe, AWS, Lambda, el modelo de Alexa ni las pruebas automatizadas.

## Recuperación

Antes de editar se guardaron los archivos afectados y los seis diagnósticos en
`.cleanup-backups/antes-limpieza-20261110-092751.zip`. No incluye `config.h` ni
certificados locales. La copia también contiene hashes para verificar `docs/`.
Esta carpeta no contiene `.git`: no se crearon commits y no se pudo auditar
el historial ni los archivos previamente versionados.

## Cambios

- Retirados de `firmware/src/diagnostico/`: `prueba_esp32.cpp`, `prueba_wifi.cpp`,
  `prueba_aws.cpp`, `prueba_rfid.cpp`, `prueba_servo.cpp` y `prueba_rfid_servo.cpp`.
- `firmware/platformio.ini`: conserva únicamente `esp32doit-devkit-v1` con las
  mismas dependencias; elimina los seis entornos de diagnóstico y sus filtros.
- `DoorHardware.h`: elimina `doorState()`, método de compatibilidad sin usuarios.
- `DoorHardware.cpp`: aclara que la posición inicial de 90° no confirma una
  posición física. Conserva comandos a 60° y 120°, tiempos y UID existentes.
- `CloudShadow.cpp`: elimina el include de ArduinoJson duplicado en su cabecera.
- `CloudShadow.h`: elimina un comentario redundante.
- `config.example.h`: elimina constantes sin uso del Reed y ángulos antiguos
  de 15°/95°; señala dónde se definen los ángulos efectivos.
- `.gitignore`: amplía exclusiones para certificados, variantes de configuración
  privada, copias de seguridad y la copia local de recuperación.

Se conservan RFID como sensor de registro, contador, identificación de tarjeta y
llavero, GPIO, MQTT/TLS, tópicos y propiedades `tarjetasLeidas`, `ultimoRFID`,
`rfid_available` y `cerrojo`. Solo Shadow ordena el servo. `reported.cerrojo`
representa el comando finalizado por software, sin realimentación mecánica.

## Validación y pendientes

- Compilación `esp32doit-devkit-v1`: SUCCESS. RAM 45 672 bytes (13,9 %),
  flash 910 473 bytes (69,5 %). Una advertencia del core Arduino en
  `esp32-hal-uart.c` por un retorno sin valor; no se modificó esa dependencia.
- Ocho pruebas de Lambda aprobadas localmente con Python 3.11 de PlatformIO.
- Sintaxis Python y JSON Alexa válidos; intents personalizados presentes en Lambda.
- Sin referencias a diagnósticos retirados ni a `doorState()` en producción.
- Contenido de `docs/` comprobado mediante hashes, sin modificaciones.
- La revisión de fuentes publicables no encontró marcadores de claves privadas
  ni identificadores AWS del patrón comprobado; no constituye una auditoría completa.
- README y guías aún describen Reed y ángulos antiguos; se dejan intactos según
  el alcance solicitado. `EstadoPuertaIntent` se conserva para compatibilidad;
  el firmware actual no mide ni reporta el estado físico de la puerta.
- Alexa sigue pendiente de acceso a Amazon Developer, configuración del endpoint,
  compilación del modelo y pruebas reales de integración.
- No se ejecutó upload, monitor serial ni pruebas de hardware o servicios AWS.

## Actualización documental posterior — 10/11/2026

La observación anterior sobre documentación pendiente corresponde al cierre de la
limpieza. En una tarea posterior se actualizaron README, AGENTS y las guías para
describir RC522 como único sensor, SG90 a 60°/120° y Alexa pendiente. El borrador
`docs/INFORME.md` fue sustituido por `docs/informe.md`, con resultados trazables y
espacios para evidencias. Se actualizó únicamente la plantilla local
`aws/shadow-example.json`, no el Shadow desplegado. La documentación previa se
guardó en `.cleanup-backups/documentacion-antes-20261110.zip`, sin archivos privados.
El código funcional, las pruebas y el modelo Alexa no se modificaron.

La verificación documental comprobó por SHA-256 que los diez archivos públicos
de código, configuración de firmware, pruebas y modelo Alexa conservan su
contenido. También se comprobaron las 17 secciones del informe, 30 enlaces locales,
el cierre de bloques Markdown y la validez de los ejemplos JSON. Los tres diagramas
Mermaid se revisaron en texto; no se ejecutó un renderizador. No se repitieron
compilación, upload ni pruebas de hardware o AWS durante esta actualización.

## Ampliación posterior autorizada: RFID local

Se sustituyó la regla de RFID solo como registro por control local autorizado:
tarjeta solicita desbloqueo a 120°, llavero bloqueo a 60°, desconocidos solo registro.
Se reutiliza `requestLockState()` y se suprimen repeticiones por presentación.
Los únicos fuentes modificados son `DoorHardware.cpp/.h` y `CloudShadow.cpp/.h`.
La prioridad local pendiente se conserva en RAM durante desconexiones y se libera
al comprobar desired/reported coincidentes por GET; no persiste tras reinicio.
Se mantienen tópicos, Lambda, DynamoDB, modelo Alexa y configuración privada.

Build principal: SUCCESS, RAM 45 768 bytes (14,0 %), flash 914 801 bytes (69,8 %).
Las ocho pruebas existentes de Lambda volvieron a aprobarse; no prueban la nueva
lógica RFID/Shadow. Sin upload, pruebas físicas ni llamadas AWS de esta ampliación.
Política de conflictos, archivos y plan de convivencia en `docs/CONTROL_RFID_LOCAL.md`.
Se preservó el informe actual `docs/informe_final.md` y sus resultados de Alexa;
solo se actualizaron los apartados afectados y los enlaces al nombre existente.
Copia previa pública: `.cleanup-backups/antes-rfid-local.zip`, sin archivos privados.
