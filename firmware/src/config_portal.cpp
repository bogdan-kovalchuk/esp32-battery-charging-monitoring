#include "config_portal.h"
#include <WebServer.h>
#include <esp_system.h>

static WebServer server(80);
static DeviceConfig *pCfg = nullptr;

// Regenerated on every boot. The config form is protected by HTTP Basic Auth,
// which browsers replay automatically on cross-site requests, so the token is
// what actually stops another page on the network from silently reconfiguring
// the device.
static String csrfToken;

static String escapeHtml(const String &s) {
  String r = s;
  r.replace("&", "&amp;");
  r.replace("<", "&lt;");
  r.replace(">", "&gt;");
  r.replace("\"", "&quot;");
  r.replace("'", "&#39;");
  return r;
}

static String randomHex(int bytes) {
  String out;
  out.reserve(bytes * 2);
  for (int i = 0; i < bytes; i++) {
    char buf[3];
    snprintf(buf, sizeof(buf), "%02x", (unsigned)(esp_random() & 0xFF));
    out += buf;
  }
  return out;
}

// Length-independent comparison, so a wrong token cannot be recovered by
// timing the response.
static bool secureEquals(const String &a, const String &b) {
  if (a.length() != b.length()) return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < a.length(); i++) diff |= (uint8_t)(a[i] ^ b[i]);
  return diff == 0;
}

static bool checkAuth() {
  if (!server.authenticate(pCfg->webUser.c_str(), pCfg->webPassword.c_str())) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

static void sendPage(int code, const String &bodyHtml) {
  String html =
      "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Battery Monitor</title></head><body>";
  html += bodyHtml;
  html += "</body></html>";
  // The page embeds credentials and a CSRF token; keep it out of caches.
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.send(code, "text/html", html);
}

static void handleRoot() {
  if (!checkAuth()) return;

  String f = "<h1>Battery Monitor</h1><form action='/save' method='post'>";
  f += "<input type='hidden' name='csrf' value='" + csrfToken + "'>";

  f += "<h2>WiFi</h2>";
  f += "SSID: <input type='text' name='ssid' value='" +
       escapeHtml(pCfg->ssid) + "' maxlength='32' required><br>";
  f += "Password: <input type='password' name='password' "
       "placeholder='leave blank to keep current' maxlength='63'><br>";

  f += "<h2>Backend</h2>";
  f += "Server host: <input type='text' name='serverHost' value='" +
       escapeHtml(pCfg->serverHost) + "' maxlength='64' required><br>";
  f += "Server port: <input type='number' name='serverPort' value='" +
       String(pCfg->serverPort) + "' min='1' max='65535' required><br>";
  f += "TLS: <input type='checkbox' name='serverTls' value='1'";
  if (pCfg->serverTls) f += " checked";
  f += "> (requires the compiled CA root)<br>";
  f += "Device ID: <input type='text' name='deviceId' value='" +
       escapeHtml(pCfg->deviceId) + "' maxlength='32' required><br>";
  // The current token is never rendered back: anyone who can view the page can
  // already change it, but not echoing it keeps it out of screenshots, browser
  // caches and shoulder-surfing range.
  f += "API token: <input type='password' name='apiToken' "
       "placeholder='leave blank to keep current' minlength='" +
       String(CFG_API_TOKEN_MIN_LEN) + "' maxlength='" +
       String(CFG_TEXT_MAX_LEN) + "'><br>";

  f += "<h2>Fallback access point</h2>";
  f += "AP SSID: <input type='text' name='apSsid' value='" +
       escapeHtml(pCfg->apSsid) + "' maxlength='32' required><br>";
  f += "AP password: <input type='password' name='apPassword' "
       "placeholder='leave blank to keep current' minlength='8' "
       "maxlength='63'><br>";

  f += "<h2>Web UI</h2>";
  f += "Username: <input type='text' name='webUser' value='" +
       escapeHtml(pCfg->webUser) + "' maxlength='32' required><br>";
  f += "Password: <input type='password' name='webPassword' "
       "placeholder='leave blank to keep current' minlength='" +
       String(CFG_WEB_PASSWORD_MIN_LEN) + "' maxlength='" +
       String(CFG_TEXT_MAX_LEN) + "'><br>";

  f += "<h2>Measurement</h2>";
  f += "Critical voltage (V): <input type='number' name='critVoltage' value='" +
       String(pCfg->critVoltage, 1) + "' step='0.1' min='" +
       String(CFG_CRIT_VOLTAGE_MIN, 1) + "' max='" +
       String(CFG_CRIT_VOLTAGE_MAX, 1) + "' required><br>";
  f += "Info interval (min): <input type='number' name='infoInterval' value='" +
       String(pCfg->infoInterval) + "' min='" + String(CFG_INTERVAL_MIN) +
       "' max='" + String(CFG_INTERVAL_MAX) + "' required><br>";
  f += "Critical interval (min): <input type='number' name='critInterval' "
       "value='" +
       String(pCfg->critInterval) + "' min='" + String(CFG_INTERVAL_MIN) +
       "' max='" + String(CFG_INTERVAL_MAX) + "' required><br>";

  f += "<p><input type='submit' value='Save and restart'></p></form>";
  sendPage(200, f);
}

static void handleSave() {
  if (!checkAuth()) return;

  if (!secureEquals(server.arg("csrf"), csrfToken)) {
    sendPage(403, "<h1>Rejected</h1><p>Invalid or stale form token. Reload "
                  "the configuration page and try again.</p>");
    return;
  }

  // Edit a copy, so a rejected submission cannot leave the live config half
  // updated.
  DeviceConfig next = *pCfg;

  next.ssid = server.arg("ssid");
  next.serverHost = server.arg("serverHost");
  next.serverTls = server.hasArg("serverTls");
  next.deviceId = server.arg("deviceId");
  next.apSsid = server.arg("apSsid");
  next.webUser = server.arg("webUser");

  // Blank secret fields mean "keep the current value" — that is the only way
  // to submit the form without re-typing every credential.
  if (server.arg("password").length() > 0) next.password = server.arg("password");
  if (server.arg("apiToken").length() > 0) next.apiToken = server.arg("apiToken");
  if (server.arg("apPassword").length() > 0)
    next.apPassword = server.arg("apPassword");
  if (server.arg("webPassword").length() > 0)
    next.webPassword = server.arg("webPassword");

  long port = server.arg("serverPort").toInt();
  next.serverPort = (port >= 1 && port <= 65535) ? (uint16_t)port : 0;
  next.critVoltage = server.arg("critVoltage").toFloat();
  next.infoInterval = (unsigned long)server.arg("infoInterval").toInt();
  next.critInterval = (unsigned long)server.arg("critInterval").toInt();

  // Reject rather than silently correct: a browser that bypassed the HTML
  // constraints is submitting something the operator did not intend.
  DeviceConfig checked = next;
  if (!configClamp(checked)) {
    sendPage(400,
             "<h1>Rejected</h1><p>One or more values are out of range:</p><ul>"
             "<li>Critical voltage " +
                 String(CFG_CRIT_VOLTAGE_MIN, 1) + "&ndash;" +
                 String(CFG_CRIT_VOLTAGE_MAX, 1) + " V</li>"
                 "<li>Intervals " + String(CFG_INTERVAL_MIN) + "&ndash;" +
                 String(CFG_INTERVAL_MAX) + " minutes</li>"
                 "<li>SSIDs 1&ndash;" + String(CFG_SSID_MAX_LEN) +
                 " characters</li>"
                 "<li>AP password " + String(CFG_AP_PASSWORD_MIN_LEN) +
                 "&ndash;" + String(CFG_WIFI_PASSWORD_MAX_LEN) +
                 " characters</li>"
                 "<li>Web password at least " +
                 String(CFG_WEB_PASSWORD_MIN_LEN) + " characters</li>"
                 "<li>API token at least " + String(CFG_API_TOKEN_MIN_LEN) +
                 " characters</li>"
                 "<li>No credential may be one of the defaults published in "
                 "the repository</li>"
                 "</ul><p><a href='/'>Back</a></p>");
    return;
  }

  if (!configSave(next)) {
    sendPage(500, "<h1>Save failed</h1><p>The previous configuration is still "
                  "active. Check the serial log and NVS health, then retry.</p>");
    return;
  }
  *pCfg = next;
  sendPage(200, "<h1>Saved</h1><p>Restarting with the new configuration…</p>");
  delay(2000);
  ESP.restart();
}

void webserverInit(DeviceConfig &cfg) {
  pCfg = &cfg;
  csrfToken = randomHex(16);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

void webserverHandle() {
  server.handleClient();
}
