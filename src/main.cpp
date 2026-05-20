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

constexpr uint8_t I2C_SDA_PIN = 22;
constexpr uint8_t I2C_SCL_PIN = 21;

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, I2C_SCL_PIN, I2C_SDA_PIN);

constexpr unsigned long READ_INTERVAL_SECONDS = 2;

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

  sensorsInit(I2C_SDA_PIN, I2C_SCL_PIN);

  ApConfig cfg = loadApConfig();
  webBegin(cfg.ssid.c_str(), cfg.password.c_str());

  buttonSetup();
//  scanI2C();
}

void loop() {
  static PanelManager  panelMgr;
  static bool splashShown = false;
  static unsigned long lastSensorReading  = 0;
  static SensorReadings lastReadings      = {};

  unsigned long now = millis();

  if (!splashShown) {
    panelMgr.setPanel(Panel::SPLASH, panelMgr.splashMs, PanelPayload(), now);
    splashShown = true;
  }

  buttonTick();
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
    PanelPayload panelData;
    panelData.i2cAddresses = i2cGetLastScanAddresses();
    buttonClearI2cScanPending();
    panelMgr.setPanel(Panel::I2C_SCAN, panelMgr.i2cScanMs, panelData, now);
  }

  if (buttonNetworkInfoPending()) {
    PanelPayload panelData;
    networkGetInfo(panelData.networkSsid, panelData.networkIp);
    buttonClearNetworkInfoPending();
    panelMgr.setPanel(Panel::NETWORK_INFO, panelMgr.networkInfoMs, panelData, now);
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
    if (panelMgr.currentPanel != panelMgr.prevPanel || freshData) {
      showGraph(u8g2, lastReadings);
    }
  } else {
    panelMgr.render(u8g2, now);
  }
}
