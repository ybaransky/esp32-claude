# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32 project using PlatformIO and the Arduino framework. Reads temperature from two I2C sensors every 2 seconds and displays a scrolling difference graph on an OLED screen.

## Hardware

| Device | Type | I2C Address |
|--------|------|-------------|
| SH1106 | 1.3" OLED display (128x64) | 0x3C |
| BMP280 | Barometric pressure + temperature sensor | 0x76 |
| SHT31  | Temperature + humidity sensor | 0x44 |

All three devices share the same I2C bus with non-default pins: **SDA** → GPIO 22, **SCL** → GPIO 21.

## Libraries

- `olikraus/U8g2` — OLED display driver; its `begin()` initializes Wire with the custom pins passed to the constructor
- `adafruit/Adafruit BMP280 Library` — BMP280 driver
- `adafruit/Adafruit SHT31 Library` — SHT31 driver
- `adafruit/Adafruit Unified Sensor` — required dependency for Adafruit sensor libs

## Build & Flash

```
pio run -t upload        # compile and flash
pio device monitor       # serial monitor at 115200 baud
```

Upload speed: **921600**. Serial monitor baud rate: **115200**.

## Architecture

All code lives in `src/main.cpp`. The display renders a scrolling graph of the temperature difference (BMP280 − SHT31 in °F).

**Data flow per read cycle** (`readAndDisplay()`):
1. Optionally scan I2C bus (read #1 and every 10th read)
2. Read both sensors, compute `diffF = bmpF - shtF`
3. `updateLifetimeDiff()` — tracks all-time min/max of `diffF` for Y-axis scaling
4. `pushDiffHistory()` — appends to circular float buffer sized to graph pixel width (104px)
5. `showGraph()` — renders full display frame

**Graph rendering** (`showGraph()`):
- `getDiffMinMax()` returns the lifetime min/max with 5% padding; falls back to ±0.1 if no data
- `diffToGraphY()` maps a float value to a pixel row via linear normalization
- Y-axis labels (max, current diff, min) use `%4.2f` or `%5.2f` depending on whether any value is negative; the graph area shifts right 6px when negative labels are present to avoid clipping
- Status line at bottom: `BMP=XX.XX SHT=XX.XX` on the left, elapsed seconds on the right

**Key constants** (all in `src/main.cpp`):
- `READ_INTERVAL_SECONDS = 2`
- `GRAPH_LEFT = 24`, `GRAPH_TOP = 2`, `GRAPH_BOTTOM = 51` — pixel boundaries of the graph area
- `GRAPH_WIDTH = 104` — also the `diffHistory` array size (one sample per pixel column)
