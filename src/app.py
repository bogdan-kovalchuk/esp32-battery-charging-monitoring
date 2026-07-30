import os
import hmac
import time
import subprocess
import logging
import requests
import threading
from queue import Queue
from flask import Flask, request, jsonify
from dotenv import load_dotenv
from packaging import version

# ================================
# CONFIGURATION AND CONSTANTS
# ================================

# Load configuration from .env
load_dotenv()

SIGNAL_GROUP_ID = os.getenv("SIGNAL_GROUP_ID")
SIGNAL_USER = os.getenv("SIGNAL_USER")
FLASK_HOST = os.getenv("FLASK_HOST", "0.0.0.0")
FLASK_PORT = int(os.getenv("FLASK_PORT", "5000"))
API_TOKEN = os.getenv("API_TOKEN", "change-me-to-random-string")

# Error counters for health monitoring
failed_send_count = 0
failed_receive_count = 0
counter_lock = threading.Lock()

# Logging configuration
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

app = Flask(__name__)

# ================================
# QUEUE AND MESSAGE PROCESSING
# ================================

# Queue for message processing
message_queue = Queue()


def process_queue():
    """Processes messages from the queue sequentially."""
    while True:
        cmd = message_queue.get()
        try:
            subprocess.run(cmd, check=True, text=True, capture_output=True)
            logging.info("Message sent successfully")
            time.sleep(2)
        except subprocess.CalledProcessError as e:
            with counter_lock:
                global failed_send_count
                failed_send_count += 1
            logging.error(f"Error executing signal-cli: {e}")
        finally:
            message_queue.task_done()


# Start a background thread for queue processing
threading.Thread(target=process_queue, daemon=True).start()

# ================================
# VERSION HANDLING FUNCTIONS
# ================================


def get_local_signal_version():
    """Gets the local version of signal-cli."""
    try:
        result = subprocess.run(
            ["signal-cli", "--version"], capture_output=True, text=True, check=True
        )
        return result.stdout.strip().split(" ")[-1]  # Extract only the version number
    except (subprocess.CalledProcessError, FileNotFoundError):
        logging.error("signal-cli not found or failed to execute.")
        return None


def get_latest_signal_version():
    """Fetches the latest available signal-cli version from GitHub."""
    try:
        url = "https://api.github.com/repos/AsamK/signal-cli/releases/latest"
        response = requests.get(url, timeout=5)
        response.raise_for_status()
        return response.json()["tag_name"].lstrip("v")  # Remove "v" prefix if present
    except requests.RequestException as e:
        logging.error(f"Error fetching latest version from GitHub: {e}")
        return None


def is_update_available(local_version, latest_version):
    """Compares the local version with the latest version to check if an update is available."""
    try:
        local_version_obj = version.parse(local_version)
        latest_version_obj = version.parse(latest_version)
        return local_version_obj < latest_version_obj
    except version.InvalidVersion:
        logging.error("Invalid version format encountered.")
        return False


# ================================
# API ENDPOINTS
# ================================


@app.route("/send", methods=["POST"])
def send_message():
    """Handles POST requests to send messages via Signal."""
    global failed_send_count

    auth = request.headers.get("Authorization", "")
    expected = f"Bearer {API_TOKEN}"
    if not hmac.compare_digest(auth, expected):
        return {"error": "Unauthorized"}, 401

    if not request.is_json:
        logging.warning("Received non-JSON data.")
        failed_send_count += 1
        return {"error": "Invalid JSON format"}, 400

    data = request.get_json()
    if not all(key in data for key in ["device_id", "msg_type", "voltage"]):
        logging.warning("Missing required fields in request.")
        failed_send_count += 1
        return {"error": "Missing device_id, msg_type, or voltage"}, 400

    device_id = data["device_id"]
    msg_type = data["msg_type"].upper()
    voltage = data["voltage"]

    if msg_type == "INFO":
        message = f"{device_id}: Voltage {voltage} V."
    elif msg_type == "ALERT":
        critical_voltage = data.get("critical_voltage")
        if critical_voltage and voltage <= critical_voltage:
            message = f"ALERT {device_id}: Voltage {voltage} V is at or below critical {critical_voltage} V. Charge the battery."
        else:
            message = f"ALERT {device_id}: Voltage {voltage} V."
    else:
        logging.warning(f"Unknown msg_type: {msg_type}")
        return {"error": "Invalid msg_type, expected 'INFO' or 'ALERT'"}, 400

    cmd = [
        "signal-cli",
        "-u",
        SIGNAL_USER,
        "send",
        "-g",
        SIGNAL_GROUP_ID,
        "-m",
        message,
    ]
    message_queue.put(cmd)  # Add command to queue
    return {"status": "Message added to queue"}, 200


@app.route("/signal-cli/version", methods=["GET"])
def signal_version():
    """Returns information about the signal-cli version."""
    local_version = get_local_signal_version()
    latest_version = get_latest_signal_version()
    update_available = latest_version and is_update_available(
        local_version, latest_version
    )
    return (
        jsonify(
            {
                "local_version": local_version,
                "latest_version": latest_version,
                "update_available": update_available,
            }
        ),
        200,
    )


@app.route("/metrics", methods=["GET"])
def metrics():
    """Returns Prometheus-style metrics."""
    global failed_send_count, failed_receive_count
    local_version = get_local_signal_version()
    latest_version = get_latest_signal_version()
    return (
        "\n".join(
            filter(
                None,
                [
                    f'signal_cli_local_version{{version="{local_version}"}} 1',
                    (
                        f'signal_cli_latest_version{{version="{latest_version}"}} 1'
                        if latest_version
                        else ""
                    ),
                    f"signal_cli_update_available {1 if latest_version and is_update_available(local_version, latest_version) else 0}",
                    f"signal_cli_failed_send_messages {failed_send_count}",
                    f"signal_cli_failed_receive_messages {failed_receive_count}",
                ],
            )
        ),
        200,
        {"Content-Type": "text/plain"},
    )


@app.route("/healthcheck", methods=["GET"])
def healthcheck():
    """Returns server health status."""
    global failed_send_count, failed_receive_count
    local_version = get_local_signal_version()
    status = (
        "healthy"
        if failed_send_count == 0 and failed_receive_count == 0
        else "degraded"
    )
    return jsonify(
        {
            "status": status,
            "failed_send_messages": failed_send_count,
            "failed_receive_messages": failed_receive_count,
            "signal_cli_version": local_version,
        }
    ), (200 if status == "healthy" else 500)


# ================================
# MAIN EXECUTION
# ================================

if __name__ == "__main__":
    app.run(host=FLASK_HOST, port=FLASK_PORT)
