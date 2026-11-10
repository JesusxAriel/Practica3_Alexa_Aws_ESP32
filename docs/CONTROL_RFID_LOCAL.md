# Control local RFID y convivencia con AWS

## Comportamiento

| Lectura aceptada | Acción solicitada | Registro conservado |
|---|---|---|
| Tarjeta UID `39 61 D7 06` | `desbloqueada`, 120° | Contador y `ultimoRFID=tarjeta` |
| Llavero UID `9C 31 8B 06` | `bloqueada`, 60° | Contador y `ultimoRFID=llavero` |
| Otro UID | Ninguna orden | Contador y `ultimoRFID=desconocido` |

Las dos acciones autorizadas utilizan `requestLockState()` y el intervalo no bloqueante existente de al menos 850 ms. Una petición cuyo objetivo ya coincide no repite el comando ni reinicia su temporizador. Se conserva la posición inicial solicitada de 90° y el estado `desconocido`. Ningún estado acredita posición física ni bloqueo mecánico.

Se consulta presencia mediante WUPA cada 150 ms, incluso si la tarjeta está en HALT. El mismo UID se registra una sola vez por presentación, hasta detectar al menos 750 ms sin respuesta. Se mantiene un mínimo de 1200 ms entre lecturas aceptadas. Un UID distinto puede registrarse como otra presentación. Errores distintos de timeout no rearman la lectura. Probar un objeto a la vez: ruido de RF o varios objetos pueden afectar la detección y requieren validación física.

## Política de conflictos y sincronización

1. Una lectura autorizada ejecuta la orden local y notifica su objetivo a `CloudShadow` antes de procesar MQTT en ese ciclo.
2. Mientras haya una orden local pendiente, no se aplica un desired remoto que pudiera revertirla. Una nueva lectura autorizada reemplaza el objetivo local pendiente; una desconocida no lo modifica.
3. El ESP32 solicita el Shadow por los tópicos existentes. Cada GET lleva un `clientToken`; una respuesta correspondiente a otra consulta no se aplica.
4. Al terminar el comando, publica **desired.cerrojo y reported.cerrojo en el mismo update**, junto con las propiedades RFID habituales. Si existe el Shadow, incluye la versión leída para una actualización condicional. Un conflicto provoca consulta/reintento, conservando la prioridad local.
5. La llamada a `publish()` no confirma persistencia. Se solicita otro GET y solo se libera la prioridad local cuando su respuesta muestra ambos campos coincidentes y el servo no tiene comando pendiente. La versión observada permite descartar deltas antiguos posteriores.
6. Después de esa comprobación vuelve a aceptarse el control remoto. Sin orden RFID pendiente, un desired recuperado al reconectar sigue aplicándose como antes.

**Regla explícita:** una orden remota opuesta durante la ventana local pendiente puede ser sustituida por la local. En una desconexión no existe un orden cronológico global entre eventos de ambos lados; se elige prioridad local pendiente. Enviar la orden remota después del mensaje serial `Orden RFID sincronizada` permite verificar el control remoto posterior sin ambigüedad. No se promete prioridad cronológica de la última orden emitida en cualquier origen.

La orden pendiente permanece en RAM durante pérdidas de Wi-Fi/MQTT y se reconcilia antes de aplicar desired antiguo al reconectar. **No persiste tras reinicio o corte de alimentación**: en ese caso se recupera desired de AWS. Sin red, permisos válidos o respuesta GET no se declara sincronizada la orden. Si AWS rechaza repetidamente la reconciliación, el control físico local sigue disponible y la prioridad local permanece pendiente; deben revisarse los logs y la conectividad.

No se añaden tópicos ni suscripciones: se mantienen `/get`, `/update`, `/get/accepted`, `/get/rejected`, `/update/delta` y `/update/rejected`. No se modificaron políticas, recursos desplegados, Lambda, DynamoDB, JSON Alexa, pines ni configuración privada.

El bloqueo optimista y la correlación de respuestas se basan en la documentación de AWS sobre [actualizaciones por versión](https://docs.aws.amazon.com/iot/latest/developerguide/device-shadow-data-flow.html) y [versiones y clientToken](https://docs.aws.amazon.com/iot/latest/developerguide/device-shadow-document.html). La comprobación por GET confirma datos en AWS, no un ángulo físico.

## Plan de pruebas en el montaje — pendiente

Los casos siguientes no se ejecutaron en hardware ni AWS durante este cambio. La carga del firmware debe realizarse en el montaje preparado por el equipo; no se hizo upload automáticamente. Observar Serial a 115200 y el Shadow, sin divulgar secretos. Antes de probar, anotar estado inicial, versión, contador y conectividad.

| Caso | Procedimiento | Resultado que se debe comprobar |
|---|---|---|
| Tarjeta | Presentar tarjeta autorizada desde un estado bloqueado | Una lectura, clasificación tarjeta, solicitud 120°, finalización software y desired/reported desbloqueada |
| Llavero | Retirar tarjeta y presentar llavero | Una nueva lectura, clasificación llavero, solicitud 60° y ambos estados bloqueada |
| UID desconocido | Retirar el objeto anterior y presentar otro UID | Aumenta contador y cambia clasificación, sin nueva orden ni escritura PWM |
| Tarjeta sostenida | Mantener un objeto al menos varios segundos | Una presentación registrada y sin comandos periódicos repetidos |
| Nueva presentación | Retirar por al menos 750 ms y volver a presentar, respetando el intervalo de 1200 ms | Nueva lectura; si el estado ya coincide, no se repite movimiento |
| AWS después de RFID | Esperar `Orden RFID sincronizada`; enviar desired opuesto desde AWS | Se aplica el comando remoto y se reporta; no se reescribe desired como si hubiera otra lectura local |
| AWS con tarjeta sostenida | Mantener tarjeta después de sincronizar y enviar bloqueo remoto | El bloqueo remoto no se revierte por la tarjeta que sigue presente; retirarla y volverla a presentar permite una nueva acción |
| Desired anterior opuesto | Dejar desired bloqueada y presentar tarjeta | No debe volver a bloquear al finalizar el comando local; ambos campos deben converger a desbloqueada |
| Lectura sin conexión | Cortar Wi-Fi sin reiniciar, presentar tarjeta o llavero y recuperar red | Acción local disponible y objetivo pendiente conservado; al reconectar converge local antes de aceptar desired antiguo |
| Pérdida durante publicación | Interrumpir red después del comando y antes del GET de comprobación | No se da por confirmada la sincronización; se comprueba/reintenta al reconectar |
| Nueva orden local pendiente | Sin red, presentar tarjeta y luego llavero | Prevalece la última presentación autorizada; una posterior desconocida no cambia el objetivo |
| Conflicto de versión | Enviar una orden remota mientras se reconcilia la local | Puede aparecer 409; debe conservarse el objetivo local, consultar y reintentar sin volver al desired antiguo |
| Reinicio | Reiniciar después de un comando local no sincronizado | La orden RAM no persiste; se solicita 90° inicial y se recupera desired remoto, según el comportamiento documentado |
| Alexa | Tras convergencia, vincular si procede, consultar contador y cerrojo, bloquear y desbloquear mediante la skill | Lambda y DynamoDB mantienen sus interfaces y el ESP32 aplica las órdenes remotas; verificar intent reconocido además de respuesta de voz |

Las evidencias previas de Alexa no sustituyen estos ensayos de convivencia con el firmware ampliado. Registrar desired, reported, versión, mensajes seriales, condiciones y resultado observado. El movimiento visible del SG90 no demuestra el ángulo solicitado ni la retención bajo carga.

## Validación local de esta ampliación

- `pio run -e esp32doit-devkit-v1`: **SUCCESS**. RAM 45 768 bytes (14,0 %); flash 914 801 bytes (69,8 %). Son datos de construcción, no consumo máximo medido durante ejecución.
- Ocho pruebas existentes de Lambda aprobadas con Python 3.11 del entorno PlatformIO. No validan el RC522 ni la nueva reconciliación C++.
- No se ejecutaron upload, monitor serial, pruebas de hardware ni llamadas a servicios AWS. No se dispone de un compilador nativo para ejecutar una simulación C++ local en esta sesión; el resultado de build no sustituye las pruebas de la tabla.

La copia pública previa está en `.cleanup-backups/antes-rfid-local.zip` y no incluye certificados ni configuración privada. Los cambios de firmware se limitan a `DoorHardware.cpp/.h` y `CloudShadow.cpp/.h`; `main.cpp` conserva su secuencia de llamadas.
