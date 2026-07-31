"""Regression tests for defects found in review.

Each of these produced an unhandled 500 or a broken invariant before the fix.
"""

from __future__ import annotations

import re
import threading
import time
from pathlib import Path

import pytest

from battery_monitor.app import create_app
from battery_monitor.config import NO_ENV_FILE, ConfigError, load_config
from battery_monitor.sender import MessageSender
from battery_monitor.signal_cli import SignalCli, TtlCache
from tests.conftest import API_TOKEN, FakeSignalCli, make_env

VALID_PAYLOAD = {
    "event_id": "regression-event",
    "device_id": "B",
    "msg_type": "INFO",
    "voltage": 12.5,
}


# -- non-ASCII Authorization header ------------------------------------------
#
# Werkzeug decodes header bytes as latin-1, so any byte >= 0x80 yields a
# non-ASCII str. hmac.compare_digest on two such str raises TypeError.
# /healthcheck was the worst case: it calls _authorized() unconditionally to
# decide whether to attach last_error, so an anonymous caller could force a
# 500 and make a load balancer pull the service out of rotation.

NON_ASCII_HEADERS = ["Bearer ünicode", "Bearer ü", "ÿ" * 8]


@pytest.mark.parametrize("header", NON_ASCII_HEADERS)
def test_non_ascii_auth_header_on_send(client, header):
    response = client.post(
        "/send", json=VALID_PAYLOAD, headers={"Authorization": header}
    )
    assert response.status_code == 401


@pytest.mark.parametrize("header", NON_ASCII_HEADERS)
def test_non_ascii_auth_header_on_metrics(client, header):
    assert client.get("/metrics", headers={"Authorization": header}).status_code == 401


@pytest.mark.parametrize("header", NON_ASCII_HEADERS)
def test_non_ascii_auth_header_on_healthcheck(client, header):
    # healthcheck_require_auth is false by default, so this must succeed as an
    # anonymous request rather than blow up while probing the header.
    response = client.get("/healthcheck", headers={"Authorization": header})
    assert response.status_code == 200
    assert "last_error" not in response.get_json()


# -- arbitrary-precision integers --------------------------------------------
#
# math.isfinite raises OverflowError on an int beyond float range, and
# MAX_CONTENT_LENGTH leaves room for a several-thousand-digit literal.


@pytest.mark.parametrize("field", ["voltage", "critical_voltage"])
def test_huge_integer_is_a_400_not_a_500(client, auth, field):
    huge = "1" + "0" * 400
    body = (
        '{"device_id":"B","msg_type":"ALERT","voltage":%s,"critical_voltage":%s}'
        % (huge if field == "voltage" else "12.5",
           huge if field == "critical_voltage" else "11.0")
    )
    response = client.post(
        "/send", data=body, content_type="application/json", headers=auth
    )
    assert response.status_code == 400


def test_huge_negative_integer_is_rejected(client, auth):
    body = '{"device_id":"B","msg_type":"INFO","voltage":-1%s}' % ("0" * 400)
    response = client.post(
        "/send", data=body, content_type="application/json", headers=auth
    )
    assert response.status_code == 400


# -- message forgery ---------------------------------------------------------


@pytest.mark.parametrize(
    "device_id", ["a\nb: Voltage 99 V.", "a\rb", "a\tb", "a\x00b"]
)
def test_control_characters_in_device_id_are_rejected(client, auth, device_id):
    # Otherwise an authenticated client can forge extra readings in the group.
    response = client.post(
        "/send", json={**VALID_PAYLOAD, "device_id": device_id}, headers=auth
    )
    assert response.status_code == 400


# -- worker lifecycle --------------------------------------------------------


def test_failed_stop_does_not_allow_a_second_worker(fake_cli):
    """stop() used to clear _thread even when the join timed out.

    A later start() then ran a second worker on the same queue, breaking the
    guarantee that signal-cli is never invoked concurrently for one account.
    """
    blocked = threading.Event()

    class BlockingCli(FakeSignalCli):
        def send(self, message: str) -> None:
            blocked.wait(2.0)

    sender = MessageSender(BlockingCli(), send_interval_s=0)
    sender.start()
    sender.enqueue("blocking-event", "first")
    time.sleep(0.2)

    assert sender.stop(timeout=0.2) is False

    sender.start()
    live = [t for t in threading.enumerate() if t.name == "signal-sender"]
    assert len(live) == 1

    blocked.set()
    sender.stop(timeout=2.0)


def test_concurrent_start_creates_one_worker(fake_cli):
    sender = MessageSender(fake_cli, send_interval_s=0)
    barrier = threading.Barrier(8)

    def racer():
        barrier.wait()
        sender.start()

    threads = [threading.Thread(target=racer) for _ in range(8)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    try:
        live = [t for t in threading.enumerate() if t.name == "signal-sender"]
        assert len(live) == 1
    finally:
        sender.stop()


def test_full_queue_is_a_503_not_a_silent_drop(client, auth, fake_cli):
    blocked = threading.Event()

    class BlockingCli(FakeSignalCli):
        def send(self, message: str) -> None:
            blocked.wait(3.0)

    cli = BlockingCli()
    sender = MessageSender(cli, send_interval_s=0, queue_maxsize=2)
    app = create_app(
        load_config(env_file=NO_ENV_FILE, environ=make_env()),
        signal_cli=cli,
        sender=sender,
    )
    test_client = app.test_client()
    try:
        codes = [
            test_client.post(
                "/send",
                json={**VALID_PAYLOAD, "event_id": f"full-{i}"},
                headers=auth,
            ).status_code
            for i in range(6)
        ]
        # The firmware retries any non-2xx, which is the right response to a
        # temporarily full backlog.
        assert 503 in codes
    finally:
        blocked.set()
        sender.stop(timeout=3.0)


def test_client_errors_do_not_look_like_delivery_failures(client, auth, sender):
    client.post("/send", json={"device_id": "B"}, headers=auth)
    stats = sender.stats()
    assert stats.client_errors_total == 1
    # A buggy but authenticated client must not make the service look unable
    # to reach Signal.
    assert stats.failed_total == 0
    assert client.get("/healthcheck").get_json()["status"] == "healthy"


def test_durable_outbox_survives_sender_restart(fake_cli, tmp_path):
    db_path = tmp_path / "durable.sqlite3"
    first = MessageSender(fake_cli, db_path=db_path, send_interval_s=0)
    assert first.enqueue("persisted-event", "survives restart").value == "queued"

    second = MessageSender(fake_cli, db_path=db_path, send_interval_s=0)
    second.start()
    try:
        assert second.wait_idle()
        assert fake_cli.sent == ["survives restart"]
    finally:
        second.stop()


def test_exhausted_retry_moves_event_to_dead_letter(fake_cli):
    fake_cli.mode = "fail"
    sender = MessageSender(
        fake_cli,
        send_interval_s=0,
        retry_base_s=0,
        retry_max_s=0,
        retry_jitter=0,
        max_attempts=2,
    )
    sender.start()
    sender.enqueue("dead-event", "cannot deliver")
    try:
        assert sender.wait_idle()
        status, stats = sender.health()
        assert status == "unhealthy"
        assert stats.dead_lettered == 1
        assert stats.failed_total == 2
    finally:
        sender.stop()


# -- TtlCache ----------------------------------------------------------------


def test_ttl_cache_does_not_serialise_callers():
    """The producer used to run while the lock was held.

    Four concurrent /metrics scrapes each waited the full producer duration,
    which with a hung signal-cli parks every waitress worker thread.
    """
    cache = TtlCache(60)
    started = threading.Event()

    def slow_producer():
        started.set()
        time.sleep(0.5)
        return "value"

    waiter = threading.Thread(target=lambda: cache.get(slow_producer))
    waiter.start()
    started.wait(1.0)

    began = time.monotonic()
    cache.get(lambda: pytest.fail("second caller must not run the producer"))
    assert time.monotonic() - began < 0.2

    waiter.join()
    assert cache.get(lambda: pytest.fail("should be cached")) == "value"


def test_ttl_cache_retries_a_failure_sooner_than_a_success():
    cache = TtlCache(3600, failure_ttl_seconds=0.05)
    assert cache.get(lambda: None) is None
    time.sleep(0.1)
    # A signal-cli hiccup must not pin the metric at 0 for the whole hour.
    assert cache.get(lambda: "recovered") == "recovered"


def test_ttl_cache_invalidate_forces_a_refresh():
    cache = TtlCache(3600)
    assert cache.get(lambda: "first") == "first"
    assert cache.get(lambda: "second") == "first"
    cache.invalidate()
    assert cache.get(lambda: "second") == "second"


# -- configuration validation ------------------------------------------------


@pytest.mark.parametrize("name", ["LOCAL_VERSION_TTL", "LATEST_VERSION_TTL"])
@pytest.mark.parametrize("value", ["0", "-1"])
def test_non_positive_cache_ttl_is_fatal(name, value):
    # A disabled cache re-opens the GitHub rate-limit problem.
    with pytest.raises(ConfigError):
        load_config(env_file=NO_ENV_FILE, environ=make_env(**{name: value}))


def test_tiny_max_content_length_is_fatal():
    with pytest.raises(ConfigError):
        load_config(env_file=NO_ENV_FILE, environ=make_env(MAX_CONTENT_LENGTH="1"))


def test_relative_battery_monitor_env_is_fatal():
    with pytest.raises(ConfigError) as exc:
        load_config(environ=make_env(BATTERY_MONITOR_ENV="relative/.env"))
    assert "absolute" in str(exc.value)


def test_default_env_file_is_not_read_when_opted_out(tmp_path, monkeypatch):
    """The suite must be immune to a real server/.env on the machine."""
    import battery_monitor.config as config_module

    rogue = tmp_path / ".env"
    rogue.write_text("METRICS_REQUIRE_AUTH=false\nMAX_CONTENT_LENGTH=8192\n", "utf-8")
    monkeypatch.setattr(config_module, "DEFAULT_ENV_FILE", rogue)

    opted_out = load_config(env_file=NO_ENV_FILE, environ=make_env())
    assert opted_out.metrics_require_auth is True
    assert opted_out.max_content_length == 4096

    # Sanity: without the opt-out the rogue file really would be picked up.
    picked_up = load_config(environ=make_env())
    assert picked_up.metrics_require_auth is False


def test_e164_regex_does_not_accept_a_trailing_newline():
    """`$` also matches before a trailing newline; the pattern must use `\\Z`.

    load_config strips its inputs, so this is not reachable through it today —
    the test guards the pattern itself against a future caller that does not.
    """
    from battery_monitor.config import E164_RE

    assert E164_RE.match("+380501234567")
    assert not E164_RE.match("+380501234567\n")


def test_short_secret_is_not_partially_revealed():
    config = load_config(env_file=NO_ENV_FILE, environ=make_env(SIGNAL_GROUP_ID="abcde"))
    assert "abc" not in config.describe_sources()


# -- documentation drift -----------------------------------------------------


def test_env_example_documents_every_variable_that_is_read():
    """`.env.example` is the only place these names are written down."""
    source = Path(__file__).resolve().parents[1] / "battery_monitor" / "config.py"
    example = Path(__file__).resolve().parents[1] / ".env.example"

    read_names = set(re.findall(r'values(?:\.get)?[.(]?\s*\(?\s*"([A-Z][A-Z0-9_]+)"',
                                source.read_text(encoding="utf-8")))
    read_names |= set(re.findall(r'values\.get\("([A-Z][A-Z0-9_]+)"',
                                 source.read_text(encoding="utf-8")))

    example_text = example.read_text(encoding="utf-8")
    documented = set(re.findall(r"^([A-Z][A-Z0-9_]+)=", example_text, re.MULTILINE))
    documented |= set(re.findall(r"#\s*([A-Z][A-Z0-9_]+)\b", example_text))

    missing = read_names - documented
    assert not missing, f"read by config.py but absent from .env.example: {missing}"


def test_env_example_itself_is_not_a_valid_config():
    # Copying the example without editing it must not produce a running server.
    example = Path(__file__).resolve().parents[1] / ".env.example"
    with pytest.raises(ConfigError):
        load_config(env_file=example, environ={})


# -- signal-cli --------------------------------------------------------------


@pytest.mark.parametrize(
    ("stdout", "expected"),
    [("signal-cli 0.13.4\n", "0.13.4"), ("0.13.4", "0.13.4"), ("", None), ("   \n", None)],
)
def test_local_version_parsing(monkeypatch, stdout, expected):
    import subprocess

    cli = SignalCli("signal-cli", "+380501234567", "g")

    class Result:
        pass

    result = Result()
    result.stdout = stdout
    monkeypatch.setattr(subprocess, "run", lambda *a, **k: result)
    assert cli._read_local_version() == expected


def test_send_argv_never_uses_a_shell():
    cli = SignalCli("signal-cli", "+380501234567", "group=")
    argv = cli.send_argv("-rf --delete; rm -rf /")
    # Passed as a list, so the message can never be parsed as arguments.
    assert argv[-1] == "-rf --delete; rm -rf /"
    assert argv[0] == "signal-cli"
    assert argv[1:3] == ["-a", "+380501234567"]


def test_send_receives_account_updates_before_first_delivery(monkeypatch):
    import subprocess

    calls = []

    class Result:
        stdout = ""

    def run(argv, **kwargs):
        calls.append((argv, kwargs))
        return Result()

    monkeypatch.setattr(subprocess, "run", run)
    cli = SignalCli("signal-cli", "+380501234567", "group=")

    cli.send("first")
    cli.send("second")

    assert calls[0][0] == [
        "signal-cli", "-a", "+380501234567", "receive", "--timeout", "1",
        "--ignore-attachments", "--ignore-stories", "--ignore-avatars",
        "--ignore-stickers",
    ]
    assert calls[1][0] == cli.send_argv("first")
    assert calls[2][0] == cli.send_argv("second")


def test_readiness_checks_account_and_exact_group(monkeypatch):
    import subprocess

    calls = []

    class Result:
        def __init__(self, stdout):
            self.stdout = stdout
            self.stderr = ""

    results = iter(
        [Result("signal-cli 0.13.20\n"), Result('[{"id":"expected-group"}]')]
    )

    def run(argv, **kwargs):
        calls.append(argv)
        return next(results)

    monkeypatch.setattr(subprocess, "run", run)
    cli = SignalCli("signal-cli", "+380501234567", "expected-group")

    assert cli.check_ready() == (True, None)
    assert calls == [
        ["signal-cli", "--version"],
        [
            "signal-cli", "-a", "+380501234567", "--output=json",
            "listGroups", "-g", "expected-group",
        ],
    ]


def test_readiness_rejects_group_not_visible_to_account(monkeypatch):
    import subprocess

    class Result:
        stderr = ""

        def __init__(self, stdout):
            self.stdout = stdout

    results = iter([Result("signal-cli 0.13.20\n"), Result("[]")])
    monkeypatch.setattr(subprocess, "run", lambda *a, **k: next(results))

    ready, error = SignalCli(
        "signal-cli", "+380501234567", "missing-group"
    ).check_ready()
    assert ready is False
    assert "not visible" in error
