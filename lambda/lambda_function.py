"""Backend de Alexa para la Práctica 3.

Requiere rol IAM con accesos a DynamoDB, DescribeThing y Device Shadow.
Usa el Shadow clásico, nunca envía comandos MQTT directamente al ESP32.
"""
from __future__ import annotations

from datetime import datetime, timezone
import json
import logging
import os
import re

logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)
THING_NAME_PATTERN = re.compile(r"^[A-Za-z0-9:_-]{1,128}$")


def alexa_response(message: str, end_session: bool = True) -> dict:
    return {
        "version": "1.0",
        "response": {
            "outputSpeech": {"type": "PlainText", "text": message},
            "shouldEndSession": end_session,
        },
    }


class LinkRepository:
    """Mantiene CODE#codigo -> Thing y USER#userId -> Thing en una tabla."""

    def __init__(self, table):
        self.table = table

    def resolve(self, user_id: str) -> str | None:
        item = self.table.get_item(Key={"pk": "USER#" + user_id}).get("Item", {})
        return item.get("thingName")

    def thing_for_code(self, code: str) -> str | None:
        item = self.table.get_item(Key={"pk": "CODE#" + code}).get("Item", {})
        return item.get("thingName")

    def link(self, user_id: str, thing_name: str) -> None:
        self.table.put_item(Item={
            "pk": "USER#" + user_id,
            "thingName": thing_name,
            "fechaVinculacion": datetime.now(timezone.utc).isoformat(),
        })

    def unlink(self, user_id: str) -> None:
        self.table.delete_item(Key={"pk": "USER#" + user_id})


class ShadowGateway:
    """Adapter AWS IoT Core: registry y Shadow clásico."""

    def __init__(self, registry_client, data_client):
        self.registry = registry_client
        self.data = data_client

    def validate_thing(self, thing_name: str) -> None:
        if not THING_NAME_PATTERN.fullmatch(thing_name):
            raise ValueError("Nombre de Thing inválido")
        self.registry.describe_thing(thingName=thing_name)

    def get_reported(self, thing_name: str) -> dict:
        raw = self.data.get_thing_shadow(thingName=thing_name)["payload"].read()
        document = json.loads(raw)
        return document.get("state", {}).get("reported", {})

    def set_lock(self, thing_name: str, state: str) -> None:
        if state not in ("bloqueada", "desbloqueada"):
            raise ValueError("Estado de cerradura no permitido")
        body = json.dumps({"state": {"desired": {"cerrojo": state}}}).encode("utf-8")
        self.data.update_thing_shadow(thingName=thing_name, payload=body)


class SkillController:
    def __init__(self, links: LinkRepository, cloud: ShadowGateway):
        self.links = links
        self.cloud = cloud

    def handle(self, event: dict) -> dict:
        request = event.get("request") or {}
        request_type = request.get("type")
        user_id = ((event.get("context") or {}).get("System") or {}
                   ).get("user", {}).get("userId")
        if not user_id:
            user_id = (event.get("session") or {}).get("user", {}).get("userId")
        if request_type == "LaunchRequest":
            return alexa_response("Bienvenido a Puerta Conectada. Puedes vincular una puerta, "
                                  "consultar su estado o bloquear la cerradura.", False)
        if request_type == "SessionEndedRequest":
            return {"version": "1.0", "response": {}}
        if request_type != "IntentRequest":
            return alexa_response("Solicitud no reconocida.")
        intent = request.get("intent") or {}
        name = intent.get("name", "")
        if name in ("AMAZON.CancelIntent", "AMAZON.StopIntent"):
            return alexa_response("Hasta luego.")
        if name == "AMAZON.HelpIntent":
            return alexa_response("Di vincula con el código seguido de los seis dígitos, "
                                  "o pregunta si la puerta está abierta.", False)
        if not user_id:
            return alexa_response("No pude identificar la cuenta de Alexa.")
        try:
            if name == "VincularIntent":
                raw = ((intent.get("slots") or {}).get("codigo") or {}).get("value", "")
                # AMAZON.NUMBER puede convertir seis dígitos hablados a texto numérico.
                code = str(raw).strip()
                if not re.fullmatch(r"[1-9][0-9]{5}", code):
                    return alexa_response("Indica un código numérico de seis dígitos, sin cero inicial.")
                thing_name = self.links.thing_for_code(code)
                if not thing_name:
                    return alexa_response("El código no corresponde a un dispositivo registrado.")
                self.cloud.validate_thing(thing_name)
                self.links.link(user_id, thing_name)
                return alexa_response("Puerta vinculada correctamente. Ya puedes consultarla o controlarla.")
            if name == "DesvincularIntent":
                self.links.unlink(user_id)
                return alexa_response("Se eliminó la vinculación de esta cuenta.")

            thing_name = self.links.resolve(user_id)
            if not thing_name:
                return alexa_response("Aún no tienes una puerta vinculada. "
                                      "Di vincula con el código y sus seis dígitos.")
            if name in ("BloquearIntent", "DesbloquearIntent"):
                target = "bloqueada" if name == "BloquearIntent" else "desbloqueada"
                self.cloud.set_lock(thing_name, target)
                # No afirmar que el servo se movió: solo cambia desired.
                return alexa_response("He enviado la orden de " +
                                      ("bloquear" if target == "bloqueada" else "desbloquear") +
                                      " la cerradura. Puedes consultar su estado para verificarla.")
            if name == "EstadoPuertaIntent":
                state = self.cloud.get_reported(thing_name).get("puerta")
                return alexa_response("La puerta está " + str(state) + "." if state in
                                      ("abierta", "cerrada") else "Aún no tengo una lectura de la puerta.")
            if name == "EstadoCerrojoIntent":
                state = self.cloud.get_reported(thing_name).get("cerrojo")
                return alexa_response("La cerradura está " + str(state) + "." if state in
                                      ("bloqueada", "desbloqueada") else "Aún no tengo el estado de la cerradura.")
            if name == "LecturasRfidIntent":
                total = self.cloud.get_reported(thing_name).get("tarjetasLeidas")
                return alexa_response("El lector ha registrado " + str(total) +
                                      " lecturas desde el último reinicio." if
                                      isinstance(total, int) else "Aún no tengo lecturas RFID.")
            return alexa_response("No entendí ese comando. Di ayuda para conocer las opciones.")
        except ValueError:
            return alexa_response("Los datos introducidos no son válidos.")
        except Exception:
            logger.exception("Fallo atendiendo solicitud de Alexa")
            return alexa_response("No pude comunicarme con el sistema. Revisa AWS y la conexión.")


def create_controller() -> SkillController:
    import boto3
    table_name = os.environ["TABLE_NAME"]
    endpoint = os.environ["IOT_DATA_ENDPOINT"].strip()
    if not endpoint.startswith("https://"):
        endpoint = "https://" + endpoint
    table = boto3.resource("dynamodb").Table(table_name)
    registry = boto3.client("iot")
    data = boto3.client("iot-data", endpoint_url=endpoint)
    return SkillController(LinkRepository(table), ShadowGateway(registry, data))


_controller = None


def lambda_handler(event, context):
    global _controller
    if _controller is None:
        _controller = create_controller()
    return _controller.handle(event)
