#include "i2c_scanner.h"
#include <Wire.h>

static String lastScanAddresses = "None";

void scanI2C() {
  Serial.println("Scanning I2C bus...");
  int found = 0;
  lastScanAddresses = "";

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Found device at 0x%02X\n", addr);
      if (found > 0) {
        lastScanAddresses += " ";
      }
      char addrBuf[8];
      snprintf(addrBuf, sizeof(addrBuf), "0x%02X", addr);
      lastScanAddresses += addrBuf;
      found++;
    }
  }
  if (found == 0) {
    lastScanAddresses = "None";
  }
  Serial.printf("Scan complete. %d device(s) found.\n\n", found);
}

const String& i2cGetLastScanAddresses() {
  return lastScanAddresses;
}
