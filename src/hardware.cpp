#include "hardware.h"
#include <Wire.h>
#include <Arduino.h>

static uint8_t lastScanAddresses[126];
static size_t  lastScanCount = 0;

void i2cScan() {
  Serial.println("Scanning I2C bus...");
  size_t found = 0;
  lastScanCount = 0;

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Found device at 0x%02X\n", addr);
      if (found < sizeof(lastScanAddresses)) {
        lastScanAddresses[found] = addr;
      }
      found++;
    }
  }
  lastScanCount = found;
  Serial.printf("Scan complete. %u device(s) found.\n\n", static_cast<unsigned>(found));
}

I2CScanResult i2cGetLastScanResult() {
  return {lastScanAddresses, lastScanCount};
}
