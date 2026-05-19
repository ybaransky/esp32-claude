#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "graph.h"
#include "config.h"
#include "web.h"
#include "button.h"
#include "sensors.h"
#include "i2c_scanner.h"

constexpr uint8_t I2C_SDA_PIN = 22;
constexpr uint8_t I2C_SCL_PIN = 21;

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, I2C_SCL_PIN, I2C_SDA_PIN);


constexpr unsigned long READ_INTERVAL_SECONDS = 2;

void displayValues(const SensorReadings &readings) {
  // readings has a legal value
  updateDataBounds(readings.deltaF);
  pushGraphHistory(readings.deltaF);
  webUpdate(readings);

  Serial.printf("BMP: %.2f F  SHT: %.2f F  Diff: %+.2f F\n", readings.bmpF, readings.shtF, readings.deltaF);
  showGraph(u8g2, readings);
}

void setup() {
  Serial.begin(115200);

  // u8g2.begin() initializes Wire using the pins passed in the constructor.
  u8g2.begin();

  sensorsInit(I2C_SDA_PIN, I2C_SCL_PIN);

  ApConfig cfg = loadApConfig();
  webBegin(cfg.ssid.c_str(), cfg.password.c_str());

  buttonSetup();
//  scanI2C();
}

void loop() {
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();

  buttonTick();
  webHandleClients();

  if (now - lastUpdate >= (READ_INTERVAL_SECONDS * 1000UL)) {
    SensorReadings const& readings = readSensors();
    displayValues(readings);
    lastUpdate = now;
  }
}
