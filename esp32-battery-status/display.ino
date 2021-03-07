#include "display.h"

#if USE_DISPLAY

#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
TFT_eSPI tft = TFT_eSPI();

void setup_display() {
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);  // Adding a black background colour erases previous text automatically
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

#else //if USE_DISPLAY
void setup_display() {}
void DrawStatus(const ChargerStatus &status) {}
#endif //if USE_DISPLAY
