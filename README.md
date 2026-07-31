# ESP32 Battery Charging Monitor

An ESP32-based 12 V battery voltage monitor. The firmware measures battery
voltage, while a Flask backend delivers notifications to a Signal group through
`signal-cli`.

```text
ESP32 -- HTTP or HTTPS --> Flask -- SQLite outbox --> signal-cli --> Signal group
```

## Features

- Voltage sampling every 60 seconds
- Immediate `ALERT` when voltage crosses below the critical threshold
- Immediate `INFO` when voltage recovers
- Separate reminder intervals for INFO and ALERT states
- Fallback WiFi access point with a web configuration form
- CRC-protected ESP32 configuration stored in alternating NVS slots
- Persistent SQLite outbox with retries and `event_id` deduplication
- Optional HTTP or HTTPS transport
- Health check and Prometheus metrics

## Project structure

| Path | Purpose |
|---|---|
| `firmware/` | PlatformIO/Arduino firmware for ESP32 |
| `server/` | Flask API and signal-cli integration |
| `firmware/test/` | Host-side firmware logic tests |
| `server/tests/` | Backend tests |

## Hardware

| Signal | ESP32 pin | Notes |
|---|---:|---|
| Voltage divider | GPIO 32 | ADC input, 30 kOhm / 7.5 kOhm |
| Button | GPIO 34 | Requires an external 10 kOhm pull-down |
| Divider/gauge enable | GPIO 4 | Through a transistor and 1 kOhm resistor |

The ADC calibration factor is `CORR_FACTOR` in
`firmware/include/app_config.h`.

## Firmware

Create the configuration file:

```bash
cp firmware/include/secrets.h.example firmware/include/secrets.h
```

Set the WiFi credentials, backend host, API token, and passwords. `secrets.h`
is ignored by Git. Its values provide the initial defaults; after the first
boot, runtime configuration is stored in NVS.

Build and upload:

```bash
pio run -d firmware -e esp32dev
pio run -d firmware -e esp32dev -t upload
pio device monitor -b 115200
```

### HTTP and HTTPS

The configuration template uses HTTP by default:

```cpp
#define SERVER_PORT 5000
#define SERVER_USE_TLS 0
#define SERVER_ROOT_CA ""
```

Existing private `secrets.h` files without the TLS macros also continue to use
HTTP.

To enable HTTPS, select the TLS port and provide the PEM CA root:

```cpp
#define SERVER_PORT 443
#define SERVER_USE_TLS 1
#define SERVER_ROOT_CA \
  "-----BEGIN CERTIFICATE-----\n" \
  "...\n" \
  "-----END CERTIFICATE-----\n"
```

The CA root is required only when HTTPS is enabled.

### Fallback access point

If the ESP32 cannot connect to WiFi within 30 seconds, it starts the configured
fallback access point while continuing STA connection attempts in the
background.

1. Connect to the network configured by `AP_SSID`.
2. Open `http://192.168.4.1/`.
3. Sign in with `WEB_USER` and `WEB_PASSWORD`.
4. Update the WiFi, backend, threshold, and interval settings.
5. Save the configuration.

The fallback access point stops after the STA connection remains stable.
Holding the button for 10 seconds restores the compile-time defaults.

## Backend

The backend requires Python 3.10+ and an installed `signal-cli`. Follow the
official [signal-cli README](https://github.com/AsamK/signal-cli) for current
installation and account setup instructions.

```bash
cd server
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env
```

Set at least these values:

```dotenv
SIGNAL_USER=+380XXXXXXXXX
SIGNAL_GROUP_ID=group-id
API_TOKEN=same-token-as-in-firmware
```

Start the backend:

```bash
python -m battery_monitor.wsgi
```

Before opening the socket, the backend verifies the signal-cli binary, account,
and target group. Accepted events are persisted in
`server/.data/outbox.sqlite3` and then sent sequentially through signal-cli.

## API

`POST /send` requires `Authorization: Bearer <API_TOKEN>`.

```json
{
  "event_id": "device-boot-sequence",
  "device_id": "BATT#1",
  "msg_type": "ALERT",
  "voltage": 10.8,
  "critical_voltage": 11.0
}
```

| Status | Meaning |
|---:|---|
| `202` | Event persisted or previously accepted |
| `400` | Invalid payload |
| `401` | Invalid API token |
| `413` | Request body too large |
| `503` | Outbox full; retry with the same `event_id` |

Additional endpoints:

- `GET /healthcheck`
- `GET /metrics`
- `GET /signal-cli/version`

## Tests

```bash
pio test -d firmware -e native
pio run -d firmware -e esp32dev-ci

cd server
python -m pytest
```

`esp32dev-ci` uses dummy credentials for compile checks only. Upload firmware
with the `esp32dev` environment and your own `secrets.h`.

## Limitations

- HTTP does not encrypt the API token; enable HTTPS on untrusted networks.
- The ESP32 configuration portal uses HTTP.
- ESP32 NVS is not encrypted.
- Devices using the same `API_TOKEN` share access. Use individual tokens when
  separate device identities are required.

## License

[MIT](LICENSE)
