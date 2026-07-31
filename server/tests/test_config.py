"""The configuration layer must refuse to produce an insecure Config."""

from __future__ import annotations

import pytest

from battery_monitor.config import ConfigError, load_config
from tests.conftest import API_TOKEN, GROUP_ID, SIGNAL_USER, make_env


def test_valid_environment_loads():
    config = load_config(environ=make_env())
    assert config.signal_user == SIGNAL_USER
    assert config.signal_group_id == GROUP_ID
    assert config.api_token == API_TOKEN
    # Loopback by default: exposing the port directly would put the bearer
    # token on the wire in cleartext.
    assert config.host == "127.0.0.1"
    assert config.port == 5000


@pytest.mark.parametrize("missing", ["SIGNAL_USER", "SIGNAL_GROUP_ID", "API_TOKEN"])
def test_missing_required_value_is_fatal(missing):
    env = make_env()
    del env[missing]
    with pytest.raises(ConfigError) as exc:
        load_config(environ=env)
    assert missing in str(exc.value)


@pytest.mark.parametrize("blank", ["", "   "])
def test_whitespace_only_value_is_fatal(blank):
    with pytest.raises(ConfigError):
        load_config(environ=make_env(API_TOKEN=blank))


@pytest.mark.parametrize(
    "number",
    ["0501234567", "380501234567", "+0501234567", "+38050", "not-a-number", "+"],
)
def test_invalid_phone_number_is_fatal(number):
    with pytest.raises(ConfigError) as exc:
        load_config(environ=make_env(SIGNAL_USER=number))
    assert "E.164" in str(exc.value)


@pytest.mark.parametrize("token", ["change-me-to-random-string", "CHANGE_ME", "changeme"])
def test_published_placeholder_token_is_fatal(token):
    with pytest.raises(ConfigError) as exc:
        load_config(environ=make_env(API_TOKEN=token))
    assert "placeholder" in str(exc.value)


def test_short_token_is_fatal():
    with pytest.raises(ConfigError) as exc:
        load_config(environ=make_env(API_TOKEN="short"))
    assert "16 characters" in str(exc.value)


def test_placeholder_group_id_is_fatal():
    with pytest.raises(ConfigError):
        load_config(environ=make_env(SIGNAL_GROUP_ID="your-signal-group-id"))


@pytest.mark.parametrize("port", ["0", "65536", "-1"])
def test_out_of_range_port_is_fatal(port):
    with pytest.raises(ConfigError):
        load_config(environ=make_env(FLASK_PORT=port))


def test_non_numeric_port_is_fatal():
    # The original code did int(os.getenv(...)) at import time, so this raised
    # an unhandled ValueError before anything could log it.
    with pytest.raises(ConfigError) as exc:
        load_config(environ=make_env(FLASK_PORT="five thousand"))
    assert "integer" in str(exc.value)


@pytest.mark.parametrize(
    ("raw", "expected"),
    [("true", True), ("1", True), ("yes", True), ("false", False), ("0", False)],
)
def test_boolean_parsing(raw, expected):
    config = load_config(environ=make_env(METRICS_REQUIRE_AUTH=raw))
    assert config.metrics_require_auth is expected


def test_invalid_boolean_is_fatal():
    with pytest.raises(ConfigError):
        load_config(environ=make_env(METRICS_REQUIRE_AUTH="maybe"))


def test_env_file_is_read_and_environment_wins(tmp_path):
    env_file = tmp_path / ".env"
    env_file.write_text(
        f"SIGNAL_USER={SIGNAL_USER}\n"
        f"SIGNAL_GROUP_ID={GROUP_ID}\n"
        f"API_TOKEN=token-from-the-dotenv-file\n"
        f"FLASK_PORT=6001\n",
        encoding="utf-8",
    )

    from_file = load_config(env_file=env_file, environ={})
    assert from_file.api_token == "token-from-the-dotenv-file"
    assert from_file.port == 6001
    assert from_file.env_file == env_file

    # A real environment variable must override the file, so a systemd
    # Environment= line or a container secret wins over the checkout.
    overridden = load_config(
        env_file=env_file, environ={"API_TOKEN": "token-from-the-environment"}
    )
    assert overridden.api_token == "token-from-the-environment"


def test_missing_explicit_env_file_is_fatal(tmp_path):
    with pytest.raises(ConfigError) as exc:
        load_config(env_file=tmp_path / "nope.env", environ=make_env())
    assert "not found" in str(exc.value)


def test_battery_monitor_env_override_is_honoured(tmp_path):
    env_file = tmp_path / "elsewhere.env"
    env_file.write_text("API_TOKEN=token-from-the-override-path\n", encoding="utf-8")
    env = make_env()
    del env["API_TOKEN"]
    env["BATTERY_MONITOR_ENV"] = str(env_file)

    config = load_config(environ=env)
    assert config.api_token == "token-from-the-override-path"
    assert config.env_file == env_file


def test_bad_battery_monitor_env_path_is_fatal():
    with pytest.raises(ConfigError) as exc:
        load_config(environ=make_env(BATTERY_MONITOR_ENV="/no/such/file.env"))
    assert "BATTERY_MONITOR_ENV" in str(exc.value)


def test_describe_sources_never_reveals_a_secret():
    config = load_config(environ=make_env())
    described = config.describe_sources()
    assert API_TOKEN not in described
    assert GROUP_ID not in described
    # Enough of the phone number to identify it, not enough to reuse it.
    assert SIGNAL_USER not in described
