#pragma once

// Compile-only values for env:esp32dev-ci. They are deliberately unusable on
// a device; this environment lets CI compile every firmware translation unit
// without access to a developer's ignored secrets.h.
#define WIFI_SSID "ci-network"
#define WIFI_PASSWORD "ci-network-password"
#define SERVER_HOST "ci.invalid"
#define SERVER_PORT 443
#define SERVER_USE_TLS 1
#define SERVER_ROOT_CA                                                       \
  "-----BEGIN CERTIFICATE-----\n"                                            \
  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"       \
  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"       \
  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"       \
  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"       \
  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"       \
  "-----END CERTIFICATE-----\n"
#define AP_SSID "BCM-CI-AP"
#define AP_PASSWORD "ci-access-point-password"
#define DEVICE_ID "CI-DEVICE"
#define API_TOKEN "ci-token-not-for-device-use"
#define WEB_USER "ci-admin"
#define WEB_PASSWORD "ci-web-password"
#define CRIT_VOLTAGE 11.0f
#define INFO_INTERVAL 300
#define CRIT_INTERVAL 120
