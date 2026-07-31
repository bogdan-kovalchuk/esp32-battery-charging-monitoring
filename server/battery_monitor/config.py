"""Where every runtime setting comes from, and what happens when it is missing.

Resolution order for the .env file, first hit wins:

1. the ``env_file`` argument to :func:`load_config` (used by the tests);
2. ``$BATTERY_MONITOR_ENV`` — an absolute path, for deployments that keep
   secrets outside the checkout;
3. ``server/.env`` — resolved relative to *this file*, not to the process
   working directory, so a systemd unit with the wrong ``WorkingDirectory``
   still finds it.

Real environment variables always win over the file: the file only fills in
what the environment has not already set. Nothing here has a usable default —
a missing or obviously-placeholder secret raises :class:`ConfigError` at
startup rather than letting the service run in an insecure state.
"""

from __future__ import annotations

import os
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Mapping

from dotenv import dotenv_values

# server/battery_monitor/config.py -> server/
#
# This only resolves usefully when running from the checkout, which is how the
# systemd unit in README.md runs it. After `pip install .` it would
# point at site-packages, so an installed deployment must set
# BATTERY_MONITOR_ENV or pass the values as real environment variables.
PACKAGE_DIR = Path(__file__).resolve().parent
SERVER_DIR = PACKAGE_DIR.parent
DEFAULT_ENV_FILE = SERVER_DIR / ".env"

# E.164: a leading +, a non-zero country code, 8-15 digits total.
# \Z rather than $: $ also matches before a trailing newline.
E164_RE = re.compile(r"^\+[1-9]\d{7,14}\Z")

# Placeholders that appear in this repository's git history and in
# .env.example. Accepting any of them would mean the deployment is running with
# a credential that is public knowledge.
PLACEHOLDER_SECRETS = frozenset(
    {
        "",
        "change-me-to-random-string",
        "CHANGE_ME",
        "changeme",
        "your-signal-group-id",
        "same-token-as-in-secrets-h",
    }
)

MIN_API_TOKEN_LENGTH = 16

# Sentinel for "read no .env file at all". Passing None means "use the normal
# resolution order", which would silently pick up a developer's real
# server/.env — the tests need a way to opt out entirely.
NO_ENV_FILE = object()


class ConfigError(RuntimeError):
    """Raised when the environment cannot produce a safe configuration."""


@dataclass(frozen=True)
class Config:
    """Fully validated runtime settings. Constructed only by load_config()."""

    # Signal identity. signal_user is the phone number registered with
    # signal-cli — this is the "where does the phone number come from" answer:
    # SIGNAL_USER in the .env file described in this module's docstring.
    signal_user: str
    signal_group_id: str

    # Shared secret with the ESP32; must match API_TOKEN in
    # firmware/include/secrets.h.
    api_token: str

    host: str
    port: int

    signal_cli_bin: str
    outbox_db_path: Path

    # How long a looked-up signal-cli version stays fresh. Without this, every
    # /metrics scrape shells out to signal-cli and calls the GitHub API, which
    # exhausts GitHub's 60-requests-per-hour unauthenticated limit within
    # minutes of pointing Prometheus at the service.
    local_version_ttl: int
    latest_version_ttl: int

    # Largest accepted request body. The ESP32 posts well under 1 KB.
    max_content_length: int

    # /send always requires the token. These two are separate because
    # monitoring systems often cannot send an Authorization header, while
    # leaving /metrics open exposes the signal-cli version to the internet.
    metrics_require_auth: bool
    healthcheck_require_auth: bool

    # Consecutive send failures before /healthcheck reports unhealthy. The old
    # implementation used a total counter that never reset, so a single
    # transient failure pinned the endpoint at HTTP 500 forever.
    unhealthy_after_failures: int

    env_file: Path | None = field(default=None, compare=False)

    def describe_sources(self) -> str:
        """Human-readable summary for startup logs — no secret values."""
        origin = str(self.env_file) if self.env_file else "environment only"
        return (
            f"config loaded from {origin}: "
            f"SIGNAL_USER={_mask(self.signal_user)} "
            f"SIGNAL_GROUP_ID={_mask(self.signal_group_id)} "
            f"API_TOKEN={_mask(self.api_token)} "
            f"bind={self.host}:{self.port} "
            f"signal_cli={self.signal_cli_bin} "
            f"outbox={self.outbox_db_path}"
        )


def _mask(value: str) -> str:
    """Show enough to identify a value, never enough to reuse it."""
    # Below this length the 2+2 preview would reveal most of the secret.
    if len(value) < 12:
        return f"**** ({len(value)} chars)"
    return f"{value[:2]}...{value[-2:]} ({len(value)} chars)"


def _resolve_env_file(
    env_file: str | os.PathLike[str] | None, environ: Mapping[str, str]
) -> Path | None:
    if env_file is NO_ENV_FILE:
        return None
    if env_file is not None:
        path = Path(env_file)
        if not path.is_file():
            raise ConfigError(f"env file not found: {path}")
        return path

    override = environ.get("BATTERY_MONITOR_ENV")
    if override:
        path = Path(override)
        if not path.is_absolute():
            raise ConfigError(
                f"BATTERY_MONITOR_ENV must be an absolute path, got {override!r}. "
                f"A relative path resolves against the working directory, which "
                f"is exactly what this setting exists to avoid."
            )
        if not path.is_file():
            raise ConfigError(
                f"BATTERY_MONITOR_ENV points at {path}, which does not exist"
            )
        return path

    return DEFAULT_ENV_FILE if DEFAULT_ENV_FILE.is_file() else None


def _require(values: Mapping[str, str], name: str, source: str) -> str:
    value = values.get(name)
    if value is None or value.strip() == "":
        raise ConfigError(
            f"{name} is not set. Add it to {source}. "
            f"See server/.env.example for the full list."
        )
    return value.strip()


def _int(values: Mapping[str, str], name: str, default: int, source: str) -> int:
    raw = values.get(name)
    if raw is None or raw.strip() == "":
        return default
    try:
        return int(raw.strip())
    except ValueError as exc:
        raise ConfigError(f"{name} in {source} must be an integer, got {raw!r}") from exc


def _bool(values: Mapping[str, str], name: str, default: bool, source: str) -> bool:
    raw = values.get(name)
    if raw is None or raw.strip() == "":
        return default
    normalized = raw.strip().lower()
    if normalized in {"1", "true", "yes", "on"}:
        return True
    if normalized in {"0", "false", "no", "off"}:
        return False
    raise ConfigError(f"{name} in {source} must be a boolean, got {raw!r}")


def load_config(
    env_file: str | os.PathLike[str] | None | object = None,
    environ: Mapping[str, str] | None = None,
) -> Config:
    """Build a validated Config or raise ConfigError explaining what is wrong."""
    environ = os.environ if environ is None else environ
    resolved = _resolve_env_file(env_file, environ)

    # Real environment variables take precedence over the file, so a systemd
    # Environment= line or a container secret can override the checkout.
    values: dict[str, str] = {}
    if resolved is not None:
        values.update({k: v for k, v in dotenv_values(resolved).items() if v is not None})
    values.update({k: v for k, v in environ.items() if v is not None})

    source = str(resolved) if resolved else "the process environment"

    signal_user = _require(values, "SIGNAL_USER", source)
    if not E164_RE.match(signal_user):
        raise ConfigError(
            f"SIGNAL_USER must be an E.164 phone number such as +380501234567, "
            f"got {signal_user!r}. This is the number registered with "
            f"signal-cli."
        )

    signal_group_id = _require(values, "SIGNAL_GROUP_ID", source)
    if signal_group_id in PLACEHOLDER_SECRETS:
        raise ConfigError(
            f"SIGNAL_GROUP_ID is still the placeholder {signal_group_id!r}. "
            f"Get the real value with: signal-cli -a {signal_user} listGroups"
        )

    api_token = _require(values, "API_TOKEN", source)
    if api_token in PLACEHOLDER_SECRETS:
        raise ConfigError(
            "API_TOKEN is a placeholder that is published in this repository, "
            "so it grants no protection. Generate one with: "
            'python -c "import secrets; print(secrets.token_urlsafe(32))"'
        )
    if len(api_token) < MIN_API_TOKEN_LENGTH:
        raise ConfigError(
            f"API_TOKEN must be at least {MIN_API_TOKEN_LENGTH} characters, "
            f"got {len(api_token)}. It must match API_TOKEN in "
            f"firmware/include/secrets.h."
        )

    port = _int(values, "FLASK_PORT", 5000, source)
    if not 1 <= port <= 65535:
        raise ConfigError(f"FLASK_PORT must be in 1..65535, got {port}")

    unhealthy_after = _int(values, "UNHEALTHY_AFTER_FAILURES", 3, source)
    if unhealthy_after < 1:
        raise ConfigError("UNHEALTHY_AFTER_FAILURES must be at least 1")

    # 256 rather than 1: anything smaller rejects the firmware's own payload.
    max_content_length = _int(values, "MAX_CONTENT_LENGTH", 4096, source)
    if max_content_length < 256:
        raise ConfigError(
            f"MAX_CONTENT_LENGTH must be at least 256 bytes, got "
            f"{max_content_length}. Smaller values reject the firmware's own "
            f"payload."
        )

    # A zero or negative TTL disables caching entirely, which re-opens the
    # GitHub rate-limit and DoS-amplification problem the cache exists to fix.
    local_version_ttl = _int(values, "LOCAL_VERSION_TTL", 300, source)
    latest_version_ttl = _int(values, "LATEST_VERSION_TTL", 3600, source)
    for name, ttl in (
        ("LOCAL_VERSION_TTL", local_version_ttl),
        ("LATEST_VERSION_TTL", latest_version_ttl),
    ):
        if ttl < 1:
            raise ConfigError(
                f"{name} must be at least 1 second; a non-positive value "
                f"disables caching and lets every scrape hit signal-cli and "
                f"the GitHub API."
            )

    outbox_raw = values.get("OUTBOX_DB_PATH", ".data/outbox.sqlite3").strip()
    if not outbox_raw:
        raise ConfigError("OUTBOX_DB_PATH must not be empty")
    outbox_path = Path(outbox_raw)
    if not outbox_path.is_absolute():
        outbox_path = (SERVER_DIR / outbox_path).resolve()

    return Config(
        signal_user=signal_user,
        signal_group_id=signal_group_id,
        api_token=api_token,
        host=values.get("FLASK_HOST", "127.0.0.1").strip() or "127.0.0.1",
        port=port,
        signal_cli_bin=values.get("SIGNAL_CLI_BIN", "signal-cli").strip()
        or "signal-cli",
        outbox_db_path=outbox_path,
        local_version_ttl=local_version_ttl,
        latest_version_ttl=latest_version_ttl,
        max_content_length=max_content_length,
        metrics_require_auth=_bool(values, "METRICS_REQUIRE_AUTH", True, source),
        healthcheck_require_auth=_bool(
            values, "HEALTHCHECK_REQUIRE_AUTH", False, source
        ),
        unhealthy_after_failures=unhealthy_after,
        env_file=resolved,
    )
