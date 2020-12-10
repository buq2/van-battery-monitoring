// Requires ArduinoJson - arduinojson.org

#include <Arduino.h>
#include <ArduinoJson.h>

#include <WiFi.h>
#include <WiFiMulti.h>

#include <HTTPClient.h>
#include "config.sec.h"


#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>'


TFT_eSPI tft = TFT_eSPI();

#define USE_SERIAL Serial
#define FONT_SIZE 8

WiFiMulti wifiMulti;

struct ComponentStatus {
  float power_w{0.0f};
  float current_a{0.0f};
  float voltage_v{0.0f};
};

struct ChargerStatus {
  ComponentStatus solar;
  ComponentStatus alternator;
  ComponentStatus battery;
};

ChargerStatus GetChargerStatus() {
  ChargerStatus out;
  
  out.solar.power_w = random(1000)/10.0f;
  out.solar.current_a = random(1000)/100.0f;
  out.solar.voltage_v = 12.0f + random(100)/100.0f;

  out.alternator.power_w = random(1000)/10.0f;
  out.alternator.current_a = random(1000)/100.0f;
  out.alternator.voltage_v = 12.0f + random(100)/100.0f;

  out.battery.power_w = random(1000)/10.0f;
  out.battery.current_a = random(1000)/100.0f;
  out.battery.voltage_v = 12.0f + random(100)/100.0f;
   
  return out;
}

void setup_serial() {
  USE_SERIAL.begin(115200);

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  for(uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] WAIT %d\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }
}

void setup_wlan() {
  wifiMulti.addAP(WLAN_SSID, WLAN_PASSWORD);
}

void setup_display() {
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);  // Adding a black background colour erases previous text automatically
}

void setup() {
  setup_serial();
  setup_wlan();
  setup_display();
}

template<typename T>
void ConvertComponentStatus(const ComponentStatus &status, T out) {
  out["power_w"] = status.power_w;
  out["current_a"] = status.current_a;
  out["voltage_v"] = status.voltage_v;
}

String GetHttpPayload(const ChargerStatus &status) {
  StaticJsonDocument<1024> doc;
  ConvertComponentStatus(status.battery, doc["battery"]);
  ConvertComponentStatus(status.solar, doc["solar"]);
  ConvertComponentStatus(status.alternator, doc["alternator"]);
  
  String output;
  serializeJson(doc, output);
  return output;
}

void DrawText(const char *str, const int x, int &y) {
  tft.drawString(str, x, y);
  y += FONT_SIZE;
}

void DrawSubComponent2(const char *txt, const float &val, int &y) {
  const int decimals = 2;
  const int x = 10;
  tft.drawString(txt, x, y);
  tft.drawFloat(val, decimals, x+tft.textWidth(txt), y);
  y += FONT_SIZE;
}

void DrawComponent(const ComponentStatus &status, int &y) {
  DrawSubComponent2("Power:", status.power_w, y);
  DrawSubComponent2("Current:", status.current_a, y);
  DrawSubComponent2("Voltage:", status.voltage_v, y);
}

void DrawStatus(const ChargerStatus &status) {
  int pos_y = 0;

  DrawText("Battery:", 0, pos_y);
  DrawComponent(status.battery, pos_y);
  
  DrawText("Solar:", 0, pos_y);
  DrawComponent(status.solar, pos_y);

  DrawText("Alternator:", 0, pos_y);
  DrawComponent(status.alternator, pos_y);
}

void loop() {
    const auto status = GetChargerStatus();
    DrawStatus(status);
    
    // wait for WiFi connection
    if((wifiMulti.run() == WL_CONNECTED)) {

        HTTPClient http;

        USE_SERIAL.print("[HTTP] begin\n");
  
        http.begin("http://"  HTTP_USER  ":"  HTTP_PASSWORD "@" HTTP_ADDRESS); //HTTP
        http.addHeader("Content-Type", "application/json");
        
        // Post the payload
        int httpCode = http.POST(GetHttpPayload(status));

        // httpCode will be negative on error
        if(httpCode > 0) {
            // HTTP header has been send and Server response header has been handled
            USE_SERIAL.printf("[HTTP] POST code: %d\n", httpCode);

            // All ok
            if(httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                USE_SERIAL.println(payload);
            }
        } else {
            USE_SERIAL.printf("[HTTP] POST returned error: %s\n", http.errorToString(httpCode).c_str());
        }

        http.end();
    } else {
      USE_SERIAL.print("[WLAN] Waiting WLAN\n");
    }

    delay(5000);
}
