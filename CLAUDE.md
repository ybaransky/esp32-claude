# CLAUDE.md

You are a senior software engineer with 15+ years of experience. When providing code solutions, follow these principles:

## DESIGN PRINCIPLES:
- Apply SOLID principles strictly (Single Responsibility, Open/Closed, Liskov, Interface Segregation, Dependency Inversion)
- Minimize coupling between classes/modules — prefer dependency injection over hard dependencies
- Favor composition over inheritance
- Use clear abstractions and interfaces to separate concerns

## CODE READABILITY:
- Write self-documenting code: meaningful variable/function/class names that reveal intent
- Keep functions small and focused (do one thing)
- Avoid deep nesting — use early returns and guard clauses
- Add concise comments only where the "why" isn't obvious from the code

## ARCHITECTURE:
- Separate concerns into distinct layers (e.g. data, logic, presentation)
- Define clear boundaries between modules
- Avoid leaky abstractions
- Prefer explicit over implicit

## OUTPUT FORMAT:
- Before writing code, briefly explain the design decisions and tradeoffs
- After the code, note any further improvements worth considering
- If the task is large, outline the structure first and confirm before implementing
## Project Overview

ESP32 project using PlatformIO and the Arduino framework. Reads temperature from two I2C sensors every 2 seconds, displays a scrolling difference graph on an OLED screen, and serves a live sensor web page over a WiFi access point with a captive portal.

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
- `bblanchon/ArduinoJson` — JSON parsing for `config.json`
- `WiFi`, `WebServer`, `DNSServer` — built into the ESP32 Arduino framework; no extra lib_deps entry needed

## Build & Flash

```
pio run -t upload        # compile and flash firmware
pio run -t uploadfs      # upload the data/ directory to LittleFS (run once, or after editing config.json)
pio device monitor       # serial monitor at 115200 baud
```

Upload speed: **921600**. Serial monitor baud rate: **115200**.

## AP Config

`data/config.json` is uploaded to LittleFS and read at boot. If the file is missing or unparseable, defaults are used silently.

```json
{ "ssid": "ESP32-Sensor", "password": "12345678" }
```

The AP always comes up at **192.168.4.1**. A captive portal (DNS wildcard + HTTP redirect) causes phones to open the page automatically on connect.

## Web API

All endpoints return JSON. The page at `/` polls `/api/sensors` every 2 seconds via `fetch`.

| Route | Response |
|-------|----------|
| `GET /` | HTML dashboard |
| `GET /api/sensors` | `{"bmp":F,"sht":F,"diff":F}` |
| `GET /api/bmp` | `{"bmp":F}` |
| `GET /api/sht` | `{"sht":F}` |
| `GET /api/diff` | `{"diff":F}` |
| any other path | 302 → `http://192.168.4.1/` |

Every request is logged to Serial: `[HTTP] METHOD /path <- client-ip  => status`.

## Architecture

### Source files

| File | Responsibility |
|------|---------------|
| `src/main.cpp` | Hardware init, sensor read loop, I2C scanning |
| `src/graph.h/.cpp` | OLED graph state and rendering |
| `src/config.h/.cpp` | LittleFS mount + `config.json` parsing |
| `src/web.h/.cpp` | WiFi AP, DNS server, HTTP server, captive portal |

### Data flow per read cycle (`readAndDisplay()`)
1. Optionally scan I2C bus (read #1 and every 10th read)
2. Read both sensors, compute `diffF = bmpF - shtF`
3. `updateLifetimeDiff()` — tracks all-time min/max of `diffF` for Y-axis scaling
4. `pushDiffHistory()` — appends to circular float buffer sized to graph pixel width (104px)
5. `webUpdate()` — stores latest readings for HTTP handlers
6. `showGraph()` — renders full OLED display frame

### Graph rendering (`src/graph.cpp`)
- `getDiffMinMax()` returns lifetime min/max with 5% padding; falls back to ±0.1 if no data yet
- `diffToGraphY()` maps a float to a pixel row via linear normalization
- Y-axis labels use `%4.2f` or `%5.2f` depending on whether any value is negative; the graph area shifts right 6px for negative labels to avoid clipping
- Status line at bottom: `BMP=XX.XX SHT=XX.XX` on the left, elapsed seconds on the right

### Key constants (`src/graph.h`)
- `GRAPH_LEFT = 24`, `GRAPH_TOP = 2`, `GRAPH_BOTTOM = 51` — pixel boundaries of the graph area
- `GRAPH_WIDTH = 104` — also the `diffHistory` array size (one sample per pixel column)

### Captive portal (`src/web.cpp`)
`webBegin()` polls until `WiFi.softAPIP()` is non-zero before starting the DNS server — calling `dnsServer.start()` with `0.0.0.0` breaks IP connectivity.
