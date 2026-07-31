#include "device_config.h"
#include "tls_config.h"
#include <ArduinoJson.h>
#include <Preferences.h>

// ============================================================================
// Compile-time credential guard
// ============================================================================
//
// secrets.h ships as a template full of placeholders. Building with those
// placeholders left in place would produce a device whose API token and web
// password are printed in a public repository, so refuse to compile instead.
// The historical weak defaults are rejected too, so an old secrets.h carried
// over from a previous checkout cannot slip through either.

static constexpr bool cfgStrEq(const char *a, const char *b) {
  return (*a == *b) && (*a == '\0' || cfgStrEq(a + 1, b + 1));
}

#define CFG_ASSERT_SET(macro)                                                  \
  static_assert(!cfgStrEq(macro, "CHANGE_ME"),                                 \
                "secrets.h: " #macro " is still the placeholder. Copy "        \
                "firmware/include/secrets.h.example to "                       \
                "firmware/include/secrets.h and fill in a real value.")

CFG_ASSERT_SET(WIFI_SSID);
CFG_ASSERT_SET(WIFI_PASSWORD);
CFG_ASSERT_SET(SERVER_HOST);
CFG_ASSERT_SET(AP_PASSWORD);
CFG_ASSERT_SET(API_TOKEN);
CFG_ASSERT_SET(WEB_PASSWORD);

static_assert(!cfgStrEq(WIFI_SSID, "YourWiFiSSID"),
              "secrets.h: WIFI_SSID is still the old example value.");
static_assert(!cfgStrEq(WIFI_PASSWORD, "YourWiFiPassword"),
              "secrets.h: WIFI_PASSWORD is still the old example value.");
static_assert(!cfgStrEq(API_TOKEN, "change-me-to-random-string"),
              "secrets.h: API_TOKEN is the old default, which is published in "
              "the repository. Generate a new one.");
static_assert(!cfgStrEq(WEB_PASSWORD, "admin"),
              "secrets.h: WEB_PASSWORD is the old default.");
static_assert(!cfgStrEq(AP_PASSWORD, "12345678"),
              "secrets.h: AP_PASSWORD is the old default.");

static_assert(sizeof(AP_PASSWORD) - 1 >= CFG_AP_PASSWORD_MIN_LEN,
              "secrets.h: AP_PASSWORD must be at least 8 characters (WPA2).");
static_assert(sizeof(AP_PASSWORD) - 1 <= CFG_WIFI_PASSWORD_MAX_LEN,
              "secrets.h: AP_PASSWORD must be at most 63 characters.");
static_assert(sizeof(API_TOKEN) - 1 >= CFG_API_TOKEN_MIN_LEN,
              "secrets.h: API_TOKEN must be at least 16 characters.");
static_assert(sizeof(WEB_PASSWORD) - 1 >= CFG_WEB_PASSWORD_MIN_LEN,
              "secrets.h: WEB_PASSWORD must be at least 8 characters.");
static_assert(SERVER_PORT > 0 && SERVER_PORT <= 65535,
              "secrets.h: SERVER_PORT must be in 1..65535.");

// The fallbacks configClamp() reaches for must themselves be in range, or a
// device whose stored value is invalid would be "corrected" to another invalid
// value and the config form would reject its own prefilled contents forever.
static_assert(CRIT_VOLTAGE >= CFG_CRIT_VOLTAGE_MIN &&
                  CRIT_VOLTAGE <= CFG_CRIT_VOLTAGE_MAX,
              "secrets.h: CRIT_VOLTAGE is outside the accepted 5.0-15.0 V.");
static_assert(INFO_INTERVAL >= CFG_INTERVAL_MIN &&
                  INFO_INTERVAL <= CFG_INTERVAL_MAX,
              "secrets.h: INFO_INTERVAL must be 1..10080 minutes.");
static_assert(CRIT_INTERVAL >= CFG_INTERVAL_MIN &&
                  CRIT_INTERVAL <= CFG_INTERVAL_MAX,
              "secrets.h: CRIT_INTERVAL must be 1..10080 minutes.");
static_assert(sizeof(WIFI_SSID) - 1 >= 1 &&
                  sizeof(WIFI_SSID) - 1 <= CFG_SSID_MAX_LEN,
              "secrets.h: WIFI_SSID must be 1..32 characters.");
static_assert(sizeof(AP_SSID) - 1 >= 1 &&
                  sizeof(AP_SSID) - 1 <= CFG_SSID_MAX_LEN,
              "secrets.h: AP_SSID must be 1..32 characters.");
static_assert(sizeof(WIFI_PASSWORD) - 1 <= CFG_WIFI_PASSWORD_MAX_LEN,
              "secrets.h: WIFI_PASSWORD must be at most 63 characters.");
static_assert(sizeof(SERVER_HOST) - 1 >= 1,
              "secrets.h: SERVER_HOST must not be empty.");
static_assert(sizeof(DEVICE_ID) - 1 >= 1, "secrets.h: DEVICE_ID is empty.");
static_assert(sizeof(WEB_USER) - 1 >= 1, "secrets.h: WEB_USER is empty.");

// ============================================================================
// NVS persistence
// ============================================================================

static Preferences prefs;
static const char *NS = "config";
static bool nvsReady = false;

// Bumped whenever the stored key set changes, so devices flashed with an older
// layout are migrated instead of silently falling back to compile-time
// defaults.
//   0 -> unversioned original layout ("serverIP", no port / AP / web user)
//   1 -> scalar keys
//   2 -> CRC-protected, double-buffered records
static const uint32_t CFG_VERSION = 2;
static const char *SLOT_KEYS[] = {"slotA", "slotB"};
static const char *ACTIVE_KEY = "active";

void configInit() {
  // A failed begin() makes every getter return its default and every setter a
  // silent no-op, so the operator would see "Saved" while nothing persisted.
  nvsReady = prefs.begin(NS, false);
  if (!nvsReady) {
    Serial.println("FATAL: could not open NVS namespace 'config'. "
                   "Configuration changes will NOT persist.");
  }
}

static uint32_t crc32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++)
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

static String encodeRecord(const DeviceConfig &cfg, uint32_t generation) {
  JsonDocument payload;
  payload["v"] = CFG_VERSION;
  payload["g"] = generation;
  payload["ssid"] = cfg.ssid;
  payload["password"] = cfg.password;
  payload["host"] = cfg.serverHost;
  payload["port"] = cfg.serverPort;
  payload["tls"] = cfg.serverTls;
  payload["device"] = cfg.deviceId;
  payload["token"] = cfg.apiToken;
  payload["apSsid"] = cfg.apSsid;
  payload["apPassword"] = cfg.apPassword;
  payload["webUser"] = cfg.webUser;
  payload["webPassword"] = cfg.webPassword;
  payload["critical"] = cfg.critVoltage;
  payload["info"] = cfg.infoInterval;
  payload["alert"] = cfg.critInterval;

  String payloadText;
  serializeJson(payload, payloadText);

  JsonDocument envelope;
  envelope["crc"] = crc32(reinterpret_cast<const uint8_t *>(payloadText.c_str()),
                           payloadText.length());
  envelope["payload"] = payloadText;
  String record;
  serializeJson(envelope, record);
  return record;
}

static bool decodeRecord(const String &record, DeviceConfig &cfg,
                         uint32_t &generation) {
  JsonDocument envelope;
  if (deserializeJson(envelope, record) != DeserializationError::Ok) return false;
  if (!envelope["payload"].is<const char *>() || !envelope["crc"].is<uint32_t>())
    return false;

  String payloadText = envelope["payload"].as<String>();
  uint32_t expected = envelope["crc"].as<uint32_t>();
  uint32_t actual =
      crc32(reinterpret_cast<const uint8_t *>(payloadText.c_str()),
            payloadText.length());
  if (actual != expected) return false;

  JsonDocument payload;
  if (deserializeJson(payload, payloadText) != DeserializationError::Ok)
    return false;
  if (payload["v"].as<uint32_t>() != CFG_VERSION) return false;

  const char *required[] = {"g",          "ssid",        "password",
                            "host",       "port",        "tls",
                            "device",     "token",       "apSsid",
                            "apPassword", "webUser",     "webPassword",
                            "critical",   "info",        "alert"};
  for (const char *key : required)
    if (payload[key].isNull()) return false;

  generation = payload["g"].as<uint32_t>();
  cfg.ssid = payload["ssid"].as<String>();
  cfg.password = payload["password"].as<String>();
  cfg.serverHost = payload["host"].as<String>();
  cfg.serverPort = payload["port"].as<uint16_t>();
  cfg.serverTls = payload["tls"].as<bool>();
  cfg.deviceId = payload["device"].as<String>();
  cfg.apiToken = payload["token"].as<String>();
  cfg.apSsid = payload["apSsid"].as<String>();
  cfg.apPassword = payload["apPassword"].as<String>();
  cfg.webUser = payload["webUser"].as<String>();
  cfg.webPassword = payload["webPassword"].as<String>();
  cfg.critVoltage = payload["critical"].as<float>();
  cfg.infoInterval = payload["info"].as<unsigned long>();
  cfg.critInterval = payload["alert"].as<unsigned long>();
  return true;
}

bool configIsPublishedDefault(const String &value) {
  // These strings were committed to this repository, so they are public
  // knowledge. A device flashed before the credential guard existed still has
  // them sitting in NVS, where no compile-time check can reach them.
  return value == "change-me-to-random-string" || value == "admin" ||
         value == "12345678" || value == "YourWiFiSSID" ||
         value == "YourWiFiPassword" || value == "CHANGE_ME";
}

void configLoad(DeviceConfig &cfg) {
  if (!nvsReady) {
    configReset(cfg);
    configClamp(cfg);
    return;
  }

  uint8_t active = prefs.getUChar(ACTIVE_KEY, 0xFF);
  if (active <= 1) {
    uint32_t ignoredGeneration = 0;
    if (decodeRecord(prefs.getString(SLOT_KEYS[active], ""), cfg,
                     ignoredGeneration) &&
        configClamp(cfg)) {
      return;
    }

    // The active record is corrupt or semantically invalid. Fall back only to
    // the previously committed slot; never promote an uncommitted newer slot.
    uint8_t fallback = 1 - active;
    if (decodeRecord(prefs.getString(SLOT_KEYS[fallback], ""), cfg,
                     ignoredGeneration) &&
        configClamp(cfg)) {
      Serial.println("Active config record invalid; using previous slot");
      if (prefs.putUChar(ACTIVE_KEY, fallback) != 1)
        Serial.println("Could not repair active config marker");
      return;
    }
  }

  // Migrate the historical scalar layout without deleting it first. It
  // remains a recovery source until the new record and marker both commit.
  // "ssid" is written by every layout, so its absence means the device has
  // never been provisioned.
  if (!prefs.isKey("ssid")) {
    configReset(cfg);
    configClamp(cfg);
    configSave(cfg);
    return;
  }

  uint32_t storedVersion = prefs.getUInt("cfgVer", 0);

  cfg.ssid = prefs.getString("ssid", WIFI_SSID);
  cfg.password = prefs.getString("password", WIFI_PASSWORD);
  cfg.deviceId = prefs.getString("deviceId", DEVICE_ID);
  cfg.apiToken = prefs.getString("apiToken", API_TOKEN);
  cfg.webPassword = prefs.getString("webPassword", WEB_PASSWORD);
  cfg.critVoltage = prefs.getFloat("critVoltage", CRIT_VOLTAGE);
  cfg.infoInterval = prefs.getULong("infoInterval", INFO_INTERVAL);
  cfg.critInterval = prefs.getULong("critInterval", CRIT_INTERVAL);

  // Fields added in layout 1. On a layout 0 device these keys are absent, so
  // the compile-time defaults apply — except for the host, which was stored
  // under the old "serverIP" name.
  cfg.serverHost = (storedVersion == 0)
                       ? prefs.getString("serverIP", SERVER_HOST)
                       : prefs.getString("serverHost", SERVER_HOST);
  cfg.serverPort = prefs.getUShort("serverPort", SERVER_PORT);
  cfg.serverTls = SERVER_USE_TLS;
  cfg.apSsid = prefs.getString("apSsid", AP_SSID);
  cfg.apPassword = prefs.getString("apPassword", AP_PASSWORD);
  cfg.webUser = prefs.getString("webUser", WEB_USER);

  bool clean = configClamp(cfg);

  Serial.printf("Migrating config (layout %u -> %u, corrected=%d)\n",
                (unsigned)storedVersion, (unsigned)CFG_VERSION, !clean);
  if (!configSave(cfg))
    Serial.println("Config migration failed; legacy keys remain available");
}

bool configSave(const DeviceConfig &cfg) {
  if (!nvsReady) return false;

  uint8_t active = prefs.getUChar(ACTIVE_KEY, 0xFF);
  uint8_t target = active <= 1 ? 1 - active : 0;
  uint32_t generation = 0;
  DeviceConfig ignored;
  if (active <= 1)
    decodeRecord(prefs.getString(SLOT_KEYS[active], ""), ignored, generation);

  String record = encodeRecord(cfg, generation + 1);
  size_t written = prefs.putString(SLOT_KEYS[target], record);
  if (written == 0 || prefs.getString(SLOT_KEYS[target], "") != record) {
    Serial.println("Config record write/verification failed");
    return false;
  }

  DeviceConfig verified;
  uint32_t verifiedGeneration = 0;
  if (!decodeRecord(record, verified, verifiedGeneration) ||
      verifiedGeneration != generation + 1) {
    Serial.println("Config record CRC verification failed");
    return false;
  }

  // This single marker write is the commit point. Until it succeeds, boot
  // keeps using the previous complete slot.
  if (prefs.putUChar(ACTIVE_KEY, target) != 1) {
    Serial.println("Config commit marker write failed");
    return false;
  }
  return true;
}

void configReset(DeviceConfig &cfg) {
  cfg.ssid = WIFI_SSID;
  cfg.password = WIFI_PASSWORD;
  cfg.serverHost = SERVER_HOST;
  cfg.serverPort = SERVER_PORT;
  cfg.serverTls = SERVER_USE_TLS;
  cfg.deviceId = DEVICE_ID;
  cfg.apiToken = API_TOKEN;
  cfg.apSsid = AP_SSID;
  cfg.apPassword = AP_PASSWORD;
  cfg.webUser = WEB_USER;
  cfg.webPassword = WEB_PASSWORD;
  cfg.critVoltage = CRIT_VOLTAGE;
  cfg.infoInterval = INFO_INTERVAL;
  cfg.critInterval = CRIT_INTERVAL;
}

// Replaces `field` with `fallback` unless its length is within [minLen, maxLen].
static bool clampText(String &field, const char *fallback, unsigned int minLen,
                      unsigned int maxLen) {
  if (field.length() >= minLen && field.length() <= maxLen) return true;
  field = fallback;
  return false;
}

bool configClamp(DeviceConfig &cfg) {
  bool ok = true;

  // Written as a positive range test so NaN is rejected too: a NaN threshold
  // makes every voltage comparison false and silently stops all alerting.
  if (!(cfg.critVoltage >= CFG_CRIT_VOLTAGE_MIN &&
        cfg.critVoltage <= CFG_CRIT_VOLTAGE_MAX)) {
    cfg.critVoltage = CRIT_VOLTAGE;
    ok = false;
  }
  if (cfg.infoInterval < CFG_INTERVAL_MIN ||
      cfg.infoInterval > CFG_INTERVAL_MAX) {
    cfg.infoInterval = INFO_INTERVAL;
    ok = false;
  }
  if (cfg.critInterval < CFG_INTERVAL_MIN ||
      cfg.critInterval > CFG_INTERVAL_MAX) {
    cfg.critInterval = CRIT_INTERVAL;
    ok = false;
  }
  if (cfg.serverPort == 0) {
    cfg.serverPort = SERVER_PORT;
    ok = false;
  }

  // Upper bounds are enforced here, not just in the HTML: an SSID longer than
  // 32 bytes makes softAP() fail outright, and a WPA2 passphrase longer than
  // 63 is silently truncated to something the operator cannot type back in.
  ok &= clampText(cfg.ssid, WIFI_SSID, 1, CFG_SSID_MAX_LEN);
  ok &= clampText(cfg.password, WIFI_PASSWORD, 0, CFG_WIFI_PASSWORD_MAX_LEN);
  ok &= clampText(cfg.apSsid, AP_SSID, 1, CFG_SSID_MAX_LEN);
  ok &= clampText(cfg.apPassword, AP_PASSWORD, CFG_AP_PASSWORD_MIN_LEN,
                  CFG_WIFI_PASSWORD_MAX_LEN);
  ok &= clampText(cfg.serverHost, SERVER_HOST, 1, CFG_TEXT_MAX_LEN);
  ok &= clampText(cfg.deviceId, DEVICE_ID, 1, CFG_TEXT_MAX_LEN);
  ok &= clampText(cfg.webUser, WEB_USER, 1, CFG_SSID_MAX_LEN);
  // The runtime minimums match the static_asserts, so the web UI cannot set a
  // credential the build itself would have rejected.
  ok &= clampText(cfg.webPassword, WEB_PASSWORD, CFG_WEB_PASSWORD_MIN_LEN,
                  CFG_TEXT_MAX_LEN);
  ok &= clampText(cfg.apiToken, API_TOKEN, CFG_API_TOKEN_MIN_LEN,
                  CFG_TEXT_MAX_LEN);

  // Last line of defence for a device provisioned before the compile-time
  // guard existed: those values live in NVS, where no static_assert can reach
  // them, and they are published in this repository's git history.
  if (configIsPublishedDefault(cfg.apiToken)) {
    cfg.apiToken = API_TOKEN;
    ok = false;
  }
  if (configIsPublishedDefault(cfg.webPassword)) {
    cfg.webPassword = WEB_PASSWORD;
    ok = false;
  }
  if (configIsPublishedDefault(cfg.apPassword)) {
    cfg.apPassword = AP_PASSWORD;
    ok = false;
  }
  if (configIsPublishedDefault(cfg.password)) {
    cfg.password = WIFI_PASSWORD;
    ok = false;
  }

  return ok;
}
