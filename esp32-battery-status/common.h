#pragma once

struct ComponentStatus {
  float power_w{0.0f};
  float current_a{0.0f};
  float voltage_v{0.0f};
};

struct ChargerStatus {
  ComponentStatus solar;
  ComponentStatus alternator;
  ComponentStatus battery;
  uint16_t total_daily_charge_ah{0};
  uint16_t status_bits1{0};
  uint16_t status_bits2{0};
  uint16_t status_bits3{0};
  uint8_t battery_percentage{0};
};
