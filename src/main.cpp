#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_SHT31.h>

// Both devices share I2C bus: SCL=21, SDA=22
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 21, 22);
Adafruit_BMP280 bme;
Adafruit_SHT31 sht31;

unsigned long lastRead = 0;
int readCount = 0;

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

void showTemps(float bmpF, float shtF, int count) {
  char bmpBuf[20];
  char shtBuf[20];
  char countBuf[16];
  snprintf(bmpBuf,   sizeof(bmpBuf),   "BMP: %.2f F", bmpF);
  snprintf(shtBuf,   sizeof(shtBuf),   "SHT: %.2f F", shtF);
  snprintf(countBuf, sizeof(countBuf), "Reads: %d",   count);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(0, 14, bmpBuf);
  u8g2.drawStr(0, 34, shtBuf);
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 52, countBuf);
  u8g2.sendBuffer();
}

void readAndDisplay() {
  scanI2C();
  readCount++;
  float bmpF = bme.readTemperature()   * 9.0f / 5.0f + 32.0f;
  float shtF = sht31.readTemperature() * 9.0f / 5.0f + 32.0f;
  Serial.printf("Read #%d — BMP: %.2f F  SHT: %.2f F\n", readCount, bmpF, shtF);
  showTemps(bmpF, shtF, readCount);
}

void setup() {
  Serial.begin(115200);

  // u8g2.begin() initializes Wire with SCL=21, SDA=22
  u8g2.begin();

  // BMP280 default address is 0x76; use 0x77 if sensor not found
  bme.begin(0x76);
  sht31.begin(0x44);

  readAndDisplay();
  lastRead = millis();
}

void loop() {
  if (millis() - lastRead >= 10000) {
    lastRead = millis();
    readAndDisplay();
  }
}
