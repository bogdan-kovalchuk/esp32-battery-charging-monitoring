#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const String ssid = "Starlink172";
const String password = "Qwe123rty456";

// Server URL
const String serverUrl = "http://141.144.245.187:5000/send";

// Device identifier
const String deviceId = "АКБ №1";

// Pin definitions
const int voltagePin = 34;  // Analog input for voltage measurement
const int buttonPin = 5;    // Button to activate voltmeter
const int voltmeterPin = 4; // Control pin for voltmeter via transistor

// Voltage measurement parameters
const float resistorR1 = 30000.0;      // 30kΩ resistor
const float resistorR2 = 7500.0;       // 7.5kΩ resistor
const float vRef = 3.3;                // Reference voltage for ESP32 ADC
const float adcMax = 4095.0;           // Maximum ADC value
const int numSamples = 100;            // Number of samples for averaging
const float correctionFactor = 1.0468; // Correction factor for calibration

// Configuration parameters
const float criticalVoltage = 11.25;               // Critical voltage level (V)
const unsigned long infoInterval = 3600000;        // Info messages interval
const unsigned long criticalInterval = 30 * 60000; // Critical messages interval
const unsigned long voltmeterOnTime = 20000;       // 20 seconds to keep voltmeter active

unsigned long lastInfoTime = 0;
unsigned long lastCriticalTime = 0;
unsigned long voltmeterStartTime = 0;
bool isVoltmeterOn = false;
bool isButtonPressed = false;

// Function to read battery voltage with averaging
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
  return batteryVoltage * correctionFactor;
}

// Function to send voltage data to the server
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
    msg["voltage"] = roundf(voltage * 100) / 100.0;

    if (msgType == "ALERT")
    {
      msg["critical_voltage"] = criticalVoltage;
    }

    String msgString;
    serializeJson(msg, msgString);
    int httpResponseCode = http.POST(msgString);

    Serial.println("Response: " + String(httpResponseCode));
    http.end();
  }
}

// Setup function
void setup()
{
  Serial.begin(115200);
  pinMode(voltmeterPin, OUTPUT);
  pinMode(buttonPin, INPUT);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Immediately send the first info message
  float batteryVoltage = readBatteryVoltage();
  sendToServer("INFO", batteryVoltage);
  lastInfoTime = millis();

  // If voltage is below critical level, send a critical alert
  if (batteryVoltage <= criticalVoltage)
  {
    sendToServer("ALERT", batteryVoltage);
    lastCriticalTime = millis();
  }
}

// Main loop
void loop()
{
  float batteryVoltage = readBatteryVoltage();

  // Send informational message
  if (millis() - lastInfoTime >= infoInterval)
  {
    lastInfoTime = millis();
    sendToServer("INFO", batteryVoltage);
  }

  // Send critical message if voltage is too low
  if (batteryVoltage <= criticalVoltage && millis() - lastCriticalTime >= criticalInterval)
  {
    lastCriticalTime = millis();
    sendToServer("ALERT", batteryVoltage);
  }

  // Handle button press to activate voltmeter
  bool currentButtonState = digitalRead(buttonPin);
  if (currentButtonState == HIGH && !isButtonPressed)
  {
    isButtonPressed = true;
  }

  if (currentButtonState == LOW && isButtonPressed)
  {
    isButtonPressed = false;
    Serial.println("Button released. Enabling voltmeter for 20 seconds.");
    digitalWrite(voltmeterPin, HIGH);
    voltmeterStartTime = millis();
    isVoltmeterOn = true;
  }

  // Turn off voltmeter after 20 seconds
  if (isVoltmeterOn && millis() - voltmeterStartTime >= voltmeterOnTime)
  {
    digitalWrite(voltmeterPin, LOW);
    isVoltmeterOn = false;
    Serial.println("Voltmeter turned off.");
  }

  delay(100);
}
