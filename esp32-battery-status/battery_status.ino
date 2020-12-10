// Requires ArduinoJson - arduinojson.org

#include <Arduino.h>
#include <ArduinoJson.h>

#include <WiFi.h>
#include <WiFiMulti.h>

#include <HTTPClient.h>
#include "config.sec.h"

#define USE_SERIAL Serial

WiFiMulti wifiMulti;

void setup() {

    USE_SERIAL.begin(115200);

    USE_SERIAL.println();
    USE_SERIAL.println();
    USE_SERIAL.println();

    for(uint8_t t = 4; t > 0; t--) {
        USE_SERIAL.printf("[SETUP] WAIT %d\n", t);
        USE_SERIAL.flush();
        delay(1000);
    }

    wifiMulti.addAP(WLAN_SSID, WLAN_PASSWORD);
}

String GetPayload() {
  StaticJsonDocument<500> doc;
  doc["temperature"] = 12;
  doc["watts"] = 124.2;
  doc["amp"] = 1231;
  
  String output;
  serializeJson(doc, output);
  return output;
}

void loop() {
    // wait for WiFi connection
    if((wifiMulti.run() == WL_CONNECTED)) {

        HTTPClient http;

        USE_SERIAL.print("[HTTP] begin\n");
  
        http.begin("http://"  HTTP_USER  ":"  HTTP_PASSWORD "@" HTTP_ADDRESS); //HTTP
        http.addHeader("Content-Type", "application/json");
        
        // Post the payload
        int httpCode = http.POST(GetPayload());

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
