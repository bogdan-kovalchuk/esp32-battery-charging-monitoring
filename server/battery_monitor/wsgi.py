"""Production entry point.

Run with:

    python -m battery_monitor.wsgi

The previous implementation called ``app.run()``, which is the Flask
development server — single-threaded, no request limits, and explicitly not
meant to face a network. waitress is a pure-Python WSGI server with no build
dependencies, which suits a small VPS.
"""

from __future__ import annotations

import logging
import sys

from waitress import serve

from .app import configure_logging, create_app
from .config import ConfigError, load_config
from .signal_cli import SignalCli

log = logging.getLogger(__name__)


def main() -> int:
    configure_logging()
    try:
        config = load_config()
    except ConfigError as exc:
        # Fail loudly at startup instead of running with a missing or
        # placeholder credential.
        log.error("configuration error: %s", exc)
        return 1

    signal_cli = SignalCli(
        binary=config.signal_cli_bin,
        user=config.signal_user,
        group_id=config.signal_group_id,
        local_version_ttl=config.local_version_ttl,
        latest_version_ttl=config.latest_version_ttl,
    )
    ready, error = signal_cli.check_ready()
    if not ready:
        log.error("signal-cli readiness check failed: %s", error)
        return 1

    app = create_app(config, signal_cli=signal_cli)
    log.info("listening on http://%s:%d", config.host, config.port)
    serve(app, host=config.host, port=config.port, threads=4)
    return 0


if __name__ == "__main__":
    sys.exit(main())
