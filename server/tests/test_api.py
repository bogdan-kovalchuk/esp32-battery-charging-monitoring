"""HTTP contract with the ESP32 firmware, plus auth and validation."""

from __future__ import annotations

import itertools
import pytest

from tests.conftest import API_TOKEN


_EVENT_IDS = itertools.count(1)


def post_send(client, auth, **payload):
    payload.setdefault("event_id", f"test-event-{next(_EVENT_IDS)}")
    return client.post("/send", json=payload, headers=auth)


# -- the firmware's two real payloads ---------------------------------------


def test_info_payload_is_accepted_and_delivered(client, auth, fake_cli, sender):
    response = post_send(client, auth, device_id="BATT#1", msg_type="INFO", voltage=12.4)
    assert response.status_code == 202
    assert response.get_json()["status"] == "queued"

    assert sender.wait_idle()
    assert fake_cli.sent == ["BATT#1: Voltage 12.4 V."]


def test_alert_below_critical_names_the_threshold(client, auth, fake_cli, sender):
    response = post_send(
        client,
        auth,
        device_id="BATT#1",
        msg_type="ALERT",
        voltage=10.8,
        critical_voltage=11.0,
    )
    assert response.status_code == 202

    assert sender.wait_idle()
    assert fake_cli.sent == [
        "ALERT BATT#1: Voltage 10.8 V is at or below critical 11.0 V. "
        "Charge the battery."
    ]


def test_alert_without_critical_voltage_is_plain(client, auth, fake_cli, sender):
    post_send(client, auth, device_id="BATT#1", msg_type="ALERT", voltage=12.0)
    assert sender.wait_idle()
    assert fake_cli.sent == ["ALERT BATT#1: Voltage 12.0 V."]


def test_msg_type_is_case_insensitive(client, auth, fake_cli, sender):
    assert post_send(
        client, auth, device_id="B", msg_type="info", voltage=12.0
    ).status_code == 202
    assert sender.wait_idle()
    assert fake_cli.sent == ["B: Voltage 12.0 V."]


# -- authentication ----------------------------------------------------------


def test_send_without_token_is_rejected(client):
    response = client.post(
        "/send", json={"device_id": "B", "msg_type": "INFO", "voltage": 12.5}
    )
    assert response.status_code == 401
    assert response.get_json()["error"] == "Unauthorized"


@pytest.mark.parametrize(
    "header",
    [
        "Bearer wrong-token",
        f"Bearer {API_TOKEN} ",
        f"bearer {API_TOKEN}",
        API_TOKEN,
        "",
    ],
)
def test_malformed_or_wrong_token_is_rejected(client, header):
    response = client.post(
        "/send",
        json={"device_id": "B", "msg_type": "INFO", "voltage": 12.5},
        headers={"Authorization": header},
    )
    assert response.status_code == 401


def test_unauthenticated_request_cannot_affect_health(client, sender):
    before = sender.stats().failed_total
    client.post("/send", json={"device_id": "B", "msg_type": "INFO", "voltage": 12.5})
    # Otherwise anyone who can reach the port could drive the service to
    # "unhealthy" just by sending garbage without a token.
    assert sender.stats().failed_total == before


# -- payload validation ------------------------------------------------------


@pytest.mark.parametrize("field", ["event_id", "device_id", "msg_type", "voltage"])
def test_missing_required_field(client, auth, field):
    payload = {
        "event_id": "evt-required",
        "device_id": "B",
        "msg_type": "INFO",
        "voltage": 12.5,
    }
    del payload[field]
    response = client.post("/send", json=payload, headers=auth)
    assert response.status_code == 400
    assert field in response.get_json()["error"]


@pytest.mark.parametrize("voltage", ["12.5", None, True, False, [], {}])
def test_non_numeric_voltage_is_a_400_not_a_500(client, auth, voltage):
    # The original code compared a string to a float and raised TypeError,
    # turning a bad request into an unhandled 500.
    response = client.post(
        "/send",
        json={"device_id": "B", "msg_type": "ALERT", "voltage": voltage,
              "critical_voltage": 11.0},
        headers=auth,
    )
    assert response.status_code == 400


@pytest.mark.parametrize("voltage", [1e9, -1e9])
def test_out_of_range_voltage_is_rejected(client, auth, voltage):
    assert post_send(
        client, auth, device_id="B", msg_type="INFO", voltage=voltage
    ).status_code == 400


def test_non_finite_voltage_is_rejected(client, auth):
    # Flask's JSON parser accepts the bare NaN / Infinity literals, so they
    # reach the handler and must be rejected explicitly.
    response = client.post(
        "/send",
        data='{"device_id":"B","msg_type":"INFO","voltage":NaN}',
        content_type="application/json",
        headers=auth,
    )
    assert response.status_code == 400


@pytest.mark.parametrize("msg_type", ["UNKNOWN", "", 5, None, ["INFO"]])
def test_invalid_msg_type(client, auth, msg_type):
    assert post_send(
        client, auth, device_id="B", msg_type=msg_type, voltage=12.5
    ).status_code == 400


@pytest.mark.parametrize("device_id", ["", "   ", 42, None, "x" * 65])
def test_invalid_device_id(client, auth, device_id):
    assert post_send(
        client, auth, device_id=device_id, msg_type="INFO", voltage=12.5
    ).status_code == 400


def test_non_json_body(client, auth):
    assert client.post("/send", data="not json", headers=auth).status_code == 400


def test_json_array_body_is_rejected(client, auth):
    assert client.post("/send", json=[1, 2, 3], headers=auth).status_code == 400


def test_oversized_body_is_rejected(client, auth):
    response = client.post(
        "/send",
        data="x" * 9000,
        content_type="application/json",
        headers=auth,
    )
    assert response.status_code == 413


@pytest.mark.parametrize("event_id", ["", "space id", "x" * 129, 42, None])
def test_invalid_event_id_is_rejected(client, auth, event_id):
    response = post_send(
        client,
        auth,
        event_id=event_id,
        device_id="B",
        msg_type="INFO",
        voltage=12.5,
    )
    assert response.status_code == 400


def test_duplicate_event_is_accepted_once(client, auth, fake_cli, sender):
    payload = {
        "event_id": "same-event",
        "device_id": "B",
        "msg_type": "INFO",
        "voltage": 12.5,
    }
    assert client.post("/send", json=payload, headers=auth).status_code == 202
    assert sender.wait_idle()
    duplicate = client.post("/send", json=payload, headers=auth)
    assert duplicate.status_code == 202
    assert duplicate.get_json()["status"] == "duplicate"
    assert fake_cli.sent == ["B: Voltage 12.5 V."]


def test_unicode_device_id_round_trips(client, auth, fake_cli, sender):
    post_send(client, auth, device_id="Батарея №1", msg_type="INFO", voltage=12.5)
    assert sender.wait_idle()
    assert fake_cli.sent == ["Батарея №1: Voltage 12.5 V."]


# -- error handlers ----------------------------------------------------------


def test_unknown_route_returns_json(client):
    response = client.get("/nope")
    assert response.status_code == 404
    assert response.get_json()["error"] == "Not found"


def test_wrong_method_returns_json(client, auth):
    response = client.get("/send", headers=auth)
    assert response.status_code == 405
    assert response.get_json()["error"] == "Method not allowed"
