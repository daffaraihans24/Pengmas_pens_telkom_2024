#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ModbusRTUMaster.h>

// RS485 pin definition
const uint8_t DERE_PIN = 4;

HardwareSerial mySerial(1);
const uint8_t RXpin = 16;
const uint8_t TXpin = 17;

// WiFi Declare
String wifiSSID = "Samsung";
String wifiPassword = "asdfghjk";

// NTP Configuration
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 6 * 3600; // WIB Time, Indonesia
const int daylightOffset_sec = 3600;

ModbusRTUMaster modbus(mySerial, DERE_PIN);
uint16_t holdingRegisterGas[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

float so2;
float o3;
float o2;
float co2;
float no2;
float pm;

void readGas();
void connectWifi();
void AutoReconnectWiFi();

unsigned long previousMillis = 0;
const long interval = 1000;

void setup()
{
    Serial.begin(9600);
    modbus.begin(9600, SERIAL_8N1, RXpin, TXpin);
    modbus.setTimeout(1000);
    connectWifi();
    //   syncLocalTime();
    // GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);
    // GSheet.setTokenCallback(tokenStatusCallback);
    // GSheet.setPrerefreshSeconds(10 * 60);
}

void loop()
{
    unsigned long currentMillis = millis();
    AutoReconnectWiFi();
    if (currentMillis - previousMillis >= interval)
    {
        previousMillis = currentMillis;
        readGas();
        delay(300);
         postFirebase();
    }
}

void connectWifi()
{
    Serial.print("Connecting to WiFi...");
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    unsigned long wifiConnectStart = millis();

    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        if (millis() - wifiConnectStart >= 10000)
        {
            Serial.println("\nWiFi Connection Failed");
            return;
        }
    }
    Serial.println("\nWiFi Connected");
    Serial.println(WiFi.localIP());
}

void AutoReconnectWiFi()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Wifi Disconnect");
        connectWifi();
    }
}

void postFirebase()
{
    String url = "https://daililog-default-rtdb.firebaseio.com/pengmas.json";
    HTTPClient http;
    String response;
    String jsonParams;
    // jsonParams = "{'test' : 'test'}";

    jsonParams = "{\"so2\": " + String(so2, 2) + 
               ", \"o3\": " + String(o3, 2) + 
               ", \"o2\": " + String(o2, 2) + 
               ", \"co2\": " + String(co2, 2) + 
               ", \"no2\": " + String(no2, 2) + 
               ", \"pm\": " + String(pm, 2) + "}";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int statusCode = http.POST(jsonParams);
    response = http.getString();

    if (statusCode == 200)
    {
        Serial.println("Post Method Success!");
    }
    else
    {
        Serial.println("Post Method Failed!");
    }
}

void readGas()
{
    if (modbus.readHoldingRegisters(5, 0, holdingRegisterGas, 6))
    {
        so2 = holdingRegisterGas[0];
        o3 = holdingRegisterGas[1];
        o2 = holdingRegisterGas[2];
        co2 = holdingRegisterGas[3];
        no2 = holdingRegisterGas[4];
        pm = holdingRegisterGas[5];

        Serial.print("SO2: ");
        Serial.print(so2);
        Serial.println(" ppm");

        Serial.print("O3: ");
        Serial.print(o3);
        Serial.println(" ppm");

        Serial.print("O2: ");
        Serial.print(o2);
        Serial.println(" %");

        Serial.print("CO2: ");
        Serial.print(co2);
        Serial.println(" ppm");

        Serial.print("NO2: ");
        Serial.print(no2);
        Serial.println(" ppm");

        Serial.print("PM2.5~10: ");
        Serial.print(pm);
        Serial.println(" ug/m3");
    }
    else
        processError();
}

void processError()
{
    if (modbus.getTimeoutFlag())
    {
        Serial.println(F("Connection timed out"));
        modbus.clearTimeoutFlag();
    }
    else if (modbus.getExceptionResponse() != 0)
    {
        Serial.print(F("Received exception response: "));
        Serial.print(modbus.getExceptionResponse());
        switch (modbus.getExceptionResponse())
        {
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
    else
    {
        Serial.println("An error occurred");
    }
}