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
};
