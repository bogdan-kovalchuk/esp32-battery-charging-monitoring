import pytest
import json
from src.app import app, API_TOKEN


@pytest.fixture
def client():
    app.config["TESTING"] = True
    with app.test_client() as client:
        yield client


@pytest.fixture
def auth_header():
    return {"Authorization": f"Bearer {API_TOKEN}"}


def test_send_info_message(client, auth_header):
    data = {"device_id": "BATT#1", "msg_type": "INFO", "voltage": 12.5}
    response = client.post("/send", json=data, headers=auth_header)
    assert response.status_code == 200
    assert response.get_json()["status"] == "Message added to queue"


def test_send_alert_message(client, auth_header):
    data = {
        "device_id": "BATT#1",
        "msg_type": "ALERT",
        "voltage": 10.5,
        "critical_voltage": 11.0,
    }
    response = client.post("/send", json=data, headers=auth_header)
    assert response.status_code == 200


def test_send_unauthorized(client):
    data = {"device_id": "BATT#1", "msg_type": "INFO", "voltage": 12.5}
    response = client.post("/send", json=data)
    assert response.status_code == 401
    assert response.get_json()["error"] == "Unauthorized"


def test_send_wrong_token(client):
    data = {"device_id": "BATT#1", "msg_type": "INFO", "voltage": 12.5}
    headers = {"Authorization": "Bearer wrong-token"}
    response = client.post("/send", json=data, headers=headers)
    assert response.status_code == 401


def test_send_missing_fields(client, auth_header):
    data = {"device_id": "BATT#1"}
    response = client.post("/send", json=data, headers=auth_header)
    assert response.status_code == 400


def test_send_invalid_msg_type(client, auth_header):
    data = {"device_id": "BATT#1", "msg_type": "UNKNOWN", "voltage": 12.5}
    response = client.post("/send", json=data, headers=auth_header)
    assert response.status_code == 400


def test_send_non_json(client, auth_header):
    response = client.post("/send", data="not json", headers=auth_header)
    assert response.status_code == 400


def test_healthcheck(client):
    response = client.get("/healthcheck")
    assert response.status_code in [200, 500]
    data = response.get_json()
    assert "status" in data
    assert "failed_send_messages" in data
    assert "failed_receive_messages" in data


def test_metrics(client):
    response = client.get("/metrics")
    assert response.status_code == 200
    assert response.content_type == "text/plain; charset=utf-8"
    assert "signal_cli_failed_send_messages" in response.get_data(as_text=True)


def test_signal_version(client):
    response = client.get("/signal-cli/version")
    assert response.status_code == 200
    data = response.get_json()
    assert "local_version" in data
    assert "latest_version" in data
    assert "update_available" in data
