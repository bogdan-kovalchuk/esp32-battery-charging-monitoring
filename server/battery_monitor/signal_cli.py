"""Everything that shells out to signal-cli, plus cached version lookups."""

from __future__ import annotations

import json
import logging
import subprocess
import threading
import time
from typing import Any, Callable

import requests
from packaging import version as pkg_version

log = logging.getLogger(__name__)

GITHUB_LATEST_URL = "https://api.github.com/repos/AsamK/signal-cli/releases/latest"
GITHUB_TIMEOUT_S = 5
SEND_TIMEOUT_S = 60
VERSION_TIMEOUT_S = 15
RECEIVE_TIMEOUT_S = 15
RECEIVE_INTERVAL_S = 6 * 60 * 60
RECEIVE_RETRY_INTERVAL_S = 5 * 60


class SignalCliError(RuntimeError):
    """signal-cli could not deliver a message."""


class TtlCache:
    """Single-value cache with a time-to-live.

    The producer runs OUTSIDE the lock, with a refresh flag so a burst of
    concurrent scrapes triggers exactly one subprocess or HTTP call. Holding
    the lock across the producer instead would park every waitress worker
    thread for up to VERSION_TIMEOUT_S on a hung signal-cli, starving the
    ESP32's /send. While a refresh is in flight, callers get the previous
    value rather than queueing behind it.

    Failures are cached for a shorter TTL than successes: retrying on every
    scrape would burn the GitHub rate limit, but pinning a stale failure for
    the full hour would hide a recovery.
    """

    def __init__(self, ttl_seconds: float, failure_ttl_seconds: float = 30.0) -> None:
        self._ttl = ttl_seconds
        self._failure_ttl = min(failure_ttl_seconds, ttl_seconds)
        self._lock = threading.Lock()
        self._value: Any = None
        self._expires_at = 0.0
        self._refreshing = False

    def get(self, producer: Callable[[], Any]) -> Any:
        with self._lock:
            if time.monotonic() < self._expires_at or self._refreshing:
                return self._value
            self._refreshing = True

        value = None
        try:
            value = producer()
        except Exception:  # noqa: BLE001 - a lookup must never break a request
            log.exception("cached lookup failed")

        with self._lock:
            self._value = value
            ttl = self._ttl if value is not None else self._failure_ttl
            self._expires_at = time.monotonic() + ttl
            self._refreshing = False
        return value

    def invalidate(self) -> None:
        with self._lock:
            self._expires_at = 0.0


class SignalCli:
    """Thin wrapper over the signal-cli binary."""

    def __init__(
        self,
        binary: str,
        user: str,
        group_id: str,
        local_version_ttl: float = 300,
        latest_version_ttl: float = 3600,
    ) -> None:
        self.binary = binary
        self.user = user
        self.group_id = group_id
        self._local_cache = TtlCache(local_version_ttl)
        self._latest_cache = TtlCache(latest_version_ttl)
        self._receive_lock = threading.Lock()
        self._next_receive_at = 0.0

    def send_argv(self, message: str) -> list[str]:
        """Build the argv for one group message.

        Passed to subprocess as a list, never through a shell, so message
        content cannot be interpreted as arguments or shell syntax.
        """
        return [
            self.binary,
            "-a",
            self.user,
            "send",
            "-g",
            self.group_id,
            "-m",
            message,
        ]

    def send(self, message: str) -> None:
        """Send one message, raising SignalCliError on any failure."""
        self._receive_updates_if_due()
        try:
            subprocess.run(
                self.send_argv(message),
                check=True,
                text=True,
                capture_output=True,
                timeout=SEND_TIMEOUT_S,
            )
        except FileNotFoundError as exc:
            raise SignalCliError(f"{self.binary} not found on PATH") from exc
        except subprocess.TimeoutExpired as exc:
            raise SignalCliError(
                f"{self.binary} did not finish within {SEND_TIMEOUT_S}s"
            ) from exc
        except subprocess.CalledProcessError as exc:
            raise SignalCliError(
                f"{self.binary} exited {exc.returncode}: "
                f"{(exc.stderr or '').strip()[:500]}"
            ) from exc

    def _receive_updates_if_due(self) -> None:
        """Regularly consume pending envelopes before sending.

        This runs inside the single MessageSender worker, so it cannot race a
        send against signal-cli's account-data lock.
        """
        with self._receive_lock:
            now = time.monotonic()
            if now < self._next_receive_at:
                return
            argv = [
                self.binary,
                "-a",
                self.user,
                "receive",
                "--timeout",
                "1",
                "--ignore-attachments",
                "--ignore-stories",
                "--ignore-avatars",
                "--ignore-stickers",
            ]
            try:
                subprocess.run(
                    argv,
                    check=True,
                    text=True,
                    capture_output=True,
                    timeout=RECEIVE_TIMEOUT_S,
                )
            except (FileNotFoundError, subprocess.TimeoutExpired,
                    subprocess.CalledProcessError) as exc:
                # Avoid a tight retry loop when Signal or the network is down.
                self._next_receive_at = now + RECEIVE_RETRY_INTERVAL_S
                raise SignalCliError(f"signal-cli receive failed: {exc}") from exc
            self._next_receive_at = time.monotonic() + RECEIVE_INTERVAL_S

    def maintenance(self) -> None:
        """Receive protocol/account updates even when there are no alerts."""
        self._receive_updates_if_due()

    def check_ready(self) -> tuple[bool, str | None]:
        """Verify binary, account data and target group before serving traffic."""
        if not self._read_local_version():
            return False, "signal-cli --version failed"
        try:
            result = subprocess.run(
                [
                    self.binary,
                    "-a",
                    self.user,
                    "--output=json",
                    "listGroups",
                    "-g",
                    self.group_id,
                ],
                check=True,
                text=True,
                capture_output=True,
                timeout=VERSION_TIMEOUT_S,
            )
        except FileNotFoundError:
            return False, f"{self.binary} not found on PATH"
        except subprocess.TimeoutExpired:
            return False, "signal-cli listGroups timed out"
        except subprocess.CalledProcessError as exc:
            return False, (exc.stderr or "signal-cli listGroups failed").strip()[:500]

        # listGroups output shape has changed across releases. Parse JSON when
        # possible and fall back to an exact group-id occurrence in stdout.
        try:
            parsed = json.loads(result.stdout)
            serialized = json.dumps(parsed, ensure_ascii=False)
        except (json.JSONDecodeError, TypeError):
            serialized = result.stdout
        if self.group_id not in serialized:
            return False, "configured SIGNAL_GROUP_ID is not visible to this account"
        return True, None

    def local_version(self) -> str | None:
        return self._local_cache.get(self._read_local_version)

    def latest_version(self) -> str | None:
        return self._latest_cache.get(self._read_latest_version)

    def update_available(self) -> bool:
        local = self.local_version()
        latest = self.latest_version()
        if not local or not latest:
            return False
        try:
            return pkg_version.parse(local) < pkg_version.parse(latest)
        except pkg_version.InvalidVersion:
            log.warning("cannot compare versions %r and %r", local, latest)
            return False

    def _read_local_version(self) -> str | None:
        try:
            result = subprocess.run(
                [self.binary, "--version"],
                capture_output=True,
                text=True,
                check=True,
                timeout=VERSION_TIMEOUT_S,
            )
        except (
            subprocess.CalledProcessError,
            subprocess.TimeoutExpired,
            FileNotFoundError,
        ):
            log.warning("%s --version failed", self.binary)
            return None
        parts = result.stdout.strip().split()
        return parts[-1] if parts else None

    def _read_latest_version(self) -> str | None:
        try:
            response = requests.get(GITHUB_LATEST_URL, timeout=GITHUB_TIMEOUT_S)
            response.raise_for_status()
            tag = response.json().get("tag_name")
        except (requests.RequestException, ValueError) as exc:
            log.warning("could not fetch latest signal-cli version: %s", exc)
            return None
        return tag.lstrip("v") if isinstance(tag, str) else None
