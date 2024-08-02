#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ModbusRTUMaster.h>
#include <time.h>
#include <HardwareSerial.h>
#include <ESP_Google_Sheet_Client.h>
#include <GS_SDHelper.h>

//RS485 pin definition
const uint8_t DERE_PIN = 4;

HardwareSerial mySerial(1);
const uint8_t RXpin = 16;
const uint8_t TXpin = 17;

// WiFi Declare
String wifiSSID = "YOUR SSID ID";
String wifiPassword = "YOUR SSID PASSWORD";

// NTP Configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 6 * 3600;  //WIB Time, Indonesia
const int daylightOffset_sec = 3600;

// Google Project ID
#define PROJECT_ID "YOUR PROJECT ID"
// Service Account's client email
#define CLIENT_EMAIL "YOUR CLIENT EMAIL ID"
// Service Account's private key
const char PRIVATE_KEY[] PROGMEM = "-----BEGIN PRIVATE KEY-----\n-----END PRIVATE KEY-----\n";

// The ID of the spreadsheet where you'll publish the data
const char spreadsheetId[] = "YOUR SPREADSHEET ID";


// Making modbus object
ModbusRTUMaster modbus(mySerial, DERE_PIN);
uint16_t holdingRegisterWindSpeed [1] = {0};
uint16_t holdingRegisterWindDirection [1] = {0};
uint16_t holdingRegisterSolarRadiation [1]= {0};
uint16_t holdingRegisterClimate [3]= {0,0,0};

float windSpeed;
float windDirection;
float solarRadiation;
float temperature;
float humidity;
float pressure;

// Function Declare
void postHTTP();
void postToGoogleSheets();
void tokenStatusCallback(TokenInfo info);
void readWindSpeed();
void readWindDirection();
void readSolarRadiation();
void readClimateData();
void connectWifi();
void AutoReconnectWiFi();
void syncLocalTime();
String getFormattedDate();
void processError();

//Setting interval time
unsigned long previousMillis = 0;
const long interval = 1000;

void setup() {
  Serial.begin(9600);
  modbus.begin(9600, SERIAL_8N1, RXpin, TXpin);
  modbus.setTimeout(1000);
  connectWifi();
  syncLocalTime();
  GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);
  GSheet.setTokenCallback(tokenStatusCallback);
  GSheet.setPrerefreshSeconds(10 * 60);
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
    postToGoogleSheets();
    //postHTTP();
  }
}

void postHTTP() {
  Serial.println("Sending data sensors...");
  String url = "https://yourapi/api/sensor";
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

  serializeJson(buff, jsonParams);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int statusCode = http.POST(jsonParams);
  response = http.getString();

  if (statusCode == 200) {
  Serial.println("Post Method Success!");

  } else {
    Serial.println("Post Method Failed!");
  }
}

void postToGoogleSheets() {
  Serial.println("Sending data sensors to Google Sheets...");

  FirebaseJson response;
  FirebaseJson valueRange;

  // Get timestamp
  String formattedDate = getFormattedDate();

  valueRange.add("majorDimension", "COLUMNS");
  valueRange.set("values/[0]/[0]", formattedDate);
  valueRange.set("values/[1]/[0]", windSpeed);
  valueRange.set("values/[2]/[0]", windDirection);
  valueRange.set("values/[3]/[0]", solarRadiation);
  valueRange.set("values/[4]/[0]", temperature);
  valueRange.set("values/[5]/[0]", humidity);
  valueRange.set("values/[6]/[0]", pressure);
  
  // For Google Sheet API ref doc, go to https://developers.google.com/sheets/api/reference/rest/v4/spreadsheets.values/append
  // Append values to the spreadsheet
  bool success = GSheet.values.append(&response /* returned response */, spreadsheetId /* spreadsheet Id to append */, "Sheet1!A1" /* range to append */, &valueRange /* data range to append */);
  if (success){
    response.toString(Serial, true);
    valueRange.clear();
    Serial.println("Post Method Success!");
  }
  else{
    Serial.println(GSheet.errorReason());
    Serial.println("Post Method Failed!");
  }
}

void tokenStatusCallback(TokenInfo info){
    if (info.status == token_status_error){
        GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
        GSheet.printf("Token error: %s\n", GSheet.getTokenError(info).c_str());
    }
    else{
        GSheet.printf("Token info: type = %s, status = %s\n", GSheet.getTokenType(info).c_str(), GSheet.getTokenStatus(info).c_str());
    }
}

// RK100-02 Sensor
void readWindSpeed() {
 if(modbus.readHoldingRegisters(1, 0, holdingRegisterWindSpeed, 1)){
    windSpeed = holdingRegisterWindSpeed[0]/10.0;
    Serial.print("Wind Speed: ");
    Serial.print(windSpeed);
    Serial.println(" m/s");
 }
 else processError();
}

// RK110-02 Sensor
void readWindDirection() {
  if(modbus.readHoldingRegisters(2, 0, holdingRegisterWindDirection, 1)){
    windDirection = holdingRegisterWindDirection[0];
    Serial.print("Wind Direction: ");
    Serial.print(windDirection);
    Serial.println(" degree");
 }
 else processError();
}

// RK200-04 Sensor
void readSolarRadiation() {
    if(modbus.readHoldingRegisters(6, 0, holdingRegisterSolarRadiation, 1)){
    solarRadiation = holdingRegisterSolarRadiation[0];
    Serial.print("Solar Radiation : ");
    Serial.print(solarRadiation);
    Serial.println(" W/m²");
 }
 else processError();
}

// RK330-01 Sensor
void readClimateData() {
  if(modbus.readHoldingRegisters(3, 0, holdingRegisterClimate, 3)){
    temperature = holdingRegisterClimate[0]/10.0;
    humidity = holdingRegisterClimate[1]/10.0;
    pressure = holdingRegisterClimate[2]/10.0;

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");
    Serial.print("Pressure: ");
    Serial.print(pressure);
    Serial.println(" mbar");
 }
 else processError();
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

void AutoReconnectWiFi(){
  if (WiFi.status()!=WL_CONNECTED){
    Serial.println("Wifi Disconnect");
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
  }
  else if (modbus.getExceptionResponse() != 0) {
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
  }
  else {
    Serial.println("An error occurred");
  }
}

String getFormattedDate() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Failed to obtain time";
  }
  char buffer[80];
  strftime(buffer, 80, "%Y-%m-%d %H:%M:%S ", &timeinfo);
  return String(buffer);
}
