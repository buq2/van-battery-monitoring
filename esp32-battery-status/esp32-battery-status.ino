// Requires ArduinoJson - arduinojson.org

#include <Arduino.h>
#include <ArduinoJson.h>

#include <WiFi.h>
#include <WiFiMulti.h>

#include <HTTPClient.h>
#include "config.sec.h"

#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>


TFT_eSPI tft = TFT_eSPI();

#define USE_SERIAL Serial
#define FONT_SIZE 8

WiFiMulti wifiMulti;

/*=============================
 * Data
 *============================= 
 */


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

/*=============================
 * Getting new data
 *============================= 
 */

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

/*=============================
 * BLE
 *============================= 
 */

 
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "06046fb5-2505-4798-9a5a-89dd85655c3f"
#define CHARACTERISTIC_UUID "e5d3a406-a784-4bb1-947b-630a9732098a"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

class ServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    BLEDevice::startAdvertising();
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

/*=============================
 * Setup
 *============================= 
 */

void setup_ble() {
  // Create the BLE Device
  BLEDevice::init("VAN Charge Monitor");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   
                      // | BLECharacteristic::PROPERTY_WRITE 
                      | BLECharacteristic::PROPERTY_NOTIFY 
                      | BLECharacteristic::PROPERTY_INDICATE
                    );

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
}

void setup_serial() {
  USE_SERIAL.begin(115200);

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  /*for(uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] WAIT %d\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }*/
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
  setup_ble();
}

/*=============================
 * JSON
 *============================= 
 */
 
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

/*=============================
 * Display
 *============================= 
 */

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

/*=============================
 * WLAN / HTTP
 *============================= 
 */

void UpdateHTTP(const ChargerStatus &status, const String &json_payload) {
  // Check WLAN connection
  if((wifiMulti.run() == WL_CONNECTED)) {
    HTTPClient http;

    USE_SERIAL.print("[HTTP] begin\n");

    http.begin("http://"  HTTP_USER  ":"  HTTP_PASSWORD "@" HTTP_ADDRESS); //HTTP
    http.addHeader("Content-Type", "application/json");
    
    // Post the payload
    int httpCode = http.POST(json_payload);

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
}

/*=============================
 * BLE
 *============================= 
 */

void UpdateBLE(const ChargerStatus &status, const String &json_payload) {
  // Notify changed value
  if (deviceConnected) {
    pCharacteristic->setValue((uint8_t*)json_payload.c_str(), json_payload.length());
    pCharacteristic->notify();
  }
  
  // Disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // Give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // Restart advertising
    Serial.println("[BLE] Start advertising");
    oldDeviceConnected = deviceConnected;
  }
  
  // Connecting
  if (deviceConnected && !oldDeviceConnected) {
    // Do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
}

/*=============================
 * Main Loop
 *============================= 
 */

void loop() {
    const auto status = GetChargerStatus();
    const auto json_payload = GetHttpPayload(status);
    DrawStatus(status);
    UpdateHTTP(status, json_payload);
    UpdateBLE(status, json_payload);

    delay(5000);
}
