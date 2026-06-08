#include "sensors.h"

#include "hardware.h"
#include <Adafruit_BMP280.h>
#include <Adafruit_SHT31.h>
#include <Wire.h>

class SensorManager {
public:
  void begin() {
    // Wire is already initialized by u8g2.begin().
    bmp.begin(Hardware::I2CAddress::BMP280);
    sht31.begin(Hardware::I2CAddress::SHT31);
  }

  const SensorReadings &read(bool force) {
    const unsigned long now = millis();
    if (force || now - readings.readTime >= MIN_READ_INTERVAL_MS) {
      readings.bmpF = toFahrenheit(bmp.readTemperature());
      readings.shtF = toFahrenheit(sht31.readTemperature());
      readings.deltaF = readings.bmpF - readings.shtF;
      readings.readTime = now;
    }
    return readings;
  }

private:
  static constexpr unsigned long MIN_READ_INTERVAL_MS = 500;

  static float toFahrenheit(float celsius) {
    return celsius * 9.0f / 5.0f + 32.0f;
  }

  Adafruit_BMP280 bmp;
  Adafruit_SHT31  sht31;
  SensorReadings  readings = {0.0f, 0.0f, 0.0f, 0};
};

static SensorManager sensorManager;

void sensorsBegin() {
  sensorManager.begin();
}

const SensorReadings &readSensors(bool force) {
  return sensorManager.read(force);
}
