#include "webserver.h"
#include <WebServer.h>

static WebServer server(80);
static DeviceConfig *pCfg;

static String escapeHtml(const String &s) {
  String r = s;
  r.replace("&", "&amp;");
  r.replace("<", "&lt;");
  r.replace(">", "&gt;");
  r.replace("\"", "&quot;");
  r.replace("'", "&#39;");
  return r;
}

static bool checkAuth() {
  if (!server.authenticate("admin", pCfg->webPassword.c_str())) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

static void handleRoot() {
  if (!checkAuth()) return;
  String html = "<html><head><meta charset='UTF-8'></head><body><h1>Battery Monitor</h1>";
  html += "<form action='/save' method='post'>";
  html += "SSID: <input type='text' name='ssid' value='" + escapeHtml(pCfg->ssid) + "'><br>";
  html += "Password: <input type='password' name='password' placeholder='unchanged'><br>";
  html += "Server IP: <input type='text' name='serverIP' value='" + escapeHtml(pCfg->serverIP) + "'><br>";
  html += "Device ID: <input type='text' name='deviceId' value='" + escapeHtml(pCfg->deviceId) + "'><br>";
  html += "API Token: <input type='text' name='apiToken' value='" + escapeHtml(pCfg->apiToken) + "'><br>";
  html += "Web Password: <input type='password' name='webPassword' placeholder='unchanged'><br>";
  html += "Critical Voltage: <input type='number' name='critVoltage' value='" + String(pCfg->critVoltage) + "' step='0.1'><br>";
  html += "Info Interval (min): <input type='number' name='infoInterval' value='" + String(pCfg->infoInterval) + "'><br>";
  html += "Critical Interval (min): <input type='number' name='critInterval' value='" + String(pCfg->critInterval) + "'><br>";
  html += "<input type='submit' value='Save'></form></body></html>";
  server.send(200, "text/html", html);
}

static void handleSave() {
  if (!checkAuth()) return;
  pCfg->ssid = server.arg("ssid");
  if (server.arg("password").length() > 0) pCfg->password = server.arg("password");
  pCfg->serverIP = server.arg("serverIP");
  pCfg->deviceId = server.arg("deviceId");
  pCfg->apiToken = server.arg("apiToken");
  if (server.arg("webPassword").length() > 0) pCfg->webPassword = server.arg("webPassword");
  pCfg->critVoltage = server.arg("critVoltage").toFloat();
  pCfg->infoInterval = server.arg("infoInterval").toInt();
  pCfg->critInterval = server.arg("critInterval").toInt();
  configSave(*pCfg);
  server.send(200, "text/html", "<html><body><h1>Saved! Restarting...</h1></body></html>");
  delay(2000);
  ESP.restart();
}

void webserverInit(DeviceConfig &cfg) {
  pCfg = &cfg;
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

void webserverHandle() {
  server.handleClient();
}
