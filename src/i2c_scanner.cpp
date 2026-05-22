#include "i2c_scanner.h"
#include <Wire.h>

static uint8_t lastScanAddressArray[126];
static size_t lastScanAddressCount = 0;

void scanI2C() {
  Serial.println("Scanning I2C bus...");
  size_t found = 0;
  lastScanAddressCount = 0;

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Found device at 0x%02X\n", addr);
      if (found < (sizeof(lastScanAddressArray) / sizeof(lastScanAddressArray[0]))) {
        lastScanAddressArray[found] = addr;
      }
      found++;
    }
  }
  lastScanAddressCount = found;
  Serial.printf("Scan complete. %u device(s) found.\n\n", static_cast<unsigned>(found));
}

I2CScanResult i2cGetLastScanResult() {
  return {lastScanAddressArray, lastScanAddressCount};
}
