#pragma once
#include <Arduino.h>

struct I2CScanResult {
	const uint8_t *addresses;
	size_t count;
};

void scanI2C();
I2CScanResult i2cGetLastScanResult();
