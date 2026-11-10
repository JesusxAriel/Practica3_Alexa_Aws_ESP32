"""Pruebas locales: python -m unittest discover -s tests -v (sin AWS real)."""
from io import BytesIO
import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "lambda"))
from lambda_function import LinkRepository, ShadowGateway, SkillController


class FakeTable:
    def __init__(self):
        self.items = {"CODE#482731": {"pk": "CODE#482731", "thingName": "puerta-sis234-g1-01"}}

    def get_item(self, Key):
        return {"Item": self.items[Key["pk"]]} if Key["pk"] in self.items else {}

    def put_item(self, Item):
        self.items[Item["pk"]] = Item

    def delete_item(self, Key):
        self.items.pop(Key["pk"], None)


class FakeRegistry:
    def describe_thing(self, thingName):
        if thingName != "puerta-sis234-g1-01":
            raise ValueError("No existe")


class FakeData:
    def __init__(self):
        self.desired = None
        self.reported = {"puerta": "cerrada", "cerrojo": "bloqueada", "tarjetasLeidas": 2}

    def get_thing_shadow(self, thingName):
        return {"payload": BytesIO(json.dumps({"state": {"reported": self.reported}}).encode())}

    def update_thing_shadow(self, thingName, payload):
        self.desired = json.loads(payload)["state"]["desired"]
        return {"payload": BytesIO(payload)}


def request(intent_name, code=None, user="amzn1.ask.account.TEST-A"):
    intent = {"name": intent_name}
    if code is not None:
        intent["slots"] = {"codigo": {"name": "codigo", "value": code}}
    return {"version": "1.0", "context": {"System": {"user": {"userId": user}}},
            "request": {"type": "IntentRequest", "intent": intent}}


def message(response):
    return response["response"]["outputSpeech"]["text"]


class AlexaTests(unittest.TestCase):
    def setUp(self):
        self.table = FakeTable()
        self.data = FakeData()
        self.skill = SkillController(LinkRepository(self.table),
                                     ShadowGateway(FakeRegistry(), self.data))

    def test_unlinked_user_is_guided(self):
        self.assertIn("vinculada", message(self.skill.handle(request("EstadoPuertaIntent"))))

    def test_link_then_check_door(self):
        result = self.skill.handle(request("VincularIntent", "482731"))
        self.assertIn("vinculada correctamente", message(result))
        self.assertIn("cerrada", message(self.skill.handle(request("EstadoPuertaIntent"))))

    def test_two_users_do_not_share_binding(self):
        self.skill.handle(request("VincularIntent", "482731", "USER1"))
        self.assertIn("vinculada", message(self.skill.handle(request("EstadoPuertaIntent", user="USER2"))))

    def test_open_command_only_updates_desired(self):
        self.skill.handle(request("VincularIntent", "482731"))
        result = self.skill.handle(request("DesbloquearIntent"))
        self.assertEqual(self.data.desired, {"cerrojo": "desbloqueada"})
        self.assertIn("enviado", message(result))
        self.assertEqual(self.data.reported["cerrojo"], "bloqueada")

    def test_unlink(self):
        self.skill.handle(request("VincularIntent", "482731"))
        self.skill.handle(request("DesvincularIntent"))
        self.assertNotIn("USER#amzn1.ask.account.TEST-A", self.table.items)

    def test_invalid_code(self):
        self.assertIn("seis dígitos", message(self.skill.handle(request("VincularIntent", "12"))))

    def test_missing_code(self):
        self.assertIn("seis dígitos", message(self.skill.handle(request("VincularIntent"))))

    def test_rfid_count(self):
        self.skill.handle(request("VincularIntent", "482731"))
        self.assertIn("2 lecturas", message(self.skill.handle(request("LecturasRfidIntent"))))


if __name__ == "__main__":
    unittest.main()
