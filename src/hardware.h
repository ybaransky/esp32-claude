#pragma once

#include <Arduino.h>

namespace Hardware {
	namespace Pins {
		constexpr uint8_t I2C_SDA = 22;
		constexpr uint8_t I2C_SCL = 21;

		constexpr uint8_t BUTTON_1 = 19;
		constexpr uint8_t BUTTON_2 = 23;
		constexpr uint8_t INTERNAL_LED = 2;
		constexpr uint8_t RTC_SQW = 4;
	}  // namespace Pins

	namespace I2CAddress {
		constexpr uint8_t OLED_SH1106 = 0x3C;
		constexpr uint8_t BMP280 = 0x76;
		constexpr uint8_t SHT31 = 0x44;
		constexpr uint8_t DS3231 = 0x68;
	}  // namespace I2CAddress
}  // namespace Hardware

// ---------------------------------------------------------------------------
// I2C bus scanner
// ---------------------------------------------------------------------------

struct I2CScanResult {
	const uint8_t *addresses;
	size_t count;
};

void          i2cScan();
I2CScanResult i2cGetLastScanResult();