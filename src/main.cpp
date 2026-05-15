#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_SHT31.h>
#include "graph.h"
#include "config.h"
#include "web.h"

constexpr uint8_t I2C_SDA_PIN = 22;
constexpr uint8_t I2C_SCL_PIN = 21;

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, I2C_SCL_PIN, I2C_SDA_PIN);
Adafruit_BMP280 bme;
Adafruit_SHT31  sht31;

unsigned long lastRead    = 0;
unsigned long startupTime = 0;
int readCount = 0;

constexpr unsigned long READ_INTERVAL_SECONDS = 2;

void scanI2C() {
  Serial.println("Scanning I2C bus...");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Found device at 0x%02X\n", addr);
      found++;
    }
  }
  Serial.printf("Scan complete. %d device(s) found.\n\n", found);
}

void readAndDisplay() {
  readCount++;
  if (readCount == 1 || (readCount % 10) == 0) {
    scanI2C();
  }

  float bmpF  = bme.readTemperature()   * 9.0f / 5.0f + 32.0f;
  float shtF  = sht31.readTemperature() * 9.0f / 5.0f + 32.0f;
  float diffF = bmpF - shtF;

  updateLifetimeDiff(diffF);
  pushDiffHistory(diffF);
  webUpdate({bmpF, shtF, diffF});

  unsigned long elapsedSeconds = (millis() - startupTime) / 1000UL;
  Serial.printf("Read #%d - BMP: %.2f F  SHT: %.2f F  Diff: %+.2f F\n", readCount, bmpF, shtF, diffF);
  showGraph(u8g2, bmpF, shtF, diffF, elapsedSeconds);
}

void setup() {
  Serial.begin(115200);

  // u8g2.begin() initializes Wire using the pins passed in the constructor.
  u8g2.begin();

  bme.begin(0x76);
  sht31.begin(0x44);

  ApConfig cfg = loadApConfig();
  webBegin(cfg.ssid.c_str(), cfg.password.c_str());

  startupTime = millis();
  readAndDisplay();
  lastRead = millis();
}

void loop() {
  webHandleClients();

  if (millis() - lastRead >= (READ_INTERVAL_SECONDS * 1000UL)) {
    lastRead = millis();
    readAndDisplay();
  }
}
