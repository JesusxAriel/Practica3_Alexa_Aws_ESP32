# Guía de armado — ESP32, RC522 y SG90

Esta guía describe el cableado previsto por el código actual y las precauciones de montaje. No sustituye las fotografías ni acredita las conexiones físicas realizadas. El montaje utiliza RFID-RC522 como único sensor y SG90 como único actuador; no mide la apertura física de la puerta.

## Conexión del RC522

| Pin RC522 | ESP32 | Observaciones |
|---|---|---|
| 3.3V | 3V3 | Nunca alimentar el lector con 5 V |
| GND | GND | Masa común |
| SDA / SS | GPIO21 | Selección SPI, no bus I2C |
| SCK | GPIO18 | Reloj SPI |
| MOSI | GPIO23 | Datos hacia el lector |
| MISO | GPIO19 | Datos hacia el ESP32 |
| RST | GPIO22 | Reinicio del lector |
| IRQ | Sin conectar | No utilizado |

La configuración corresponde a ESP32 DevKit V1 y RC522 SPI. No aplicar este esquema a un PN532 u otra placa sin revisar su compatibilidad. No se requiere conexión a GPIO27 en esta versión.

## Conexión del SG90

| Conductor típico | Conexión prevista |
|---|---|
| Naranja/amarillo, señal | GPIO13 |
| Rojo, alimentación | +5 V regulados externos |
| Marrón/negro, masa | GND de la fuente, unido a GND del ESP32 |

Verificar la identificación de los conductores del servo utilizado. El ESP32 se alimenta por USB y el servo mediante una fuente externa adecuada; la guía base propone alrededor de 1 A o más de capacidad. No se dispone de medidas de consumo del montaje. No alimentar el servo desde 3V3 ni aplicar 5 V a un GPIO.

## Consignas del firmware

| Situación | Ángulo solicitado | Estado de software |
|---|---|---|
| Arranque | 90° | `desconocido` |
| Orden Shadow `bloqueada` o llavero autorizado | 60° | Se actualiza tras finalizar el intervalo de comando |
| Orden Shadow `desbloqueada` o tarjeta autorizada | 120° | Se actualiza tras finalizar el intervalo de comando |

Las constantes se encuentran en `firmware/src/DoorHardware.cpp`. El servo se configura a 50 Hz con límites de pulsos de 500 y 2400 µs. Tras al menos 850 ms desde un comando válido, el programa actualiza el estado y ejecuta `detach()`: deja de emitir pulsos, sin cortar la alimentación. Los 850 ms son un parámetro de software, no un tiempo de desplazamiento medido. No se ha convertido el SG90 en un servo de rotación continua.

El posicionamiento inicial es una consigna del prototipo; no garantiza bloqueo ni una posición segura del mecanismo real. No hay sensor que confirme la posición alcanzada.

## Procedimiento de montaje y verificación

1. Identificar la placa y el puerto del ESP32. Desconectar alimentaciones antes de cablear.
2. Conectar RC522 a 3V3, GND y los GPIO de la tabla. Usar conexiones firmes y cables cortos.
3. Con la fuente del servo apagada, conectar alimentación, señal y masa común. Revisar el cableado antes de energizar.
4. Compilar el entorno `esp32doit-devkit-v1`. Cargarlo únicamente cuando se disponga de hardware y configuración local autorizados.
5. En el monitor de 115200 baudios, observar la inicialización del lector y presentar la tarjeta y el llavero configurados. Registrar clasificación y contador sin divulgar datos privados.
6. Probar tarjeta autorizada (desbloqueo), llavero autorizado (bloqueo) y UID desconocido (solo registro). Mantener una tarjeta ante el lector y comprobar que no repite lecturas ni movimientos; retirarla al menos 750 ms, considerando el sondeo de 150 ms y el intervalo mínimo de 1200 ms entre lecturas aceptadas.
7. Enviar órdenes a `desired.cerrojo` mediante AWS después de verificar la sincronización local. Observar que el control remoto sigue operativo. Seguir el [plan de pruebas de convivencia](CONTROL_RFID_LOCAL.md).
8. Documentar el montaje y las observaciones reales en el [informe](informe_final.md), sin atribuir precisión angular a una observación visual.

No instalar el prototipo como elemento de protección de una puerta real. La comprobación de reconexión, estabilidad y comportamiento mecánico requiere ensayos adicionales y evidencias.
