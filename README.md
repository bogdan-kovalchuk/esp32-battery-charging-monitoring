# ESP32 Battery Charging Monitor

12 V lead-acid battery voltage monitor on ESP32 with Signal messenger
alerts. Two components: ESP32 firmware and Flask backend on a VPS.

```
[ESP32 + voltage divider] --HTTP POST--> [Flask server] --signal-cli--> [Signal group]
```

## Hardware

### BOM

- ESP32 DevKit V1
- 12 V lead-acid battery
- Resistor 30 kΩ (R1) + 7.5 kΩ (R2) — voltage divider
- NPN transistor (2N2222) or N-MOSFET — voltmeter enable
- Momentary push button
- 10 kΩ resistor — pull-down for GPIO 34

### Wiring

| Component | ESP32 pin | Notes |
|---|---:|---|
| Voltage divider output | GPIO 32 | ADC input, 0–3 V range |
| Button | GPIO 34 | To 3.3 V, **10 kΩ pull-down to GND required** |
| Voltmeter enable | GPIO 4 | Transistor base/gate via 1 kΩ |

> GPIO 34 is input-only with no internal pull-down.
> External 10 kΩ resistor between GPIO 34 and GND is mandatory.

### Voltage divider

```
  12V BAT ──[30 kΩ]──┬──[7.5 kΩ]── GND
                      │
                   GPIO 32
```

Scales 0–15 V down to 0–3 V for ESP32 ADC. Firmware averages 100 samples
per reading. Calibration factor `CORR_FACTOR = 1.0468` in `include/app_config.h`.

---

## Firmware

### Build and upload

```powershell
pio run -e esp32dev -t upload
pio device monitor -b 115200
```

### Configuration

Edit `include/secrets.h`:

| Define | Description | Example |
|---|---|---|
| `WIFI_SSID` | WiFi network name | `MyNetwork` |
| `WIFI_PASSWORD` | WiFi password | `secret123` |
| `SERVER_IP` | Server IP or hostname | `141.144.245.187` |
| `SERVER_PORT` | Flask backend port | `5000` |
| `AP_SSID` | Access point name | `BCM-AP` |
| `AP_PASSWORD` | Access point password | `12345678` |
| `DEVICE_ID` | Device identifier | `BATT#1` |
| `API_TOKEN` | Bearer token for server | `random-string` |
| `WEB_PASSWORD` | Web UI password | `admin` |
| `CRIT_VOLTAGE` | Critical voltage threshold (V) | `11.0` |
| `INFO_INTERVAL` | INFO message interval (min) | `300` |
| `CRIT_INTERVAL` | ALERT message interval (min) | `120` |

### WiFi modes

**STA mode** (default): ESP32 connects to the specified WiFi network.
Auto-reconnect is enabled. If connection is lost, automatic reconnect
attempts are made.

**AP mode** (fallback): If WiFi is unavailable for 30 seconds,
ESP32 creates access point `BCM-AP` (IP `192.168.4.1`).
Open `http://192.168.4.1` for web configuration.

### Web configuration UI

Available in both AP and STA mode on port 80.

- **Login:** `admin`
- **Password:** `WEB_PASSWORD` value from `secrets.h`
- Basic HTTP Authentication
- XSS protection (HTML escaping)

Configurable parameters: SSID, Password, Server IP, Device ID, API Token,
Web Password, Critical Voltage, Info/Critical Intervals.

After saving, the device restarts with new configuration.
Settings are stored in NVS (Preferences) and survive reboots.

### Button

| Action | Behavior |
|---|---|
| Press | Activates voltmeter for 20 seconds |
| Hold 10 s | Factory reset — reverts to `secrets.h` defaults |

### Watchdog Timer

60-second task watchdog. Automatic reboot on hang.

### HTTP communication

ESP32 sends JSON via HTTP POST to the server:

```json
{"device_id": "BATT#1", "msg_type": "INFO", "voltage": 12.4}
```

```json
{"device_id": "BATT#1", "msg_type": "ALERT", "voltage": 10.8, "critical_voltage": 11.0}
```

INFO is sent when voltage is above critical threshold.
ALERT is sent when voltage is at or below critical threshold.

- Bearer token authentication
- 3 retry attempts with 1 s delay between retries

---

## Server

### Requirements

Any Linux VPS with a public IP. Recommendations:

| Provider | Tier | Spec | Price |
|---|---|---|---|
| Oracle Cloud | Always Free | 4 vCPU / 24 GB RAM / 200 GB | Free |
| Hetzner | CX22 | 2 vCPU / 4 GB RAM | ~4 EUR/mo |
| DigitalOcean | Basic | 1 vCPU / 1 GB RAM | ~6 USD/mo |

Minimum config for this project: 1 vCPU / 512 MB RAM.

### System packages

```bash
sudo apt update && sudo apt install -y python3 python3-pip python3-venv openjdk-17-jre nginx
```

### signal-cli setup

```bash
SIGNAL_CLI_VERSION="0.13.5"
wget "https://github.com/AsamK/signal-cli/releases/download/v${SIGNAL_CLI_VERSION}/signal-cli-${SIGNAL_CLI_VERSION}-Linux.tar.gz"
tar xf signal-cli-*.tar.gz
sudo mv signal-cli-* /opt/signal-cli
sudo ln -s /opt/signal-cli/bin/signal-cli /usr/local/bin/signal-cli
```

Register a phone number:

```bash
signal-cli -u +380XXXXXXXXX register
signal-cli -u +380XXXXXXXXX verify CODE_FROM_SMS
```

Create a Signal group and add the number:

```bash
signal-cli -u +380XXXXXXXXX updateGroup -n "Battery Monitor" -m +380XXXXXXXXX
```

### Backend setup

```bash
git clone https://github.com/bogdan-kovalchuk/esp32-battery-charging-monitoring.git
cd esp32-battery-charging-monitoring
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

Create `.env` in project root:

```
SIGNAL_GROUP_ID=your-signal-group-id
SIGNAL_USER=+380XXXXXXXXX
API_TOKEN=same-token-as-in-secrets-h
FLASK_HOST=0.0.0.0
FLASK_PORT=5000
```

> Get `SIGNAL_GROUP_ID`:
> `signal-cli -u +380XXXXXXXXX listGroups`

### Systemd service

```bash
sudo nano /etc/systemd/system/battery-monitor.service
```

```ini
[Unit]
Description=Battery Monitor Backend
After=network.target

[Service]
Type=simple
WorkingDirectory=/opt/esp32-battery-charging-monitoring
ExecStart=/opt/esp32-battery-charging-monitoring/venv/bin/python src/app.py
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now battery-monitor
```

### Nginx reverse proxy

```bash
sudo nano /etc/nginx/sites-available/battery-monitor
```

```nginx
server {
    listen 80;
    server_name battery.yourdomain.com;

    location / {
        proxy_pass http://127.0.0.1:5000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

```bash
sudo ln -s /etc/nginx/sites-available/battery-monitor /etc/nginx/sites-enabled/
sudo nginx -t && sudo systemctl reload nginx
```

If using Nginx, set hostname in `secrets.h` instead of IP:

```c
#define SERVER_IP "battery.yourdomain.com"
```

### Firewall

```bash
sudo ufw allow 80/tcp
sudo ufw allow 443/tcp
sudo ufw enable
```

---

## API

| Endpoint | Method | Auth | Description |
|---|---|---|---|
| `/send` | POST | `Bearer <token>` | Accepts data from ESP32 |
| `/signal-cli/version` | GET | — | signal-cli version info |
| `/metrics` | GET | — | Prometheus-style metrics |
| `/healthcheck` | GET | — | Server health status |

### POST /send

```json
{
  "device_id": "BATT#1",
  "msg_type": "INFO",
  "voltage": 12.4
}
```

```json
{
  "device_id": "BATT#1",
  "msg_type": "ALERT",
  "voltage": 10.8,
  "critical_voltage": 11.0
}
```

---

## Testing

```powershell
# Backend
pip install pytest
pytest test/test_app.py

# Firmware
pio test -e native
```

## License

[MIT](LICENSE)
