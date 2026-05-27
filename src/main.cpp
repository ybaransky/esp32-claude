#include <Arduino.h>
#include <math.h>
#include "graph.h"
#include "display.h"
#include "config.h"
#include "web.h"
#include "button.h"
#include "sensors.h"
#include "histogram.h"
#include "i2c_scanner.h"
#include "panel_manager.h"
#include "rtc_ds3231.h"
#include "hardware.h"

constexpr unsigned long READ_INTERVAL_SECONDS = 1;

// ---------------------------------------------------------------------------
// Application state
// ---------------------------------------------------------------------------

struct AppState {
  PanelManager      panelMgr;
  SensorReadings    lastReadings        = {};
  bool              splashShown          = false;
  unsigned long     lastSensorReadMs     = 0;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void updateAllData(const SensorReadings &readings) {
  graphUpdateBounds(readings.deltaF);
  histogramUpdateData(readings.deltaF);
  graphUpdateData(readings.deltaF);
  webUpdate(readings);
  Serial.printf("BMP: %.2f F  SHT: %.2f F  Diff: %+.2f F\n", readings.bmpF, readings.shtF, readings.deltaF);
}

// Returns true if a base-view redraw should be forced.
static bool processButtonEvents(AppState &state, unsigned long now) {
  bool forceRedraw = false;

  if (buttonSplashPending()) {
    buttonClearSplashPending();
    state.panelMgr.setPanel(Panel::SPLASH, state.panelMgr.splashMs, PanelPayload(), now);
  }

  if (buttonMenuPending()) {
    buttonClearMenuPending();
    state.panelMgr.setPanel(Panel::MENU, state.panelMgr.menuMs, PanelPayload(), now);
  }

  if (buttonI2cScanPending()) {
    buttonClearI2cScanPending();
    scanI2C();
    state.panelMgr.setPanel(Panel::I2C_SCAN, state.panelMgr.i2cScanMs, PanelPayload(), now);
  }

  if (buttonRtcStatusPending()) {
    buttonClearRtcStatusPending();
    state.panelMgr.setPanel(Panel::RTC_STATUS, state.panelMgr.rtcStatusMs, PanelPayload(), now);
  }

  if (buttonNetworkInfoPending()) {
    PanelPayload panelData;
    networkGetInfo(panelData.networkSsid, panelData.networkIp);
    buttonClearNetworkInfoPending();
    state.panelMgr.setPanel(Panel::NETWORK_INFO, state.panelMgr.networkInfoMs, panelData, now);
  }

  if (buttonHistogramTogglePending()) {
    buttonClearHistogramTogglePending();
    Panel nextBasePanel = (state.panelMgr.primaryPanel == Panel::GRAPH)
      ? Panel::HISTOGRAM
      : Panel::GRAPH;
    state.panelMgr.setPanel(nextBasePanel, 0, PanelPayload(), now);
    Serial.printf("[BTN1] Base view: %s\n", nextBasePanel == Panel::HISTOGRAM ? "Histogram" : "Graph");
    forceRedraw = true;
  }

  if (buttonPanelDataResetPending()) {
    buttonClearPanelDataResetPending();
    if (state.panelMgr.currentPanel == Panel::GRAPH) {
      graphResetBounds();
      Serial.println("[BTN1] Graph bounds reset");
    } else if (state.panelMgr.currentPanel == Panel::HISTOGRAM) {
      histogramReset();
      Serial.println("[BTN1] Histogram reset");
    } else {
      Serial.println("[BTN1] Reset ignored: not on graph or histogram panel");
    }
    forceRedraw = true;
  }

  return forceRedraw;
}

// Returns true if fresh sensor data was successfully read.
static bool sensorsUpdate(AppState &state, unsigned long now) {
  if (now - state.lastSensorReadMs < READ_INTERVAL_SECONDS * 1000UL) {
    return false;
  }

  state.lastReadings    = readSensors();
  state.lastSensorReadMs = now;

  bool bmpError = isnan(state.lastReadings.bmpF);
  bool shtError = isnan(state.lastReadings.shtF);

  if (bmpError || shtError) {
    String nextError = (bmpError && shtError) ? "BMP and SHT"
                     : bmpError               ? "BMP"
                                              : "SHT";
    PanelPayload panelData;
    panelData.errorMessage = nextError;
    state.panelMgr.setPanel(Panel::ERROR_MESSAGE, state.panelMgr.errorMessageMs, panelData, now);
    return false;
  }

  updateAllData(state.lastReadings);
  return true;
}

static void renderFrame(AppState &state, bool freshData, bool forceRedraw, unsigned long now) {
  state.panelMgr.checkExpiration(now);

  U8G2 &display = displayDevice();

  if (state.panelMgr.currentPanel == Panel::GRAPH) {
    if (state.panelMgr.currentPanel != state.panelMgr.prevPanel || freshData || forceRedraw) {
      showGraph(display, state.lastReadings);
    }
  } else if (state.panelMgr.currentPanel == Panel::HISTOGRAM) {
    if (state.panelMgr.currentPanel != state.panelMgr.prevPanel || freshData || forceRedraw) {
      showHistogram(display, state.lastReadings);
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
    RtcStatus status = rtcGetStatus();
    Serial.printf("[RTC] Init failed: %s\n", status.error.c_str());
  }
  // all hardware started
  scanI2C();

  ApConfig cfg = loadApConfig();
  webBegin(cfg.ssid.c_str(), cfg.password.c_str());

  buttonBegin();
}

void loop() {
  static AppState state;
  unsigned long now = millis();

  if (!state.splashShown) {
    state.panelMgr.setPanel(Panel::SPLASH, state.panelMgr.splashMs, PanelPayload(), now);
    state.splashShown = true;
  }

  buttonTick();
  rtcTick();
  webHandleClients();

  bool forceRedraw = processButtonEvents(state, now);
  bool freshData   = sensorsUpdate(state, now);
  renderFrame(state, freshData, forceRedraw, now);
}
