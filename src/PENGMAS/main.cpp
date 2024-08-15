#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ModbusRTUMaster.h>
#include <time.h>
#include <HardwareSerial.h>

//RS485 pin definition
const uint8_t dePin = 4;
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
ModbusRTUMaster modbus(mySerial, dePin);
uint16_t holdingRegisterWindSpeed [1] = {0};    // The data read from the wind speed register
uint16_t holdingRegisterWindDirection [1] = {0}; // The data read from the wind direction register
uint16_t holdingRegisterSolarRadiation [1]= {0}; // The data read from the solar radiation register
uint16_t holdingRegisterClimate [3]= {0,0,0}; // The data read from the climate registers
uint16_t holdingRegisterRainfall [2]= {0,0}; // The data read from the climate registers
uint16_t holdingRegisterGas[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

float windSpeed;
float windDirection;
float solarRadiation;
float temperature;
float humidity;
float pressure;
float rainfall;
float PM2;
float PM10;
float CO2;
float O2;
float SO2;
float NO2;
float O3;

// Function Declare
void postHTTP();
void readWindSpeed();
void readWindDirection();
void readSolarRadiation();
void readClimateData();
void readRainfall();
void readGas();
void connectWifi();
void AutoReconnectWiFi();
void syncLocalTime();
void processError();
String getFormattedDate();


// Setting interval time
unsigned long previousMillis = 0;
const long interval = 1000;

void setup() {
  Serial.begin(9600);
  modbus.begin(9600, SERIAL_8N1, RXpin, TXpin); // baudrate, config, RX, TX
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
    readRainfall();
    delay(300);
    readGas();
    delay(300);
    postHTTP();
  }
}

void postHTTP() {
  Serial.println("Sending data sensors...");
  String url = String(supabaseUrl) + endpoint;
  HTTPClient http;
  String response;

  StaticJsonDocument<1000> buff;
  String jsonParams;
  String formattedDate = getFormattedDate();

  buff["date"] = formattedDate;
  buff["windSpeed"] = String(windSpeed);
  buff["windDirection"] = String(windDirection);
  buff["solarRadiation"] = String(solarRadiation);
  buff["temperature"] = String(temperature);
  buff["humidity"] = String(humidity);
  buff["pressure"] = String(pressure);
  buff["rainfall"] = String(rainfall);
  buff["PM2.5"] = String(PM2);
  buff["PM10"] = String(PM10);
  buff["CO2"] = String(CO2);
  buff["O2"] = String(O2);
  buff["SO2"] = String(SO2);
  buff["NO2"] = String(NO2);
  buff["O3"] = String(O3);

  serializeJson(buff, jsonParams);
  http.begin(url);
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
 if(modbus.readHoldingRegisters(1, 0, holdingRegisterWindSpeed, 1)){  //readHoldingRegisters(ID Slave, startAddress, buffer, quantity)
    windSpeed = holdingRegisterWindSpeed[0]/10.0;
    Serial.print("Wind Speed: ");
    Serial.print(windSpeed, 2);
    Serial.println(" m/s");
  } else {
    processError();
  }
}

// RK110-02 Sensor
void readWindDirection() {
  if(modbus.readHoldingRegisters(2, 0, holdingRegisterWindDirection, 1)){ //readHoldingRegisters(ID Slave, startAddress, buffer, quantity)
    windDirection = holdingRegisterWindDirection[0];
    Serial.print("Wind Direction: ");
    Serial.print(windDirection, 2);
    Serial.println(" degree");
  } else {
    processError();
  }
}

// RK200-04 Sensor
void readSolarRadiation() {
    if(modbus.readHoldingRegisters(6, 0, holdingRegisterSolarRadiation, 1)){  //readHoldingRegisters(ID Slave, startAddress, buffer, quantity)
    solarRadiation = holdingRegisterSolarRadiation[0];
    Serial.print("Solar Radiation : ");
    Serial.print(solarRadiation, 2);
    Serial.println(" W/m²");
  } else {
    processError();
  }
}

// RK330-01 Sensor
void readClimateData() {
  if(modbus.readHoldingRegisters(3, 0, holdingRegisterClimate, 3)){ //readHoldingRegisters(ID Slave, startAddress, buffer, quantity)
    temperature = holdingRegisterClimate[0]/10.0;
    humidity = holdingRegisterClimate[1]/10.0;
    pressure = holdingRegisterClimate[2]/10.0;

    Serial.print("Temperature: ");
    Serial.print(temperature, 2);
    Serial.println(" °C");
    Serial.print("Humidity: ");
    Serial.print(humidity, 2);
    Serial.println(" %");
    Serial.print("Pressure: ");
    Serial.print(pressure, 2);
    Serial.println(" mbar");
  } else {
    processError();
  }
}

// RK110-02 Sensor
void readRainfall() {
  if(modbus.readHoldingRegisters(4, 0, holdingRegisterRainfall, 2)){ //readHoldingRegisters(ID Slave, startAddress, buffer, quantity)
    rainfall = holdingRegisterRainfall[1] / 10;
    Serial.print("Rainfall : ");
    Serial.print(rainfall, 2);
    Serial.println(" mm");
 }
 else processError();
}

//RK300-08
void readGas()
{
  if (modbus.readHoldingRegisters(5, 0, holdingRegisterGas, 16)) //readHoldingRegisters(ID Slave, startAddress, buffer, quantity)
  {
    PM2 = holdingRegisterGas[0];
    PM10 = holdingRegisterGas[1];
    CO2 = holdingRegisterGas[6];
    O2 = holdingRegisterGas[7]/10;
    SO2 = holdingRegisterGas[8] / 10;
    NO2 = holdingRegisterGas[9] / 10;
    O3 = holdingRegisterGas[10]/10;

    Serial.print("PM2.5: ");
    Serial.print(PM2);
    Serial.println(" ug/m3");

    Serial.print("PM10: ");
    Serial.print(PM10);
    Serial.println(" ug/m3");

    Serial.print("CO2: ");
    Serial.print(CO2);
    Serial.println(" ppm");

    Serial.print("O2: ");
    Serial.print(O2);
    Serial.println(" %VOL");

    Serial.print("SO2: ");
    Serial.print(SO2);
    Serial.println(" ppm");

    Serial.print("NO2: ");
    Serial.print(NO2);
    Serial.println(" ppm");

    Serial.print("O3: ");
    Serial.print(O3);
    Serial.println(" ppm");
  }
  else
    processError();
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
