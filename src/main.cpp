#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

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

float critVoltage = 0.0;          // Critical voltage level (V)
unsigned long infoInterval = 0;   // Info messages interval, minutes
unsigned long infoIntervalMs = 0; // Info messages interval, milliseconds
unsigned long critInterval = 0;   // Critical messages interval, minutes
unsigned long critIntervalMs = 0; // Critical messages interval, milliseconds

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

const unsigned long voltOnTime = 20;                             // Voltmeter active, seconds
unsigned long voltOnTimeMs = convertSecondsToMillis(voltOnTime); // Voltmeter active, milliseconds

bool isKeyExists(const char *namespaceName, const char *keyName)
{
  config.begin(namespaceName, true);
  bool exists = config.isKey(keyName);
  config.end();
  return exists;
}

// Set default config
void setDefaultConfig()
{
  ssid = "Starlink172";
  password = "Qwe123rty456";
  serverIP = "141.144.245.187";
  deviceId = "BATT#1";
  critVoltage = 11.0;
  infoInterval = 300;
  critInterval = 120;
}

// Save configuration
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

// Load configuration
void loadConfig()
{
  if (!isKeyExists(namespaceName, "ssid"))
  {
    setDefaultConfig();
    saveConfig();
  }
  else
  {
    config.begin(namespaceName, true);
    ssid = config.getString("ssid", "");
    Serial.println("SSID: " + ssid);
    password = config.getString("password", "");
    Serial.println("Password: ************");
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

  // Converted time intervals
  infoIntervalMs = convertMinutesToMillis(infoInterval);
  critIntervalMs = convertMinutesToMillis(critInterval);
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
  return corrFactor * roundf(batteryVoltage * 10) / 10.0;
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
  String html = "<html><head><meta charset='UTF-8'></head><body><h1>Battery monitor cofig</h1>";
  html += "<form action='/save' method='post'>";
  html += "SSID: <input type='text' name='ssid' value='" + ssid + "'><br>";
  html += "Password: <input type='text' name='password' value='" + password + "'><br>";
  html += "Server URL: <input type='text' name='serverIP' value='" + serverIP + "'><br>";
  html += "Device ID: <input type='text' name='deviceId' value='" + deviceId + "'><br>";
  html += "Critical Voltage: <input type='number' name='critVoltage' value='" + String(critVoltage) + "' step='0.1'><br>";
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
    WiFi.softAP("BCM-AP", "12345678");

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
  float batteryVoltage = roundf(readBatteryVoltage() * 10) / 10.0;
  if (batteryVoltage > 0)
  {
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
}

// Main loop
void loop()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    float batteryVoltage = roundf(readBatteryVoltage() * 10) / 10.0;
    if (batteryVoltage > 0)
    {
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
  if (isButtonPressed && millis() - buttonPressStartTime >= 10 * 1000) // 10 seconds
  {
    if (!isButtonHeld)
    {
      isButtonHeld = true;
      Serial.println("Button held for 10 seconds, resetting configuration.");

      // Clear the configuration and set default values
      setDefaultConfig();
      saveConfig();

      Serial.println("Restarting ESP32.....");
      ESP.restart();
    }
  }

  delay(100);
}
