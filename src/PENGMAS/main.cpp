#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ModbusRTUMaster.h>
#include <time.h>
#include <HardwareSerial.h>
#include <HTTPClient.h>

// RS485 pin definition
const uint8_t DERE_PIN = 4;

HardwareSerial mySerial(1);
const uint8_t RXpin = 16;
const uint8_t TXpin = 17;

// WiFi Declare
String wifiSSID = "Xiaomi";
String wifiPassword = "bijibijian";

// NTP Configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 6 * 3600;  // WIB Time, Indonesia
const int daylightOffset_sec = 3600;

// Supabase API credentials
const char* supabaseUrl = "https://fxmhhmvyattgrgdkbqbh.supabase.co";
const char* supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImZ4bWhobXZ5YXR0Z3JnZGticWJoIiwicm9sZSI6ImFub24iLCJpYXQiOjE3MjI2MDQ5NzMsImV4cCI6MjAzODE4MDk3M30.Ev7Yr7xPc965XT4TCSystOf18pMViyKpQhTrBtyULzA";
String endpoint = "/rest/v1/sensor_data";

// Making modbus object
ModbusRTUMaster modbus(mySerial, DERE_PIN);
uint16_t holdingRegisterWindSpeed[1] = {0};
uint16_t holdingRegisterWindDirection[1] = {0};
uint16_t holdingRegisterSolarRadiation[1] = {0};
uint16_t holdingRegisterClimate[3] = {0, 0, 0};

float windSpeed;
float windDirection;
float solarRadiation;
float temperature;
float humidity;
float pressure;

// Function Declare
void postHTTP();
void readWindSpeed();
void readWindDirection();
void readSolarRadiation();
void readClimateData();
void connectWifi();
void AutoReconnectWiFi();
void syncLocalTime();
String getFormattedDate();
void processError();

// Setting interval time
unsigned long previousMillis = 0;
const long interval = 1000;

void setup() {
  Serial.begin(9600);
  modbus.begin(9600, SERIAL_8N1, RXpin, TXpin);
  modbus.setTimeout(1000);
  connectWifi();
  syncLocalTime();
}

void loop() {
  unsigned long currentMillis = millis();
  AutoReconnectWiFi(); 
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    readWindSpeed();
    delay(300);
    readWindDirection();
    delay(300);
    readSolarRadiation();
    delay(300);
    readClimateData();
    delay(300);
    postHTTP();
  }
}

void postHTTP() {
  Serial.println("Sending sensor data...");
  String url = String(supabaseUrl) + endpoint;
  HTTPClient http;
  String response;

  StaticJsonDocument<1000> buff;
  String jsonParams;
  String formattedDate = getFormattedDate();

  buff["timestamp"] = formattedDate;
  buff["wind_speed"] = windSpeed;
  buff["wind_direction"] = windDirection;
  buff["solar_radiation"] = solarRadiation;
  buff["temperature"] = temperature;
  buff["humidity"] = humidity;
  buff["pressure"] = pressure;

  serializeJson(buff, jsonParams);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", supabaseKey);
  int statusCode = http.POST(jsonParams);
  response = http.getString();

  if (statusCode == 201) {
    Serial.println("Post Method Success!");
  } else {
    Serial.println("Post Method Failed!");
    Serial.println("HTTP Response code: " + String(statusCode));
    Serial.println("Response: " + response);
  }

  http.end();
}

// RK100-02 Sensor
void readWindSpeed() {
  if (modbus.readHoldingRegisters(1, 0, holdingRegisterWindSpeed, 1)) {
    windSpeed = holdingRegisterWindSpeed[0] / 10.0;
    Serial.print("Wind Speed: ");
    Serial.print(windSpeed);
    Serial.println(" m/s");
  } else {
    processError();
  }
}

// RK110-02 Sensor
void readWindDirection() {
  if (modbus.readHoldingRegisters(2, 0, holdingRegisterWindDirection, 1)) {
    windDirection = holdingRegisterWindDirection[0];
    Serial.print("Wind Direction: ");
    Serial.print(windDirection);
    Serial.println(" degree");
  } else {
    processError();
  }
}

// RK200-04 Sensor
void readSolarRadiation() {
  if (modbus.readHoldingRegisters(6, 0, holdingRegisterSolarRadiation, 1)) {
    solarRadiation = holdingRegisterSolarRadiation[0];
    Serial.print("Solar Radiation: ");
    Serial.print(solarRadiation);
    Serial.println(" W/m²");
  } else {
    processError();
  }
}

// RK330-01 Sensor
void readClimateData() {
  if (modbus.readHoldingRegisters(3, 0, holdingRegisterClimate, 3)) {
    temperature = holdingRegisterClimate[0] / 10.0;
    humidity = holdingRegisterClimate[1] / 10.0;
    pressure = holdingRegisterClimate[2] / 10.0;

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");
    Serial.print("Pressure: ");
    Serial.print(pressure);
    Serial.println(" mbar");
  } else {
    processError();
  }
}

void connectWifi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  unsigned long wifiConnectStart = millis();

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    if (millis() - wifiConnectStart >= 10000) { 
      Serial.println("\nWiFi Connection Failed");
      return;
    }
  }
  Serial.println("\nWiFi Connected");
  Serial.println(WiFi.localIP());
}

void AutoReconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Disconnected");
    connectWifi();
  }
}

void syncLocalTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void processError() {
  if (modbus.getTimeoutFlag()) {
    Serial.println(F("Connection timed out"));
    modbus.clearTimeoutFlag();
  } else if (modbus.getExceptionResponse() != 0) {
    Serial.print(F("Received exception response: "));
    Serial.print(modbus.getExceptionResponse());
    switch (modbus.getExceptionResponse()) {
      case 1:
        Serial.println(F(" - Illegal function"));
        break;
      case 2:
        Serial.println(F(" - Illegal data address"));
        break;
      case 3:
        Serial.println(F(" - Illegal data value"));
        break;
      case 4:
        Serial.println(F(" - Server device failure"));
        break;
      default:
        Serial.println();
        break;
    }
    modbus.clearExceptionResponse();
  } else {
    Serial.println("An error occurred");
  }
}

String getFormattedDate() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Failed to obtain time";
  }
  char buffer[80];
  strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}
