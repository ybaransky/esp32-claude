#include "display.h"

#include "hardware.h"

namespace {
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
  U8G2_R0,
  U8X8_PIN_NONE,
  Hardware::Pins::I2C_SCL,
  Hardware::Pins::I2C_SDA
);
}

void displayBegin() {
  // u8g2.begin() initializes Wire using the pins passed in the constructor.
  display.begin();
}

U8G2 &displayDevice() {
  return display;
}
