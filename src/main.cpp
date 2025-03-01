#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <nvs_flash.h>

// Configuration storage
const char *namespaceName = "config";
Preferences config;

// Web server instance
WebServer server(80);

// WiFi credentials
String ssid = "";
String password = "";

// Server URL
String serverIP = "";

// Device identifier
String deviceId = "";

// Pin definitions
const int voltagePin = 32;  // Analog input for voltage measurement
const int buttonPin = 34;   // Button to activate voltmeter
const int voltmeterPin = 4; // Control pin for voltmeter via transistor

// Voltage measurement parameters
const float resistorR1 = 30000.0; // 30kΩ resistor
const float resistorR2 = 7500.0;  // 7.5kΩ resistor
const float vRef = 3.3;           // Reference voltage for ESP32 ADC
const float adcMax = 4095.0;      // Maximum ADC value
const int numSamples = 100;       // Number of samples for averaging
const float corrFactor = 1.0468;  // Correction factor for calibration

float critVoltage = 0.0;            // Critical voltage level (V)
unsigned long infoInterval = 0;     // Info messages interval, minutes
unsigned long critInterval = 0;     // Critical messages interval, minutes
const unsigned long voltOnTime = 0; // Voltmeter active, seconds

unsigned long lastInfoTime = 0;
unsigned long lastCriticalTime = 0;
unsigned long voltmeterStartTime = 0;
unsigned long buttonPressStartTime = 0;

bool isVoltmeterOn = false;
bool isButtonPressed = false;
bool isButtonHeld = false;

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
  config.putString("serverIP", serverIP);
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
  Serial.println("SSID: " + ssid);
  password = config.getString("password", "");
  Serial.println("Password: " + password);
  serverIP = config.getString("serverIP", "");
  Serial.println("Server URL: " + serverIP);
  deviceId = config.getString("deviceId", "");
  Serial.println("Device ID: " + deviceId);
  critVoltage = config.getFloat("critVoltage", 0.0);
  Serial.println("Critical Voltage: " + String(critVoltage));
  infoInterval = config.getULong("infoInterval", 0);
  Serial.println("Info Interval: " + String(infoInterval));
  critInterval = config.getULong("critInterval", 0);
  Serial.println("Critical Interval: " + String(critInterval));
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
    String serverUrl = "http://" + serverIP + ":5000/send";
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
  String html = "<html><head><meta charset='UTF-8'></head><body><h1>ESP32 Web Server</h1>";
  html += "<form action='/save' method='post'>";
  html += "SSID: <input type='text' name='ssid' value='" + ssid + "'><br>";
  html += "Password: <input type='password' name='password' value='" + password + "'><br>";
  html += "Server URL: <input type='text' name='serverIP' value='" + serverIP + "'><br>";
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
  serverIP = server.arg("serverIP");
  deviceId = server.arg("deviceId");
  critVoltage = server.arg("critVoltage").toFloat();
  infoInterval = server.arg("infoInterval").toInt();
  critInterval = server.arg("critInterval").toInt();

  saveConfig();
  server.send(200, "text/html", "<html><body><h1>Config Saved! Restarting...</h1></body></html>");
  delay(2000);
  Serial.println("Restarting ESP32 .....");
  ESP.restart();
}

// Initialize WiFi connection
void setupWiFi()
{
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) // 30 seconds
  {
    delay(1000);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nConnected to WiFi");
  }
  else
  {
    Serial.println("\nFailed to connect to WiFi. Starting AP mode.");
    WiFi.mode(WIFI_MODE_AP);
    WiFi.softAP("ESP32-AP", "12345678");

    Serial.print("IP Address in AP Mode: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", handleRoot);
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

  loadConfig();
  setupWiFi();

  // Send the first info message
  float batteryVoltage = readBatteryVoltage();
  if (batteryVoltage <= critVoltage)
  {
    sendToServer("ALERT", batteryVoltage);
    lastCriticalTime = millis();
  }
  else
  {
    sendToServer("INFO", batteryVoltage);
    lastInfoTime = millis();
  }
}

// Main loop
void loop()
{
  if (WiFi.status() == WL_CONNECTED)
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
  }
  else if (WiFi.getMode() == WIFI_MODE_AP)
  {
    server.handleClient();
  }

  bool currentButtonState = digitalRead(buttonPin);
  if (currentButtonState == HIGH && !isButtonPressed)
  {
    isButtonPressed = true;
    buttonPressStartTime = millis();
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

  // If the button is pressed for more than 10 seconds reset config
  if (isButtonPressed && millis() - buttonPressStartTime >= 10000) // 10 seconds
  {
    if (!isButtonHeld)
    {
      isButtonHeld = true;
      Serial.println("Button held for 10 seconds, resetting configuration.");

      // Clear the configuration and set default values
      nvs_flash_erase();
      config.begin(namespaceName, false);
      config.putString("ssid", "Starlink1721");
      config.putString("password", "Qwe123rty456");
      config.putString("serverIP", "141.144.245.187");
      config.putString("deviceId", "BATT#1");
      config.putFloat("critVoltage", 11.9);
      config.putULong("infoInterval", 15);
      config.putULong("critInterval", 5);
      config.end();

      Serial.println("Restarting ESP32.....");

      ESP.restart();
    }
  }

  delay(100);
}
