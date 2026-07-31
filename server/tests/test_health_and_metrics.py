"""Health semantics, metrics exposition and the version cache."""

from __future__ import annotations

import itertools
import time
from battery_monitor.config import load_config
from battery_monitor.sender import MessageSender
from tests.conftest import API_TOKEN, FakeSignalCli, make_env


_EVENT_IDS = itertools.count(1)


def send(client, auth):
    return client.post(
        "/send",
        json={
            "event_id": f"health-event-{next(_EVENT_IDS)}",
            "device_id": "B",
            "msg_type": "INFO",
            "voltage": 12.5,
        },
        headers=auth,
    )


def wait_for_failures(sender, count, timeout=2.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if sender.stats().failed_total >= count:
            return True
        time.sleep(0.005)
    return False


def wait_for_sent(sender, count, timeout=2.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if sender.stats().sent_total >= count:
            return True
        time.sleep(0.005)
    return False


# -- health ------------------------------------------------------------------


def test_healthy_when_nothing_has_failed(client):
    response = client.get("/healthcheck")
    assert response.status_code == 200
    assert response.get_json()["status"] == "healthy"


def test_single_failure_degrades_but_does_not_page(client, auth, fake_cli, sender):
    fake_cli.mode = "fail"
    send(client, auth)
    assert wait_for_failures(sender, 1)

    response = client.get("/healthcheck")
    assert response.status_code == 200
    assert response.get_json()["status"] == "degraded"


def test_repeated_failures_report_unhealthy(client, auth, fake_cli, sender):
    fake_cli.mode = "fail"
    for _ in range(3):
        send(client, auth)
    assert wait_for_failures(sender, 3)

    response = client.get("/healthcheck")
    # 503, not 500: the service is up and answering, it just cannot deliver.
    assert response.status_code == 503
    assert response.get_json()["status"] == "unhealthy"


def test_health_recovers_after_a_success(client, auth, fake_cli, sender):
    fake_cli.mode = "fail"
    for _ in range(3):
        send(client, auth)
    assert wait_for_failures(sender, 3)
    assert client.get("/healthcheck").status_code == 503

    # The original implementation used a counter that never reset, so one
    # transient failure pinned this endpoint at an error status forever.
    fake_cli.mode = "ok"
    send(client, auth)
    assert wait_for_sent(sender, 1)

    response = client.get("/healthcheck")
    assert response.status_code == 200
    assert response.get_json()["status"] == "healthy"


def test_worker_survives_an_unexpected_exception(client, auth, fake_cli, sender):
    # A TypeError from an unset SIGNAL_USER used to kill the worker thread.
    # Every later message was then silently dropped while the health endpoint
    # still reported healthy.
    fake_cli.mode = "crash"
    send(client, auth)
    assert wait_for_failures(sender, 1)
    assert sender.stats().worker_alive

    fake_cli.mode = "ok"
    send(client, auth)
    assert wait_for_sent(sender, 1)
    assert fake_cli.sent == ["B: Voltage 12.5 V."]


def test_healthcheck_hides_error_detail_from_anonymous_callers(
    client, auth, fake_cli, sender
):
    fake_cli.mode = "fail"
    send(client, auth)
    assert wait_for_failures(sender, 1)

    # signal-cli stderr can name accounts and paths.
    assert "last_error" not in client.get("/healthcheck").get_json()
    assert client.get("/healthcheck", headers=auth).get_json()["last_error"]


# -- metrics -----------------------------------------------------------------


def test_metrics_requires_auth_by_default(client):
    assert client.get("/metrics").status_code == 401


def test_metrics_exposition(client, auth):
    response = client.get("/metrics", headers=auth)
    assert response.status_code == 200
    assert response.mimetype == "text/plain"
    body = response.get_data(as_text=True)
    for metric in [
        "battery_monitor_messages_sent_total",
        "battery_monitor_messages_failed_total",
        "battery_monitor_consecutive_failures",
        "battery_monitor_queue_depth",
        "battery_monitor_dead_letters",
        "battery_monitor_worker_alive",
        "battery_monitor_healthy",
        "signal_cli_up",
    ]:
        assert f"# TYPE {metric} " in body
        assert f"\n{metric} " in body


def test_metrics_counts_delivered_messages(client, auth, sender):
    send(client, auth)
    assert sender.wait_idle()
    body = client.get("/metrics", headers=auth).get_data(as_text=True)
    assert "battery_monitor_messages_sent_total 1" in body


def test_version_label_is_escaped(client, auth, fake_cli):
    # The version comes from a subprocess and from GitHub. An unescaped quote
    # would corrupt the whole exposition format for the scraper.
    fake_cli.local = ' 0.13.5"evil\\x'
    body = client.get("/metrics", headers=auth).get_data(as_text=True)
    line = next(l for l in body.splitlines() if l.startswith("signal_cli_local_version{"))
    assert line.count('"') == 2 + line.count('\\"')


def test_metrics_can_be_opened_up(fake_cli):
    config = load_config(environ=make_env(METRICS_REQUIRE_AUTH="false"))
    from battery_monitor.app import create_app

    app = create_app(
        config,
        signal_cli=fake_cli,
        sender=MessageSender(fake_cli, send_interval_s=0),
    )
    try:
        assert app.test_client().get("/metrics").status_code == 200
    finally:
        app.extensions["battery_monitor"]["sender"].stop()


# -- version endpoint and caching -------------------------------------------


def test_signal_version_endpoint(client, auth):
    response = client.get("/signal-cli/version", headers=auth)
    assert response.status_code == 200
    body = response.get_json()
    assert body["local_version"] == "0.13.5"
    assert body["latest_version"] == "0.13.9"
    assert body["update_available"] is True


def test_version_endpoint_is_gated_like_metrics(client):
    assert client.get("/signal-cli/version").status_code == 401


def test_version_lookups_are_cached(client, auth, fake_cli):
    # Without a cache, every scrape shelled out to signal-cli and called the
    # GitHub API, exhausting GitHub's 60-per-hour unauthenticated limit.
    for _ in range(10):
        client.get("/metrics", headers=auth)
    assert fake_cli.local_calls == 1
    assert fake_cli.latest_calls == 1


def test_update_available_is_false_when_github_is_unreachable(client, auth, fake_cli):
    fake_cli.latest = None
    body = client.get("/signal-cli/version", headers=auth).get_json()
    assert body["latest_version"] is None
    assert body["update_available"] is False
