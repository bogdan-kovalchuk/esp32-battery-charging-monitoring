#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// Configuration storage
const char *namespaceName = "config";
Preferences config;

// Web server instance
WebServer server(80);

// WiFi credentials
String ssid = "Starlink172";
String password = "Qwe123rty456";

// Server URL
String serverUrl = "http://141.144.245.187:5000/send";

// Device identifier
String deviceId = "АКБ №1";

// Pin definitions
const int voltagePin = 32;  // Analog input for voltage measurement
const int buttonPin = 34;   // Button to activate voltmeter
const int voltmeterPin = 4; // Control pin for voltmeter via transistor

// Voltage measurement parameters
const float resistorR1 = 30000.0; // 30kΩ resistor
const float resistorR2 = 7500.0;  // 7.5kΩ resistor
const float vRef = 3.3;           // Reference voltage for ESP32 ADC
const float adcMax = 4095.0;      // Maximum ADC value
int numSamples = 100;             // Number of samples for averaging
float corrFactor = 1.0468;        // Correction factor for calibration

// Configuration parameters
float critVoltage = 11.9;        // Critical voltage level (V)
unsigned long infoInterval = 15; // Info messages interval, minutes
unsigned long critInterval = 5;  // Critical messages interval, minutes
unsigned long voltOnTime = 20;   // Voltmeter active, seconds

unsigned long lastInfoTime = 0;
unsigned long lastCriticalTime = 0;
unsigned long voltmeterStartTime = 0;

bool isVoltmeterOn = false;
bool isButtonPressed = false;

// Convert time units
unsigned long convertMinutesToMillis(unsigned long minutes) { return minutes * 60000; }
unsigned long convertSecondsToMillis(unsigned long seconds) { return seconds * 1000; }

// Converted time intervals
unsigned long infoIntervalMs = convertMinutesToMillis(infoInterval);
unsigned long critIntervalMs = convertMinutesToMillis(critInterval);
unsigned long voltOnTimeMs = convertSecondsToMillis(voltOnTime);

// Load and save configuration
void saveConfig()
{
  config.begin(namespaceName, false);
  config.clear();
  config.putString("ssid", ssid);
  config.putString("password", password);
  config.putString("serverUrl", serverUrl);
  config.putString("deviceId", deviceId);
  config.putFloat("critVoltage", critVoltage);
  config.putULong("infoInterval", infoInterval);
  config.putULong("critInterval", critInterval);
  config.end();
}

void loadConfig()
{
  config.begin(namespaceName, true);
  if (!config.isKey("ssid"))
  {
    config.end();
    saveConfig();
    config.begin(namespaceName, true);
  }
  ssid = config.getString("ssid", "");
  password = config.getString("password", "");
  serverUrl = config.getString("serverUrl", "");
  deviceId = config.getString("deviceId", "");
  critVoltage = config.getFloat("critVoltage", 0.0);
  infoInterval = config.getULong("infoInterval", 0);
  critInterval = config.getULong("critInterval", 0);
  config.end();
}

// Read battery voltage
float readBatteryVoltage()
{
  long sumADC = 0;
  for (int i = 0; i < numSamples; i++)
  {
    sumADC += analogRead(voltagePin);
    delay(2);
  }
  float rawADC = sumADC / (float)numSamples;
  float voltageOut = (rawADC / adcMax) * vRef;
  float batteryVoltage = voltageOut * (1 + resistorR1 / resistorR2);
  return batteryVoltage * corrFactor;
}

// Send voltage data to the server
void sendToServer(String msgType, float voltage)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    // Create a JSON object
    JsonDocument msg;
    msg["device_id"] = deviceId;
    msg["msg_type"] = msgType;
    msg["voltage"] = roundf(voltage * 10) / 10.0;

    if (msgType == "ALERT")
    {
      msg["critical_voltage"] = critVoltage;
    }

    String msgString;
    serializeJson(msg, msgString);
    http.POST(msgString);
    http.end();
  }
}

void handleRoot()
{
  String html = "<html><body><h1>ESP32 Web Server</h1>";
  html += "<form action='/save' method='post'>";
  html += "SSID: <input type='text' name='ssid' value='" + ssid + "'><br>";
  html += "Password: <input type='password' name='password' value='" + password + "'><br>";
  html += "Server URL: <input type='text' name='serverUrl' value='" + serverUrl + "'><br>";
  html += "Device ID: <input type='text' name='deviceId' value='" + deviceId + "'><br>";
  html += "Critical Voltage: <input type='number' name='critVoltage' value='" + String(critVoltage) + "'><br>";
  html += "Info Interval: <input type='number' name='infoInterval' value='" + String(infoInterval) + "'><br>";
  html += "Critical Interval: <input type='number' name='critInterval' value='" + String(critInterval) + "'><br>";
  html += "<input type='submit' value='Save Config'></form></body></html>";

  server.send(200, "text/html", html);
}

void handleSave()
{
  ssid = server.arg("ssid");
  password = server.arg("password");
  serverUrl = server.arg("serverUrl");
  deviceId = server.arg("deviceId");
  critVoltage = server.arg("critVoltage").toFloat();
  infoInterval = server.arg("infoInterval").toInt();
  critInterval = server.arg("critInterval").toInt();

  saveConfig();
  server.send(200, "text/html", "<html><body><h1>Config Saved! Restarting...</h1></body></html>");
  delay(2000);
  ESP.restart();
}

// Initialize WiFi connection
void setupWiFi()
{
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000)
  {
    delay(1000);
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.softAP("ESP32-AP", "12345678");
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();
  }
}

// Setup function
void setup()
{
  Serial.begin(115200);
  pinMode(voltmeterPin, OUTPUT);
  pinMode(buttonPin, INPUT);

  setupWiFi();

  // Immediately send the first info message
  float batteryVoltage = readBatteryVoltage();
  sendToServer("INFO", batteryVoltage);
  lastInfoTime = millis();

  // If voltage is below critical level, send a critical alert
  if (batteryVoltage <= critVoltage)
  {
    sendToServer("ALERT", batteryVoltage);
    lastCriticalTime = millis();
  }
}

// Main loop
void loop()
{
  if (WiFi.getMode() == WIFI_AP)
  {
    server.handleClient();
  }
  else if (WiFi.status() == WL_CONNECTED)
  {
    float batteryVoltage = readBatteryVoltage();
    if (batteryVoltage > critVoltage && millis() - lastInfoTime >= infoIntervalMs)
    {
      lastInfoTime = millis();
      sendToServer("INFO", batteryVoltage);
    }
    if (batteryVoltage <= critVoltage && millis() - lastCriticalTime >= critIntervalMs)
    {
      lastCriticalTime = millis();
      sendToServer("ALERT", batteryVoltage);
    }
    bool currentButtonState = digitalRead(buttonPin);
    if (currentButtonState == HIGH && !isButtonPressed)
    {
      isButtonPressed = true;
    }
    if (currentButtonState == LOW && isButtonPressed)
    {
      isButtonPressed = false;
      digitalWrite(voltmeterPin, HIGH);
      voltmeterStartTime = millis();
      isVoltmeterOn = true;
    }
    if (isVoltmeterOn && millis() - voltmeterStartTime >= voltOnTimeMs)
    {
      digitalWrite(voltmeterPin, LOW);
      isVoltmeterOn = false;
    }
  }
  delay(100);
}
