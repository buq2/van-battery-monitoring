// Requires:
// ArduinoJson - arduinojson.org
// ModbusMaster - https://github.com/craftmetrics/esp32-modbusmaster



#include <Arduino.h>
#include <ArduinoJson.h>

#include <WiFi.h>
#include <WiFiMulti.h>

#include <HTTPClient.h>
#include "config.sec.h"

#include "common.h"
#include "modbus.h"
#include "display.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define USE_SERIAL Serial

WiFiMulti wifiMulti;


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

class ServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    USE_SERIAL.println("[BLE] Device connected");
    delay(500); 
    BLEDevice::startAdvertising();
  };

  void onDisconnect(BLEServer* pServer) {
    USE_SERIAL.println("[BLE] Device disconnected");
    delay(500); 
    BLEDevice::startAdvertising();
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
                      // BLECharacteristic::PROPERTY_READ   
                      // | BLECharacteristic::PROPERTY_WRITE 
                      BLECharacteristic::PROPERTY_NOTIFY 
                      // | BLECharacteristic::PROPERTY_INDICATE
                    );

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  auto ble2902 = new BLE2902();
  pCharacteristic->addDescriptor(ble2902);

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();

  // Enable notifications
  // This can also be done from android, but there seems to be a possible race condition in this code
  // which is easier to fix by setting the notifications to always on
  ble2902->setNotifications(true);

  // See:
  // https://github.com/nkolban/ESP32_BLE_Arduino/blob/master/src/BLECharacteristic.h
  // https://github.com/espressif/arduino-esp32/blob/master/libraries/BLE/examples/BLE_server_multiconnect/BLE_server_multiconnect.ino
}

void setup_serial() {
  USE_SERIAL.begin(115200);

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();
}

void setup_wlan() {
  wifiMulti.addAP(WLAN_SSID, WLAN_PASSWORD);
}

void setup() {
  setup_serial();
  setup_wlan();
  setup_display();
  setup_ble();
  setup_modbus();
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

  doc["total_daily_charge_ah"] = status.total_daily_charge_ah;
  doc["status_bits1"] = status.status_bits1;
  doc["status_bits2"] = status.status_bits2;
  doc["status_bits3"] = status.status_bits3;
  doc["battery_percentage"] = status.battery_percentage;
  doc["external_temperature_c"] = status.external_temperature_c;
  
  String output;
  serializeJson(doc, output);
  return output;
}

/*=============================
 * WLAN / HTTP
 *============================= 
 */

void UpdateHTTP(const ChargerStatus &status, const String &json_payload) {
  // Check WLAN connection
  if((wifiMulti.run() == WL_CONNECTED)) {
    HTTPClient http;
    http.setConnectTimeout(512); //ms
    http.setTimeout(512); //ms

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
  USE_SERIAL.println("[BLE] Sending data");
  
  pCharacteristic->setValue((uint8_t*)json_payload.c_str(), json_payload.length());
  pCharacteristic->notify();
  delay(50); // Maybe not needed
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
    print_battery_status_to_serial();
    delay(5000);
}
