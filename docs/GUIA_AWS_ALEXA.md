# AWS IoT Core + Lambda + Alexa: pasos de configuración

> Esta guía asume una cuenta AWS con permisos apropiados, una cuenta Alexa Developer y acceso de red Wi-Fi 2,4 GHz. No usar claves AWS IAM en firmware ni Lambda. Confirmar región en cada servicio.

**Estado del proyecto:** el backend y el modelo es-ES están implementados en el repositorio. Alexa permanece pendiente de configuración y pruebas reales por los problemas de acceso a Amazon Developer declarados por el equipo. Los pasos siguientes son instrucciones de preparación y ensayo; no acreditan acciones ejecutadas. Esta actualización documental no modifica AWS.

## 1. AWS IoT Core

1. Elegir una región AWS y usarla en **IoT Core, DynamoDB y Lambda**. Establecer alarma/budget de facturación.
2. Abrir **AWS IoT Core → Manage → All devices → Things → Create things** y crear `puerta-sis234-g1-01` (o nombre con prefijo `puerta-sis234-`).
3. Crear certificado X.509 y claves para el Thing; descargar certificado, clave privada y **Amazon Root CA 1**; activar certificado; asociarlo al Thing.
4. Editar `aws/iot-policy.example.json`: cambiar `REGION`, `ACCOUNT_ID`, `THING_NAME`; crear la política en IoT Core y adjuntarla al certificado.
5. En **Settings → Device data endpoint** copiar `xxxxxxxx-ats.iot.REGION.amazonaws.com`.
6. En `firmware/include/`, copiar `config.example.h` como `config.h`, completar SSID, clave Wi-Fi, endpoint, Thing Name y PEM. **No guardar la clave en Git.** Conservar los encabezados PEM reales (`BEGIN PRIVATE KEY` o `BEGIN RSA PRIVATE KEY`, según corresponda).
7. Compilar y subir firmware con PlatformIO; abrir monitor serial a 115200.
8. Revisar **Things → Thing → Device Shadows → Classic shadow**. El primer `reported` crea el Shadow si no existe.

## 2. Qué está haciendo el firmware

El ESP32 se conecta vía MQTT/TLS con certificado X.509 en el puerto 8883. Se suscribe al Shadow clásico `$aws/things/THING_NAME/shadow/update/delta`, a `/get/accepted`, `/get/rejected` y `/update/rejected`.

Al conectar MQTT publica en `/get`, con `clientToken`, y aplica `state.desired.cerrojo` al recibir la respuesta correspondiente si no hay una orden RFID local pendiente. Los comandos posteriores llegan en `state.cerrojo` de `/update/delta`; se descartan versiones ya vistas. El reporte se permite cuando MQTT está conectado, ha terminado la espera de sincronización y no hay movimiento pendiente. Se intenta por cambios y con respaldo configurado de 5 segundos; no es una garantía temporal bajo fallos de red.

La tarjeta autorizada solicita desbloqueo local y el llavero autorizado bloqueo, reutilizando el mismo controlador. Una orden local pendiente prevalece sobre desired anterior, incluso al reconectar sin reiniciar. Al finalizar el comando, el ESP32 escribe desired y reported en una actualización condicionada por la versión obtenida mediante GET; comprueba la coincidencia mediante otro GET antes de liberar la prioridad local. Un conflicto de versión conserva la orden pendiente y se vuelve a consultar/reintentar. Durante esa ventana una orden remota opuesta puede ser sustituida; después de la confirmación se acepta el control remoto normal. No se promete orden cronológico global entre eventos ocurridos durante una desconexión. No hay persistencia de la orden pendiente tras reinicio.

Este mecanismo mantiene los tópicos existentes y no requiere añadir `/update/accepted` ni modificar políticas desplegadas. Véase [control RFID local y pruebas](CONTROL_RFID_LOCAL.md).

El RC522 es el único sensor actual. Cada lectura aceptada incrementa `tarjetasLeidas` y actualiza `ultimoRFID` como `tarjeta`, `llavero` o `desconocido`, sin publicar el UID. El estado inicial es `ninguno`; la cuenta reinicia con el ESP32. `rfid_available` refleja la comprobación del registro de versión al arrancar y no se actualiza continuamente.

El servo recibe 60° para `bloqueada` y 120° para `desbloqueada`. Al arrancar se solicita 90° y se conserva `cerrojo=desconocido`. Tras al menos 850 ms desde una orden válida, el software actualiza su estado y deja de emitir pulsos con `detach()`. Estos valores son parámetros del código, no mediciones mecánicas.

Ejemplo del comando esperado en `desired`: `{"state":{"desired":{"cerrojo":"desbloqueada"}}}`. Los estados del actuador se limitan a `bloqueada` y `desbloqueada`.

El firmware reporta **la posición ordenada al servo**, no confirma un pestillo mecánico real: el SG90 de tres hilos no tiene retroalimentación externa de posición.

No publica `reported.puerta`. `EstadoPuertaIntent` permanece por compatibilidad: si no existe ese campo, responde que no dispone de lectura; si AWS conserva un valor histórico, podría devolverlo aunque no exista medición actual. No usar esa intención para demostrar apertura física ni eliminar campos del Shadow desplegado como parte de esta tarea documental. El [ejemplo local de Shadow](../aws/shadow-example.json) refleja las propiedades actuales y es ilustrativo, no una captura real.

## 3. DynamoDB — tabla única

1. Crear `PuertaAlexaBindings` con llave de partición `pk` de tipo `String` (schema en `aws/dynamodb-schema.example.json`).
2. Agregar manualmente registro de catálogo de ejemplo: `pk` = `CODE#482731`, `thingName` = `puerta-sis234-g1-01` (el Thing debe existir). Elegir un código único de **6 cifras sin cero inicial**; usar el mismo para probar con Alexa.
3. El Lambda crea automáticamente el registro por cuenta: `pk` = `USER#<context.System.user.userId>`, `thingName`, `fechaVinculacion`.
4. Cambiar de dispositivo se realiza volviendo a ejecutar `VincularIntent` con otro código. Desvincular elimina el registro USER# correspondiente.

**Seguridad de demo:** un código compartido permite vincular el Thing desde otra cuenta. Para producción se requiere limitar intentos, códigos temporales o de un solo uso, y autorización de propietario.

## 4. AWS Lambda

1. Crear Lambda **Python 3.12 o superior**, misma región que IoT/DynamoDB; handler: `lambda_function.lambda_handler`.
2. Subir o copiar el archivo `lambda/lambda_function.py`; `boto3` está disponible en el runtime administrado por AWS Lambda. Para despliegue propio fuera de Lambda, instalarlo.
3. Configurar variables de entorno:
   - `TABLE_NAME=PuertaAlexaBindings`
   - `IOT_DATA_ENDPOINT=xxxxxxxx-ats.iot.REGION.amazonaws.com`
4. Asignar un rol con permisos adaptados de `aws/lambda-iam-policy.example.json` y permisos de CloudWatch Logs, por ejemplo la política administrada `AWSLambdaBasicExecutionRole`. Reemplazar REGION y ACCOUNT_ID; revisar alcance del prefijo en Thing ARN.
5. Verificar que **GetThingShadow/UpdateThingShadow** apunten al mismo endpoint de datos de AWS IoT Core.
6. Agregar trigger **Alexa Skills Kit** y restringirlo al ID de **su propia skill**.

## 5. Alexa Developer Console — integración pendiente

1. Crear skill con modelo **Custom** y lenguaje **Spanish (Spain), es-ES**, para usar el JSON incluido. Nombre de invocación propuesto: `puerta conectada`.
2. En **Build → JSON Editor** pegar el contenido de `alexa/interaction-model.es-ES.json`; guardar y pulsar **Build Model**.
3. En Endpoint, elegir AWS Lambda ARN y colocar el ARN de la función. Asegurar permisos para invocación Alexa Skills Kit.
4. En Test habilitar entorno de desarrollo y seleccionar locale es-ES; probar comandos escritos y, cuando esté disponible, hablados.
5. Verificar el valor de los slots: `AMAZON.NUMBER` puede interpretar el código hablado de distinta forma. Si se desea otro locale, adaptar el modelo y probar las frases para ese idioma.

## 6. Plan de comprobación de extremo a extremo — pendiente con Alexa

1. Sin vincular, consultar la puerta: debe solicitar un código.
2. Vincular código `482731` (si fue ese el registrado). Verificar en DynamoDB el item `USER#...`.
3. Pedir `desbloquea la cerradura` y revisar el Shadow `desired`.
4. Comprobar que el ESP32 recibe `/update/delta`, solicita 120° y publica `reported.cerrojo=desbloqueada` tras finalizar el intervalo de comando. Repetir bloqueo a 60°. No interpretar el reporte como confirmación física.
5. Preguntar `dime el estado del cerrojo` y `cuántas tarjetas se han leído`; contrastar las respuestas con `reported`. Presentar tarjeta y llavero y observar el contador. La consulta de apertura física no forma parte del hardware actual.
6. Desconectar Wi-Fi, enviar orden desde Alexa, reconectar y verificar aplicación de `desired` pendiente.
7. Desvincular y comprobar que las consultas posteriores piden vincular de nuevo.

Además, ejecutar el plan de convivencia RFID/AWS: orden local con desired opuesto anterior, tarjeta sostenida, orden remota posterior a sincronización, desconexión durante un comando y conflicto concurrente. Los ensayos están descritos en `CONTROL_RFID_LOCAL.md`; su éxito no se ha verificado en hardware ni AWS como parte de este cambio.

## 7. Problemas comunes

- MQTT -2 / fallo TLS: verificar NTP, endpoint sin `https://`, certificados PEM completos, política adjunta, Thing name y Wi-Fi 2,4 GHz.
- Shadow no aparece: verificar `iot:Publish` en `.../shadow/update` y logs seriales.
- 403 en Lambda: rol IAM, cuenta/ARN/endpoint/región; revisar CloudWatch.
- Alexa no vincula: comprobar slots reconocidos en el simulador, formato de código y registro `CODE#`.
- Consulta de apertura sin lectura o con valor antiguo: el firmware no publica `puerta`; `EstadoPuertaIntent` no representa una medición actual.
- Servo reinicia ESP32: revisar fuente independiente 5 V con masa común; no alimentar servo desde 3V3.

Referencias oficiales:
- https://docs.aws.amazon.com/iot/latest/developerguide/device-shadow-mqtt.html
- https://docs.aws.amazon.com/iot/latest/developerguide/device-shadow-comms-device.html
- https://docs.aws.amazon.com/iot/latest/developerguide/protocols.html
- https://developer.amazon.com/en-US/docs/alexa/custom-skills/steps-to-build-a-custom-skill.html
- https://docs.aws.amazon.com/boto3/latest/reference/services/iot-data/client/get_thing_shadow.html

## 8. Compatibilidad de librerías

El ejemplo fija `espressif32@6.5.0` y `madhephaestus/ESP32Servo@1.1.0`, combinación conocida para Arduino-ESP32 2.x. No actualizar solo la librería a ESP32Servo 3.x sin actualizar simultáneamente el core de Arduino y comprobar la compilación.
