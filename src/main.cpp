#include <Arduino.h>
#include <math.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "graph.h"
#include "display.h"
#include "config.h"
#include "web.h"
#include "button.h"
#include "sensors.h"
#include "i2c_scanner.h"
#include "panel_manager.h"
#include "rtc_ds3231.h"
#include "hardware.h"

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, Hardware::Pins::I2C_SCL, Hardware::Pins::I2C_SDA);

constexpr unsigned long READ_INTERVAL_SECONDS = 1;

void updateGraphData(const SensorReadings &readings) {
  updateDataBounds(readings.deltaF);
  pushGraphHistory(readings.deltaF);
  webUpdate(readings);
  Serial.printf("BMP: %.2f F  SHT: %.2f F  Diff: %+.2f F\n", readings.bmpF, readings.shtF, readings.deltaF);
}

void setup() {
  Serial.begin(115200);

  // u8g2.begin() initializes Wire using the pins passed in the constructor.
  u8g2.begin();

  sensorsInit(Hardware::Pins::I2C_SDA, Hardware::Pins::I2C_SCL);

  if (!rtcBegin()) {
    RtcStatus status = rtcGetStatus();
    Serial.printf("[RTC] Init failed: %s\n", status.error.c_str());
  }

  ApConfig cfg = loadApConfig();
  webBegin(cfg.ssid.c_str(), cfg.password.c_str());

  buttonSetup();
//  scanI2C();
}

void loop() {
  static PanelManager  panelMgr;
  static bool histogramViewEnabled = false;
  static bool splashShown = false;
  static unsigned long lastSensorReading  = 0;
  static SensorReadings lastReadings      = {};

  unsigned long now = millis();

  if (!splashShown) {
    // Show splash, then network, I2C scan, and RTC panels in sequence
    panelMgr.setPanel(Panel::SPLASH, panelMgr.splashMs, PanelPayload(), now);
/*
    PanelPayload networkData;
    networkGetInfo(networkData.networkSsid, networkData.networkIp);
    panelMgr.enqueuePanelBack(Panel::NETWORK_INFO, panelMgr.networkInfoMs, networkData);
    scanI2C();
    panelMgr.enqueuePanelBack(Panel::I2C_SCAN, panelMgr.i2cScanMs, PanelPayload());
    panelMgr.enqueuePanelBack(Panel::RTC_STATUS, panelMgr.rtcStatusMs, PanelPayload());
*/

    splashShown = true;
  }

  buttonTick();
  rtcHandle();
  webHandleClients();

  if (buttonSplashPending()) {
    buttonClearSplashPending();
    panelMgr.setPanel(Panel::SPLASH, panelMgr.splashMs, PanelPayload(), now);
  }

  if (buttonMenuPending()) {
    buttonClearMenuPending();
    panelMgr.setPanel(Panel::MENU, panelMgr.menuMs, PanelPayload(), now);
  }

  if (buttonI2cScanPending()) {
    buttonClearI2cScanPending();
    scanI2C();
    panelMgr.setPanel(Panel::I2C_SCAN, panelMgr.i2cScanMs, PanelPayload(), now);
  }

  if (buttonRtcStatusPending()) {
    buttonClearRtcStatusPending();
    panelMgr.setPanel(Panel::RTC_STATUS, panelMgr.rtcStatusMs, PanelPayload(), now);
  }

  if (buttonNetworkInfoPending()) {
    PanelPayload panelData;
    networkGetInfo(panelData.networkSsid, panelData.networkIp);
    buttonClearNetworkInfoPending();
    panelMgr.setPanel(Panel::NETWORK_INFO, panelMgr.networkInfoMs, panelData, now);
  }

  bool forceBaseRedraw = false;
  if (buttonHistogramTogglePending()) {
    buttonClearHistogramTogglePending();
    histogramViewEnabled = !histogramViewEnabled;
    Serial.printf("[BTN1] Base view: %s\n", histogramViewEnabled ? "Histogram" : "Graph");
    forceBaseRedraw = true;
  }

  if (buttonGraphResetStyle1Pending()) {
    buttonClearGraphResetStyle1Pending();
    resetGraphAndHistogram();
    Serial.println("[BTN1] Full reset: graph bounds/history + histogram");
    forceBaseRedraw = true;
  }

  // Always update graph data on schedule, regardless of what's on screen.
  bool freshData = false;
  if (now - lastSensorReading >= (READ_INTERVAL_SECONDS * 1000UL)) {
    lastReadings = readSensors();
    bool bmpError = isnan(lastReadings.bmpF);
    bool shtError = isnan(lastReadings.shtF);

    if (bmpError || shtError) {
      String nextError;
      if (bmpError && shtError) {
        nextError = "BMP and SHT";
      } else if (bmpError) {
        nextError = "BMP";
      } else {
        nextError = "SHT";
      }

      PanelPayload panelData;
      panelData.errorMessage = nextError;
      panelMgr.setPanel(Panel::ERROR_MESSAGE, panelMgr.errorMessageMs, panelData, now);
    } else {
      updateGraphData(lastReadings);
      freshData = true;
    }

    lastSensorReading = now;
  }

  // Check panel expiration
  panelMgr.checkExpiration(now);

  // Render graph or delegate to panel manager
  if (panelMgr.currentPanel == Panel::GRAPH) {
    if (panelMgr.currentPanel != panelMgr.prevPanel || freshData || forceBaseRedraw) {
      if (histogramViewEnabled) {
        showHistogram(u8g2, lastReadings);
      } else {
        showGraph(u8g2, lastReadings);
      }
    }
  } else {
    panelMgr.render(u8g2, now);
  }
}
