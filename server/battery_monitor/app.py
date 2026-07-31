"""Application factory.

There is deliberately no module-level ``app``: importing this module must not
read the environment or start a thread, so tests can build an app from an
explicit Config. The production entry point is :mod:`battery_monitor.wsgi`.
"""

from __future__ import annotations

import logging

from flask import Flask, jsonify

from .config import Config, load_config
from .routes import create_blueprint
from .sender import MessageSender
from .signal_cli import SignalCli

log = logging.getLogger(__name__)


def configure_logging(level: int = logging.INFO) -> None:
    logging.basicConfig(
        level=level, format="%(asctime)s - %(levelname)s - %(name)s - %(message)s"
    )


def create_app(
    config: Config | None = None,
    sender: MessageSender | None = None,
    signal_cli: SignalCli | None = None,
    start_worker: bool = True,
) -> Flask:
    config = config or load_config()

    signal_cli = signal_cli or SignalCli(
        binary=config.signal_cli_bin,
        user=config.signal_user,
        group_id=config.signal_group_id,
        local_version_ttl=config.local_version_ttl,
        latest_version_ttl=config.latest_version_ttl,
    )
    sender = sender or MessageSender(
        signal_cli,
        unhealthy_after_failures=config.unhealthy_after_failures,
        db_path=config.outbox_db_path,
    )
    if start_worker:
        sender.start()

    app = Flask(__name__)
    # Flask returns 413 above this, so an oversized body is rejected before it
    # is fully buffered.
    app.config["MAX_CONTENT_LENGTH"] = config.max_content_length
    app.register_blueprint(create_blueprint(config, sender, signal_cli))

    # Keep the collaborators reachable for tests and graceful shutdown.
    app.extensions["battery_monitor"] = {
        "config": config,
        "sender": sender,
        "signal_cli": signal_cli,
    }

    @app.errorhandler(404)
    def _not_found(_error):
        return jsonify(error="Not found"), 404

    @app.errorhandler(405)
    def _method_not_allowed(_error):
        return jsonify(error="Method not allowed"), 405

    @app.errorhandler(413)
    def _too_large(_error):
        return jsonify(error="Request body too large"), 413

    @app.errorhandler(500)
    def _server_error(error):
        log.exception("unhandled error: %s", error)
        return jsonify(error="Internal server error"), 500

    log.info(config.describe_sources())
    return app
