"""HTTP surface. See the API section of README.md for the firmware contract."""

from __future__ import annotations

import hmac
import logging
import math
import re
from typing import Any

from flask import Blueprint, Response, jsonify, request

from .config import Config
from .sender import MessageSender
from .signal_cli import SignalCli

log = logging.getLogger(__name__)

MAX_DEVICE_ID_LENGTH = 64
MAX_EVENT_ID_LENGTH = 128
EVENT_ID_RE = re.compile(r"^[A-Za-z0-9._:-]+\Z")
VALID_MSG_TYPES = frozenset({"INFO", "ALERT"})

# Wider than any real 12 V lead-acid reading, but narrow enough to reject
# nonsense that would otherwise be relayed into the Signal group verbatim.
MIN_VOLTAGE = -100.0
MAX_VOLTAGE = 1000.0


def _authorized(config: Config) -> bool:
    """Constant-time bearer token check.

    Compares bytes, not str. Werkzeug decodes header bytes as latin-1, so any
    byte >= 0x80 produces a non-ASCII str and ``hmac.compare_digest`` on two
    such str raises TypeError — an unauthenticated 500 on every endpoint.
    """
    header = request.headers.get("Authorization", "")
    expected = f"Bearer {config.api_token}"
    return hmac.compare_digest(
        header.encode("utf-8", "replace"), expected.encode("utf-8")
    )


def _is_number(value: Any) -> bool:
    # bool is a subclass of int; True would otherwise pass as voltage 1.
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _in_voltage_range(value: int | float) -> bool:
    """Range check that is safe for arbitrary-precision integers.

    ``math.isfinite`` raises OverflowError on an int beyond float range, and
    MAX_CONTENT_LENGTH leaves room for a several-thousand-digit literal. Only
    floats can be NaN or infinite, so the finiteness test is float-only and the
    comparison is left to Python's exact int/float ordering.
    """
    if isinstance(value, float) and not math.isfinite(value):
        return False
    return MIN_VOLTAGE <= value <= MAX_VOLTAGE


def _escape_label(value: str) -> str:
    """Escape a Prometheus label value (backslash, quote, newline)."""
    return value.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")


def create_blueprint(
    config: Config, sender: MessageSender, signal_cli: SignalCli
) -> Blueprint:
    bp = Blueprint("battery_monitor", __name__)

    @bp.post("/send")
    def send_message():
        if not _authorized(config):
            # Deliberately not counted as a send failure: an unauthenticated
            # caller must not be able to drive the health status.
            return jsonify(error="Unauthorized"), 401

        data = request.get_json(silent=True)
        if not isinstance(data, dict):
            sender.record_client_error("malformed JSON body")
            return jsonify(error="Body must be a JSON object"), 400

        error = _validate_payload(data)
        if error:
            sender.record_client_error(error)
            return jsonify(error=error), 400

        device_id = data["device_id"]
        msg_type = data["msg_type"].upper()
        voltage = data["voltage"]
        critical_voltage = data.get("critical_voltage")

        if msg_type == "INFO":
            message = f"{device_id}: Voltage {voltage} V."
        elif critical_voltage is not None and voltage <= critical_voltage:
            message = (
                f"ALERT {device_id}: Voltage {voltage} V is at or below "
                f"critical {critical_voltage} V. Charge the battery."
            )
        else:
            message = f"ALERT {device_id}: Voltage {voltage} V."

        result = sender.enqueue(data["event_id"], message)
        if result.value == "full":
            # The firmware retries non-2xx with the same event id.
            return jsonify(error="Send queue is full"), 503
        return jsonify(status=result.value), 202

    @bp.get("/signal-cli/version")
    def signal_version():
        # Same disclosure as /metrics — an exact dependency version is useful
        # to an attacker, so it is gated by the same switch.
        if config.metrics_require_auth and not _authorized(config):
            return jsonify(error="Unauthorized"), 401
        return (
            jsonify(
                local_version=signal_cli.local_version(),
                latest_version=signal_cli.latest_version(),
                update_available=signal_cli.update_available(),
            ),
            200,
        )

    @bp.get("/metrics")
    def metrics():
        if config.metrics_require_auth and not _authorized(config):
            return jsonify(error="Unauthorized"), 401

        # One snapshot: taking stats() and health() separately can emit
        # battery_monitor_healthy 1 next to consecutive_failures 3.
        status, stats = sender.health()
        local = signal_cli.local_version()
        latest = signal_cli.latest_version()

        lines = [
            "# HELP signal_cli_up Whether the signal-cli binary responded.",
            "# TYPE signal_cli_up gauge",
            f"signal_cli_up {1 if local else 0}",
            "# HELP signal_cli_update_available Newer signal-cli release exists.",
            "# TYPE signal_cli_update_available gauge",
            f"signal_cli_update_available {1 if signal_cli.update_available() else 0}",
            "# HELP battery_monitor_messages_sent_total Messages delivered.",
            "# TYPE battery_monitor_messages_sent_total counter",
            f"battery_monitor_messages_sent_total {stats.sent_total}",
            "# HELP battery_monitor_messages_failed_total Sends that failed.",
            "# TYPE battery_monitor_messages_failed_total counter",
            f"battery_monitor_messages_failed_total {stats.failed_total}",
            "# HELP battery_monitor_consecutive_failures Failures since last success.",
            "# TYPE battery_monitor_consecutive_failures gauge",
            f"battery_monitor_consecutive_failures {stats.consecutive_failures}",
            "# HELP battery_monitor_client_errors_total Requests rejected as invalid.",
            "# TYPE battery_monitor_client_errors_total counter",
            f"battery_monitor_client_errors_total {stats.client_errors_total}",
            "# HELP battery_monitor_queue_depth Messages waiting to be sent.",
            "# TYPE battery_monitor_queue_depth gauge",
            f"battery_monitor_queue_depth {stats.queued}",
            "# HELP battery_monitor_dead_letters Events requiring operator action.",
            "# TYPE battery_monitor_dead_letters gauge",
            f"battery_monitor_dead_letters {stats.dead_lettered}",
            "# HELP battery_monitor_worker_alive Sender thread is running.",
            "# TYPE battery_monitor_worker_alive gauge",
            f"battery_monitor_worker_alive {1 if stats.worker_alive else 0}",
            "# HELP battery_monitor_healthy Service health (1 = healthy).",
            "# TYPE battery_monitor_healthy gauge",
            f"battery_monitor_healthy {1 if status == 'healthy' else 0}",
        ]
        # Version strings come from a subprocess and from GitHub, so they are
        # escaped rather than interpolated raw — an unescaped quote would
        # corrupt the whole exposition format.
        if local:
            lines += [
                "# HELP signal_cli_local_version Installed signal-cli version.",
                "# TYPE signal_cli_local_version gauge",
                f'signal_cli_local_version{{version="{_escape_label(local)}"}} 1',
            ]
        if latest:
            lines += [
                "# HELP signal_cli_latest_version Latest released version.",
                "# TYPE signal_cli_latest_version gauge",
                f'signal_cli_latest_version{{version="{_escape_label(latest)}"}} 1',
            ]

        return Response(
            "\n".join(lines) + "\n", status=200, mimetype="text/plain"
        )

    @bp.get("/healthcheck")
    def healthcheck():
        if config.healthcheck_require_auth and not _authorized(config):
            return jsonify(error="Unauthorized"), 401

        status, stats = sender.health()
        body = {
            "status": status,
            "worker_alive": stats.worker_alive,
            "queue_depth": stats.queued,
            "dead_lettered": stats.dead_lettered,
            "messages_sent_total": stats.sent_total,
            "messages_failed_total": stats.failed_total,
            "consecutive_failures": stats.consecutive_failures,
        }
        # last_error can contain signal-cli stderr, so only show it to a caller
        # that authenticated.
        if _authorized(config):
            body["last_error"] = stats.last_error
            body["signal_cli_version"] = signal_cli.local_version()

        # 503 rather than 500: the service is up and answering, it just cannot
        # do its job right now. "degraded" stays 200 so a single transient
        # failure does not page anyone.
        return jsonify(body), (503 if status == "unhealthy" else 200)

    return bp


def _validate_payload(data: dict) -> str | None:
    """Return an error message, or None if the payload is acceptable."""
    for key in ("event_id", "device_id", "msg_type", "voltage"):
        if key not in data:
            return f"Missing required field: {key}"

    event_id = data["event_id"]
    if (
        not isinstance(event_id, str)
        or not 1 <= len(event_id) <= MAX_EVENT_ID_LENGTH
        or EVENT_ID_RE.fullmatch(event_id) is None
    ):
        return (
            f"event_id must be 1..{MAX_EVENT_ID_LENGTH} ASCII letters, digits, "
            "dot, underscore, colon or hyphen"
        )

    device_id = data["device_id"]
    if not isinstance(device_id, str) or not device_id.strip():
        return "device_id must be a non-empty string"
    if len(device_id) > MAX_DEVICE_ID_LENGTH:
        return f"device_id must be at most {MAX_DEVICE_ID_LENGTH} characters"
    # A newline would let an authenticated client forge extra lines in the
    # Signal group, e.g. "a\nb: Voltage 99 V." renders as two readings.
    if any(ch in device_id for ch in "\n\r") or any(ord(c) < 32 for c in device_id):
        return "device_id must not contain control characters"

    msg_type = data["msg_type"]
    if not isinstance(msg_type, str) or msg_type.upper() not in VALID_MSG_TYPES:
        return "msg_type must be 'INFO' or 'ALERT'"

    # Without this, a string voltage reached `voltage <= critical_voltage` and
    # raised TypeError, turning a bad request into a 500.
    voltage = data["voltage"]
    if not _is_number(voltage):
        return "voltage must be a number"
    if not _in_voltage_range(voltage):
        return f"voltage must be between {MIN_VOLTAGE} and {MAX_VOLTAGE}"

    critical = data.get("critical_voltage")
    if critical is not None:
        if not _is_number(critical):
            return "critical_voltage must be a number"
        if not _in_voltage_range(critical):
            return (
                f"critical_voltage must be between {MIN_VOLTAGE} and {MAX_VOLTAGE}"
            )

    return None
