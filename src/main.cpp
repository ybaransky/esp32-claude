#include <Arduino.h>
#include "display.h"
#include "graph.h"
#include "config.h"
#include "web.h"
#include "button.h"
#include "button_event_handler.h"
#include "sensors.h"
#include "histogram.h"
#include "panel_manager.h"
#include "rtc_ds3231.h"
#include "hardware.h"

constexpr unsigned long READ_INTERVAL_MS = 1000;

// ---------------------------------------------------------------------------
// Application state
// ---------------------------------------------------------------------------

struct AppState {
  PanelManager  panelMgr;
  SensorReadings lastReadings       = {};
  bool           splashShown        = false;
  unsigned long  lastSensorReadMs   = 0;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void updateAllData(const SensorReadings &readings) {
  graphUpdateBounds(readings.deltaF);
  histogramUpdateData(readings.deltaF);
  graphUpdateData(readings.deltaF);
  webUpdate(readings);
  Serial.printf("BMP: %.2f F  SHT: %.2f F  Diff: %+.2f F\n",
                readings.bmpF, readings.shtF, readings.deltaF);
}

static const char* describeSensorFailure(bool bmpError, bool shtError) {
  if (bmpError && shtError) return "BMP and SHT";
  if (bmpError) return "BMP";
  return "SHT";
}

// Returns true if fresh sensor data was successfully read.
static bool sensorsUpdate(AppState &state, unsigned long now) {
  if (now - state.lastSensorReadMs < READ_INTERVAL_MS) return false;

  state.lastReadings    = readSensors();
  state.lastSensorReadMs = now;

  bool bmpError = isnan(state.lastReadings.bmpF);
  bool shtError = isnan(state.lastReadings.shtF);

  if (bmpError || shtError) {
    state.panelMgr.setPanel(Panel::ERROR_MESSAGE, PanelPayload::error(describeSensorFailure(bmpError, shtError)), now);
    return false;
  }

  updateAllData(state.lastReadings);
  return true;
}

static void renderFrame(AppState &state, bool freshData, bool forceRedraw, unsigned long now) {
  state.panelMgr.checkExpiration(now);

  U8G2 &display = displayDevice();
  Panel current = state.panelMgr.getCurrentPanel();

  if (current == Panel::GRAPH || current == Panel::HISTOGRAM) {
    if (state.panelMgr.panelChanged() || freshData || forceRedraw) {
      if (current == Panel::GRAPH) showGraph(display, state.lastReadings);
      else                         showHistogram(display, state.lastReadings);
    }
  } else {
    state.panelMgr.render(display, now);
  }
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[SETUP] Starting up...");

  displayBegin();
  sensorsBegin(Hardware::Pins::I2C_SDA, Hardware::Pins::I2C_SCL);

  if (!rtcBegin()) {
    Serial.printf("[RTC] Init failed: %s\n", rtcGetStatus().error.c_str());
  }
  i2cScan();

  ApConfig cfg = loadApConfig();
  webBegin(cfg.ssid.c_str(), cfg.password.c_str());

  buttonBegin();
}

void loop() {
  static AppState state;
  unsigned long now = millis();

  if (!state.splashShown) {
    state.panelMgr.setPanel(Panel::SPLASH, PanelPayload{}, now);
    state.splashShown = true;
  }

  buttonTick();
  buttonLedTick(now);
  rtcTick();
  webHandleClients();

  bool forceRedraw = processButtonEvents(state.panelMgr, now);
  bool freshData   = sensorsUpdate(state, now);
  renderFrame(state, freshData, forceRedraw, now);
}
