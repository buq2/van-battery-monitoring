#include "modbus.h"

#if USE_MODBUS
#include <ModbusMaster.h>

#define Slave_ID 1 
ModbusMaster MODBUS_MASTER;

void setup_modbus() {
  Serial2.begin(9600, SERIAL_8N1);

  // No need for long timeouts
  // But actual timeout is in ModbusMaster library
  Serial2.setTimeout(100); 
  
  MODBUS_MASTER.begin(Slave_ID, Serial2);

  // By default the chage controller assumes 200Ah battery
  // Program correct (170Ah) battery size:
  // MODBUS_MASTER.writeSingleRegister(0xe002, 170);
}

enum ReadTypes {
    kReadFull,
    kReadUpper,
    kReadLower
};

struct DataToRead {
    DataToRead(uint16_t address, String name, float conversion_multiplier, ReadTypes read_type=kReadFull)
        : 
        address_(address),
        name_(name),
        conversion_multiplier_(conversion_multiplier),
        read_type_(read_type)
    {
    }

    uint8_t GetValue(float &out) const {
      out = 0.0f;
      auto result = MODBUS_MASTER.readHoldingRegisters(address_, 1);
      if (result != ModbusMaster::ku8MBSuccess) {
          Serial.println(name_ + ": " + modbusErrorString(result));
          return result;
      }
      
      auto data_u16 = MODBUS_MASTER.getResponseBuffer(0);
      switch(read_type_) {
      case kReadUpper:
          data_u16 = data_u16 >> 8;
          break;
      case kReadLower:
          data_u16 = data_u16 & 0x00ff;
          break;
      }
      out = data_u16*conversion_multiplier_;
      return result;
    }

    String GetName() const {return name_;}
 private:
    uint16_t address_;
    String name_;
    float conversion_multiplier_;
    ReadTypes read_type_;
};

static const DataToRead BATTERY_PERCENTAGE {0x0100,"battery_percentage", 1.0f};
static const DataToRead BATTERY_VOLTAGE {0x0101,"battery_voltage_v", 0.1f};
static const DataToRead TOTAL_CHARGE_CURRENT {0x0102,"total_charge_current_a", 0.01f};
static const DataToRead INTERNAL_TEMPERATURE {0x0103, "internal_temperature_c", 1.0f, kReadUpper};
static const DataToRead EXTERNAL_TEMPERATURE {0x0103, "external_temperature_c", 1.0f, kReadLower};
static const DataToRead GENERATOR_VOLTAGE {0x0104, "generator_voltage_v", 0.1f};
static const DataToRead GENERATOR_CURRENT {0x0105, "generator_current_a", 0.01f};
static const DataToRead GENERATOR_POWER {0x0106, "generator_power_w", 1.0f};
static const DataToRead SOLAR_VOLTAGE {0x0107, "solar_voltage_v", 0.1f};
static const DataToRead SOLAR_CURRENT {0x0108, "solar_current_a", 0.01f};
static const DataToRead SOLAR_POWER {0x0109, "solar_power_w", 1.0f};
static const DataToRead MIN_DAILY_VOLTAGE {0x010b, "battery_min_daily_voltage_v", 0.1f};
static const DataToRead MAX_DAILY_VOLTAGE {0x010c, "battery_max_daily_voltage_v", 0.1f};
static const DataToRead MAX_DAILY_CHARGE_CURRENT {0x010d, "max_daily_charge_current_a", 0.01f};
static const DataToRead MAX_DAILY_CHARGE_POWER {0x010f, "max_daily_charge_power_e", 1.0f};
static const DataToRead TOTAL_DAILY_CHARGE {0x0111, "total_daily_charge_ah", 1.0f};
static const DataToRead TOTAL_DAILY_CHARGE2 {0x0113, "total_daily_charge_?", 0.001f};
static const DataToRead STATUS_BITS1 {0x0120, "status_bits1", 1.0f};
static const DataToRead STATUS_BITS2 {0x0121, "status_bits2", 1.0f};
static const DataToRead STATUS_BITS3 {0x0122, "status_bits3", 1.0f};
static const DataToRead BATTERY_MAX_AH {0xe002, "battery_max_ah", 1.0f};

String modbusErrorString(uint8_t result)
{
  switch (result) {
  case ModbusMaster::ku8MBSuccess:
    return String("ku8MBSuccess");
  case ModbusMaster::ku8MBIllegalFunction:
    return String("ku8MBIllegalFunction");
  case ModbusMaster::ku8MBIllegalDataAddress:
    return String("ku8MBIllegalDataAddress");
  case ModbusMaster::ku8MBIllegalDataValue:
    return String("ku8MBIllegalDataValue");
  case ModbusMaster::ku8MBSlaveDeviceFailure:
    return String("ku8MBSlaveDeviceFailure");
  case ModbusMaster::ku8MBInvalidSlaveID:
    return String("ku8MBInvalidSlaveID");
  case ModbusMaster::ku8MBInvalidFunction:
    return String("ku8MBInvalidFunction");
  case ModbusMaster::ku8MBResponseTimedOut:
    return String("ku8MBResponseTimedOut");
  case ModbusMaster::ku8MBInvalidCRC:
    return String("ku8MBIllegalDataValue");
  default:
    return String("Unknown");
  }
}


void print_battery_status_to_serial()
{
  //address, name, multiplier to unit, type=kReadFull
  DataToRead to_read[] = {
      BATTERY_PERCENTAGE,
      BATTERY_VOLTAGE,
      TOTAL_CHARGE_CURRENT,
      INTERNAL_TEMPERATURE,
      EXTERNAL_TEMPERATURE,
      GENERATOR_VOLTAGE,
      GENERATOR_CURRENT,
      GENERATOR_POWER,
      SOLAR_VOLTAGE,
      SOLAR_CURRENT,
      SOLAR_POWER,
      MIN_DAILY_VOLTAGE,
      MAX_DAILY_VOLTAGE,
      MAX_DAILY_CHARGE_CURRENT,
      MAX_DAILY_CHARGE_POWER,
      TOTAL_DAILY_CHARGE,
      TOTAL_DAILY_CHARGE2,
      STATUS_BITS1,
      STATUS_BITS2,
      STATUS_BITS3,
      BATTERY_MAX_AH
  };
  
  for (const auto &unit : to_read) {
      // Uses: https://github.com/craftmetrics/esp32-modbusmaster/blob/master/src/ModbusMaster.h

      float data = 0.0f;
      auto response = unit.GetValue(data);
      if (response == ModbusMaster::ku8MBSuccess) {
        Serial.println(unit.GetName() + ": " + String(data));
      } else if (response == ModbusMaster::ku8MBResponseTimedOut) {
        // If we go trough all timeouts, this loop will take way too long
        // Let just cut out losses and do something else
        break;
      }
  }
}

ChargerStatus GetChargerStatus() {
  ChargerStatus out;

  // Return if we get timeout
  #define TEST_TIMEOUT(x) {auto response = (x); if (response == ModbusMaster::ku8MBResponseTimedOut) {return out;}}

  TEST_TIMEOUT(SOLAR_POWER.GetValue(out.solar.power_w));
  TEST_TIMEOUT(SOLAR_CURRENT.GetValue(out.solar.current_a));
  TEST_TIMEOUT(SOLAR_VOLTAGE.GetValue(out.solar.voltage_v));

  TEST_TIMEOUT(GENERATOR_POWER.GetValue(out.alternator.power_w));
  TEST_TIMEOUT(GENERATOR_CURRENT.GetValue(out.alternator.current_a));
  TEST_TIMEOUT(GENERATOR_VOLTAGE.GetValue(out.alternator.voltage_v));

  //TEST_TIMEOUT(GENERATOR_POWER.GetValue(out.battery.power_w));
  TEST_TIMEOUT(TOTAL_CHARGE_CURRENT.GetValue(out.battery.current_a));
  TEST_TIMEOUT(BATTERY_VOLTAGE.GetValue(out.battery.voltage_v));

  float tmp = 0.0f;
  TEST_TIMEOUT(TOTAL_DAILY_CHARGE.GetValue(tmp)); out.total_daily_charge_ah = tmp;
  TEST_TIMEOUT(STATUS_BITS1.GetValue(tmp)); out.status_bits1 = tmp;
  TEST_TIMEOUT(STATUS_BITS2.GetValue(tmp)); out.status_bits2 = tmp;
  TEST_TIMEOUT(STATUS_BITS3.GetValue(tmp)); out.status_bits3 = tmp;
  TEST_TIMEOUT(BATTERY_PERCENTAGE.GetValue(tmp)); out.battery_percentage = tmp;
  TEST_TIMEOUT(EXTERNAL_TEMPERATURE.GetValue(tmp)); out.external_temperature_c = tmp;
   
  return out;
}

#else //if USE_MODBUS
void print_battery_status_to_serial() {}
void setup_modbus() {}

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
#endif
