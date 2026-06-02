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

ESP32 project using PlatformIO and the Arduino framework. Reads temperature from two I2C sensors every second, displays a scrolling difference graph or histogram on an OLED screen, serves a live sensor web page over a WiFi access point with a captive portal, and supports two physical buttons for panel navigation and data controls. A DS3231 RTC provides a real-time clock with 1 Hz SQW interrupt.

## Hardware

| Device | Type | I2C Address |
|--------|------|-------------|
| SH1106 | 1.3" OLED display (128x64) | 0x3C |
| BMP280 | Barometric pressure + temperature sensor | 0x76 |
| SHT31  | Temperature + humidity sensor | 0x44 |
| DS3231 | Real-time clock module | 0x68 |

All devices share the same I2C bus with non-default pins: **SDA** → GPIO 22, **SCL** → GPIO 21.

### GPIO Pin Assignments

| Pin | Function |
|-----|----------|
| GPIO 22 | I2C SDA |
| GPIO 21 | I2C SCL |
| GPIO 19 | Button 1 |
| GPIO 23 | Button 2 |
| GPIO 2  | Internal LED |
| GPIO 4  | RTC SQW interrupt |

## Libraries

- `olikraus/U8g2` — OLED display driver; its `begin()` initializes Wire with the custom pins passed to the constructor
- `adafruit/Adafruit BMP280 Library` — BMP280 driver
- `adafruit/Adafruit SHT31 Library` — SHT31 driver
- `adafruit/Adafruit Unified Sensor` — required dependency for Adafruit sensor libs
- `adafruit/RTClib` — DS3231 RTC driver
- `mathertel/OneButton` — debounced button handling (single-click, double-click, long-press)
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
| `src/main.cpp` | App setup and main loop orchestration: ticks buttons/RTC/web, schedules sensor reads, renders frames |
| `src/hardware.h/.cpp` | Pin/address constants, I2C bus scanner |
| `src/sensors.h/.cpp` | BMP280 + SHT31 init and read |
| `src/graph.h/.cpp` | Scrolling temperature-delta graph: data bounds, history buffer, rendering |
| `src/histogram.h/.cpp` | Temperature-delta frequency histogram: bins, centering, rendering |
| `src/display.h/.cpp` | U8G2 display init (`displayBegin()`, `displayDevice()`) |
| `src/panel_manager.h/.cpp` | Panel lifecycle: activation, priority queue, expiry, render dispatch |
| `src/button.h/.cpp` | Two-button input: debounce, click/long-press → `ButtonEvent` queue |
| `src/button_event_handler.h/.cpp` | Button event dispatch: maps queued `ButtonEvent`s to panel transitions, graph/histogram commands, I2C scan, network info, and RTC status |
| `src/rtc_ds3231.h/.cpp` | DS3231 RTC init, 1 Hz SQW ISR, tick, status, time string |
| `src/config.h/.cpp` | LittleFS mount + `config.json` parsing |
| `src/web.h/.cpp` | WiFi AP, DNS server, HTTP server, captive portal |
| `src/html.h/.cpp` | HTML/JS dashboard page generation |

### Panels (`enum class Panel`)

| Panel | Trigger | Duration |
|-------|---------|----------|
| `SPLASH` | On boot | ~3 s |
| `GRAPH` | Default / Button toggle | Persistent |
| `HISTOGRAM` | Button toggle | Persistent |
| `MENU` | Button 2 short press | ~3 s |
| `NETWORK_INFO` | Button event | ~5 s |
| `I2C_SCAN` | Button event | ~5 s |
| `RTC_STATUS` | Button event | ~5 s |
| `ERROR_MESSAGE` | Sensor read failure | ~5 s |

### Data flow per loop iteration
1. `buttonTick()` — debounce buttons, enqueue `ButtonEvent`s
2. `rtcTick()` — service 1 Hz SQW interrupt
3. `webHandleClients()` — process pending HTTP requests
4. `processButtonEvents()` — dispatch queued events via `button_event_handler` → panel transitions or data resets
5. `sensorsUpdate()` — every 1 s: read BMP280 + SHT31, compute `deltaF = bmpF - shtF`; on success call `updateAllData()` (graph, histogram, web); on error show `ERROR_MESSAGE` panel
6. `renderFrame()` — check panel expiry, redraw if panel changed, fresh data, or forced

### Graph rendering (`src/graph.cpp`)
- Lifetime min/max of `deltaF` with 5% padding drive Y-axis; falls back to ±0.1 before any data
- `GRAPH_LEFT = 24`, `GRAPH_TOP = 2`, `GRAPH_BOTTOM = 45` — pixel boundaries
- `GRAPH_WIDTH = 104` — rolling history buffer size (one sample per pixel column)
- Status line at y=55: `BMP=XX.XX SHT=XX.XX`; min/max line at y=63

### Histogram (`src/histogram.cpp`)
- Fixed-width bins at 0.01 °F resolution; ±5.00 °F range (1001 bins) centered on first sample
- `histogramRecenterOnPeak()` recenters the view on the most-frequent bin
- `histogramReset()` clears all bins and sample count

### Captive portal (`src/web.cpp`)
`webBegin()` polls until `WiFi.softAPIP()` is non-zero before starting the DNS server — calling `dnsServer.start()` with `0.0.0.0` breaks IP connectivity.
