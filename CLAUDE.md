# esp32-claude

ESP32 project using PlatformIO and the Arduino framework. Reads temperature from two I2C sensors every 10 seconds and displays both readings on an OLED screen.

## Hardware

| Device | Type | I2C Address |
|--------|------|-------------|
| SH1106 | 1.3" OLED display (128x64) | 0x3C |
| BMP280 | Barometric pressure + temperature sensor | 0x76 |
| SHT31  | Temperature + humidity sensor | 0x44 |

### I2C Wiring
All three devices share the same I2C bus with non-default pins:
- **SCL** → GPIO 21
- **SDA** → GPIO 22

## Libraries

- `olikraus/U8g2` — OLED display driver (handles Wire init with custom pins)
- `adafruit/Adafruit BMP280 Library` — BMP280 sensor driver
- `adafruit/Adafruit SHT31 Library` — SHT31 sensor driver
- `adafruit/Adafruit Unified Sensor` — dependency for Adafruit sensor libraries

## Behavior

- On startup: scans the I2C bus and prints found addresses to Serial, then takes an initial sensor reading
- Every 10 seconds: rescans the I2C bus, reads temperature from both sensors, and updates the display
- OLED shows BMP280 temp, SHT31 temp (both in °F to 2 decimal places), and a running read count
- Serial monitor logs each read with both temperatures

## Build & Flash

```
pio run -t upload
pio device monitor
```

Serial monitor baud rate: **115200**. Upload speed: **921600**.
