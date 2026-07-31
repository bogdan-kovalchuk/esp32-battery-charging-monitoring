"""Durable outbox that serialises and retries signal-cli calls."""

from __future__ import annotations

import logging
import random
import sqlite3
import threading
import time
from dataclasses import dataclass
from enum import Enum
from pathlib import Path

from .signal_cli import SignalCli, SignalCliError

log = logging.getLogger(__name__)

DEFAULT_SEND_INTERVAL_S = 2.0
DEFAULT_RETRY_BASE_S = 5.0
DEFAULT_RETRY_MAX_S = 300.0
DEFAULT_MAX_ATTEMPTS = 8
DEFAULT_DEAD_LETTER_RETENTION_S = 30 * 24 * 60 * 60


class EnqueueResult(str, Enum):
    QUEUED = "queued"
    DUPLICATE = "duplicate"
    FULL = "full"


@dataclass(frozen=True)
class SenderStats:
    queued: int
    dead_lettered: int
    sent_total: int
    failed_total: int
    client_errors_total: int
    consecutive_failures: int
    worker_alive: bool
    last_error: str | None
    seconds_since_last_success: float | None


@dataclass(frozen=True)
class _OutboxItem:
    event_id: str
    message: str
    attempts: int


class MessageSender:
    """Owns a persistent SQLite outbox and exactly one sender worker."""

    def __init__(
        self,
        signal_cli: SignalCli,
        unhealthy_after_failures: int = 3,
        send_interval_s: float = DEFAULT_SEND_INTERVAL_S,
        queue_maxsize: int = 1000,
        db_path: str | Path = ":memory:",
        retry_base_s: float = DEFAULT_RETRY_BASE_S,
        retry_max_s: float = DEFAULT_RETRY_MAX_S,
        max_attempts: int = DEFAULT_MAX_ATTEMPTS,
        dead_letter_retention_s: int = DEFAULT_DEAD_LETTER_RETENTION_S,
        retry_jitter: float = 0.2,
    ) -> None:
        self._signal_cli = signal_cli
        self._unhealthy_after = unhealthy_after_failures
        self._send_interval_s = send_interval_s
        self._queue_maxsize = queue_maxsize
        self._retry_base_s = retry_base_s
        self._retry_max_s = retry_max_s
        self._max_attempts = max_attempts
        self._dead_letter_retention_s = dead_letter_retention_s
        self._retry_jitter = retry_jitter

        if queue_maxsize < 1 or max_attempts < 1:
            raise ValueError("queue_maxsize and max_attempts must be positive")
        if retry_base_s < 0 or retry_max_s < retry_base_s:
            raise ValueError("invalid retry interval")

        if str(db_path) != ":memory:":
            path = Path(db_path)
            path.parent.mkdir(parents=True, exist_ok=True)
        self._db = sqlite3.connect(
            str(db_path), check_same_thread=False, isolation_level=None, timeout=5
        )
        self._db.row_factory = sqlite3.Row
        self._db_lock = threading.Lock()
        self._stats_lock = threading.Lock()
        self._wake = threading.Condition()
        self._thread_lock = threading.Lock()
        self._stop_requested = threading.Event()
        self._in_flight = False

        self._thread: threading.Thread | None = None
        self._sent_total = 0
        self._failed_total = 0
        self._consecutive_failures = 0
        self._client_errors_total = 0
        self._last_error: str | None = None
        self._last_success_at: float | None = None
        self._init_db()

    def _init_db(self) -> None:
        with self._db_lock:
            if self._db.execute("PRAGMA database_list").fetchone()[2]:
                self._db.execute("PRAGMA journal_mode=WAL")
                self._db.execute("PRAGMA synchronous=FULL")
            self._db.execute(
                """
                CREATE TABLE IF NOT EXISTS outbox (
                    event_id TEXT PRIMARY KEY,
                    message TEXT NOT NULL,
                    status TEXT NOT NULL CHECK(status IN ('pending', 'delivered', 'dead')),
                    attempts INTEGER NOT NULL DEFAULT 0,
                    next_attempt_at REAL NOT NULL DEFAULT 0,
                    created_at REAL NOT NULL,
                    failed_at REAL,
                    delivered_at REAL,
                    last_error TEXT
                )
                """
            )
            self._db.execute(
                "CREATE INDEX IF NOT EXISTS outbox_ready "
                "ON outbox(status, next_attempt_at, created_at)"
            )
            self._purge_terminal_locked(time.time())

    # -- lifecycle ---------------------------------------------------------

    def start(self) -> None:
        with self._thread_lock:
            if self._thread is not None and self._thread.is_alive():
                return
            self._stop_requested.clear()
            self._thread = threading.Thread(
                target=self._run, name="signal-sender", daemon=True
            )
            self._thread.start()

    def stop(self, timeout: float = 5.0) -> bool:
        """Stop the worker; pending events remain durable for the next start."""
        with self._thread_lock:
            thread = self._thread
            if thread is None:
                return True
            self._stop_requested.set()
            with self._wake:
                self._wake.notify_all()
            thread.join(timeout)
            if thread.is_alive():
                log.error("sender thread still running after %.1fs", timeout)
                return False
            self._thread = None
            return True

    # -- producer side -----------------------------------------------------

    def enqueue(self, event_id: str, message: str) -> EnqueueResult:
        """Persist one event atomically, deduplicating by firmware event id."""
        now = time.time()
        with self._db_lock:
            self._db.execute("BEGIN IMMEDIATE")
            try:
                self._purge_terminal_locked(now)
                if self._db.execute(
                    "SELECT 1 FROM outbox WHERE event_id = ?", (event_id,)
                ).fetchone():
                    self._db.execute("COMMIT")
                    return EnqueueResult.DUPLICATE
                pending = self._db.execute(
                    "SELECT COUNT(*) FROM outbox WHERE status = 'pending'"
                ).fetchone()[0]
                if pending >= self._queue_maxsize:
                    self._db.execute("ROLLBACK")
                    with self._stats_lock:
                        self._last_error = "durable outbox is full"
                    log.error("durable outbox full, rejecting event %s", event_id)
                    return EnqueueResult.FULL
                self._db.execute(
                    "INSERT INTO outbox "
                    "(event_id, message, status, attempts, next_attempt_at, created_at) "
                    "VALUES (?, ?, 'pending', 0, 0, ?)",
                    (event_id, message, now),
                )
                self._db.execute("COMMIT")
            except BaseException:
                if self._db.in_transaction:
                    self._db.execute("ROLLBACK")
                raise
        with self._wake:
            self._wake.notify()
        return EnqueueResult.QUEUED

    def wait_idle(self, timeout: float = 5.0) -> bool:
        """Wait until no pending or in-flight events remain (dead letters excluded)."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self._pending_count() == 0 and not self._in_flight:
                return True
            time.sleep(0.005)
        return False

    def record_client_error(self, reason: str) -> None:
        with self._stats_lock:
            self._client_errors_total += 1

    # -- worker ------------------------------------------------------------

    def _run(self) -> None:
        while not self._stop_requested.is_set():
            maintain = getattr(self._signal_cli, "maintenance", None)
            if callable(maintain):
                try:
                    maintain()
                except SignalCliError as exc:
                    # Maintenance has its own bounded retry deadline. Pending
                    # events are still attempted and will drive normal health.
                    log.warning("signal-cli receive maintenance failed: %s", exc)
            item, wait_s = self._next_ready()
            if item is None:
                with self._wake:
                    self._wake.wait(timeout=min(wait_s, 1.0))
                continue

            self._in_flight = True
            try:
                self._deliver(item)
            except BaseException:  # noqa: BLE001 - worker must remain alive
                log.exception("unexpected error while sending; worker continues")
                self._record_failure(item, "internal error while sending")
            finally:
                self._in_flight = False

    def _next_ready(self) -> tuple[_OutboxItem | None, float]:
        now = time.time()
        with self._db_lock:
            row = self._db.execute(
                "SELECT event_id, message, attempts, next_attempt_at "
                "FROM outbox WHERE status = 'pending' "
                "ORDER BY next_attempt_at, created_at LIMIT 1"
            ).fetchone()
        if row is None:
            return None, 1.0
        wait_s = max(0.0, row["next_attempt_at"] - now)
        if wait_s > 0:
            return None, wait_s
        return _OutboxItem(row["event_id"], row["message"], row["attempts"]), 0.0

    def _deliver(self, item: _OutboxItem) -> None:
        try:
            self._signal_cli.send(item.message)
        except SignalCliError as exc:
            log.error("send failed for %s: %s", item.event_id, exc)
            self._record_failure(item, str(exc))
            return

        with self._db_lock:
            self._db.execute(
                "UPDATE outbox SET status = 'delivered', delivered_at = ?, "
                "last_error = NULL WHERE event_id = ?",
                (time.time(), item.event_id),
            )
        with self._stats_lock:
            self._sent_total += 1
            self._consecutive_failures = 0
            self._last_error = None
            self._last_success_at = time.monotonic()
        log.info("message sent for event %s", item.event_id)
        if self._send_interval_s:
            self._stop_requested.wait(self._send_interval_s)

    def _record_failure(self, item: _OutboxItem, error: str) -> None:
        attempts = item.attempts + 1
        now = time.time()
        with self._db_lock:
            if attempts >= self._max_attempts:
                self._db.execute(
                    "UPDATE outbox SET status = 'dead', attempts = ?, failed_at = ?, "
                    "last_error = ? WHERE event_id = ?",
                    (attempts, now, error[:500], item.event_id),
                )
            else:
                delay = min(
                    self._retry_max_s,
                    self._retry_base_s * (2 ** max(0, attempts - 1)),
                )
                if delay and self._retry_jitter:
                    delay *= random.uniform(1 - self._retry_jitter, 1 + self._retry_jitter)
                self._db.execute(
                    "UPDATE outbox SET attempts = ?, next_attempt_at = ?, "
                    "last_error = ? WHERE event_id = ?",
                    (attempts, now + delay, error[:500], item.event_id),
                )
        with self._stats_lock:
            self._failed_total += 1
            self._consecutive_failures += 1
            self._last_error = error[:500]
        with self._wake:
            self._wake.notify()

    # -- health ------------------------------------------------------------

    def _pending_count(self) -> int:
        with self._db_lock:
            return self._db.execute(
                "SELECT COUNT(*) FROM outbox WHERE status = 'pending'"
            ).fetchone()[0]

    def _dead_count(self) -> int:
        with self._db_lock:
            return self._db.execute(
                "SELECT COUNT(*) FROM outbox WHERE status = 'dead'"
            ).fetchone()[0]

    def _purge_terminal_locked(self, now: float) -> None:
        cutoff = now - self._dead_letter_retention_s
        self._db.execute(
            "DELETE FROM outbox WHERE status = 'dead' AND failed_at < ?", (cutoff,)
        )
        self._db.execute(
            "DELETE FROM outbox WHERE status = 'delivered' AND delivered_at < ?",
            (cutoff,),
        )

    def stats(self) -> SenderStats:
        queued = self._pending_count()
        dead = self._dead_count()
        with self._stats_lock:
            since = (
                None
                if self._last_success_at is None
                else time.monotonic() - self._last_success_at
            )
            return SenderStats(
                queued=queued,
                dead_lettered=dead,
                sent_total=self._sent_total,
                failed_total=self._failed_total,
                client_errors_total=self._client_errors_total,
                consecutive_failures=self._consecutive_failures,
                worker_alive=self._thread is not None and self._thread.is_alive(),
                last_error=self._last_error,
                seconds_since_last_success=since,
            )

    def health(self) -> tuple[str, SenderStats]:
        stats = self.stats()
        if not stats.worker_alive or stats.dead_lettered > 0:
            return "unhealthy", stats
        if stats.consecutive_failures >= self._unhealthy_after:
            return "unhealthy", stats
        if stats.consecutive_failures > 0:
            return "degraded", stats
        return "healthy", stats
