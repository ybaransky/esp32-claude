#include "sensors.h"
#include "i2c_scanner.h"
#include "hardware.h"
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_SHT31.h>

Adafruit_BMP280 bme;
Adafruit_SHT31  sht31;
static SensorReadings currentReadings = {0.0f, 0.0f, 0.0f, 0};

void sensorsBegin(uint8_t sdaPin, uint8_t sclPin) {
  // Wire is already initialized by u8g2.begin()
  bme.begin(Hardware::I2CAddress::BMP280);
  sht31.begin(Hardware::I2CAddress::SHT31);
}

const SensorReadings& readSensors(bool force) {
  unsigned long now = millis();
  if (force || now - currentReadings.readTime >= 500) {
    currentReadings.bmpF = bme.readTemperature()   * 9.0f / 5.0f + 32.0f;
    currentReadings.shtF = sht31.readTemperature() * 9.0f / 5.0f + 32.0f;
    currentReadings.deltaF = currentReadings.bmpF - currentReadings.shtF;
    currentReadings.readTime = now;
  }
  return currentReadings;
}
