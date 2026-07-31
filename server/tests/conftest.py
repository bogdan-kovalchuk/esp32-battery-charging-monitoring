from __future__ import annotations

import pytest

from battery_monitor.app import create_app
from battery_monitor.config import NO_ENV_FILE, load_config
from battery_monitor.sender import MessageSender
from battery_monitor.signal_cli import SignalCli, SignalCliError

API_TOKEN = "test-token-that-is-long-enough"
SIGNAL_USER = "+380501234567"
GROUP_ID = "group-id-base64=="


class FakeSignalCli(SignalCli):
    """Records messages instead of shelling out. Failure modes are switchable.

    Subclassing the real class keeps the interface honest: if a method is
    added to SignalCli and not overridden here, tests exercise the real one
    and fail loudly rather than silently passing against a stale stub.
    """

    def __init__(self) -> None:
        super().__init__("fake-signal-cli", SIGNAL_USER, GROUP_ID)
        self.sent: list[str] = []
        self.mode = "ok"  # "ok" | "fail" | "crash"
        self.local = "0.13.5"
        self.latest = "0.13.9"
        self.local_calls = 0
        self.latest_calls = 0

    def send(self, message: str) -> None:
        if self.mode == "fail":
            raise SignalCliError("signal-cli exited 1")
        if self.mode == "crash":
            # The exact class of bug that used to kill the worker thread.
            raise TypeError("expected str, bytes or os.PathLike, not NoneType")
        self.sent.append(message)

    def maintenance(self) -> None:
        # Production periodically invokes the real `receive` command. Unit
        # tests must not search PATH for a binary on every app fixture.
        return None

    def _read_local_version(self):
        self.local_calls += 1
        return self.local

    def _read_latest_version(self):
        self.latest_calls += 1
        return self.latest


def make_env(**overrides) -> dict[str, str]:
    env = {
        "SIGNAL_USER": SIGNAL_USER,
        "SIGNAL_GROUP_ID": GROUP_ID,
        "API_TOKEN": API_TOKEN,
    }
    env.update({k: str(v) for k, v in overrides.items()})
    return env


@pytest.fixture
def config():
    # NO_ENV_FILE, not None: None means "use the normal resolution order",
    # which reads server/.env if one exists. Every deployment box has one, so
    # without this the suite would pick up real settings and fail there while
    # passing on a clean checkout.
    return load_config(env_file=NO_ENV_FILE, environ=make_env())


@pytest.fixture
def fake_cli():
    return FakeSignalCli()


@pytest.fixture
def app(config, fake_cli, tmp_path):
    # send_interval_s=0: the production 2 s pause between sends exists to keep
    # signal-cli from rate-limiting, and would add seconds to every test.
    application = create_app(
        config,
        signal_cli=fake_cli,
        sender=MessageSender(
            fake_cli,
            unhealthy_after_failures=config.unhealthy_after_failures,
            send_interval_s=0,
            db_path=tmp_path / "outbox.sqlite3",
            retry_base_s=60,
            retry_max_s=60,
            max_attempts=3,
            retry_jitter=0,
        ),
    )
    yield application
    application.extensions["battery_monitor"]["sender"].stop()


@pytest.fixture
def client(app):
    return app.test_client()


@pytest.fixture
def auth():
    return {"Authorization": f"Bearer {API_TOKEN}"}


@pytest.fixture
def sender(app):
    return app.extensions["battery_monitor"]["sender"]
