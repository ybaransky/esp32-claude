#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "graph.h"
#include "display.h"
#include "config.h"
#include "web.h"
#include "button.h"
#include "sensors.h"
#include "i2c_scanner.h"

constexpr uint8_t I2C_SDA_PIN = 22;
constexpr uint8_t I2C_SCL_PIN = 21;

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, I2C_SCL_PIN, I2C_SDA_PIN);

constexpr unsigned long READ_INTERVAL_SECONDS = 2;
constexpr unsigned long NETWORK_STATUS_INTERVAL_MS = 5000; // 5 seconds

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
  static unsigned long lastDataUpdate     = 0;
  static unsigned long lastNetworkRender  = 0;
  static unsigned long networkStatusUntil = 0;
  static SensorReadings lastReadings      = {};
  unsigned long now = millis();

  buttonTick();
  webHandleClients();

  if (buttonNetworkStatusPending()) {
    buttonClearNetworkStatus();
    networkStatusUntil = now + NETWORK_STATUS_INTERVAL_MS;
    lastNetworkRender  = 0;
  }

  // Always update graph data on schedule, regardless of what's on screen.
  bool freshData = false;
  if (now - lastDataUpdate >= (READ_INTERVAL_SECONDS * 1000UL)) {
    lastReadings = readSensors();
    updateGraphData(lastReadings);
    lastDataUpdate = now;
    freshData = true;
  }

  bool showingNetwork = (now < networkStatusUntil);

  if (showingNetwork) {
    // Refresh every 250 ms to update the countdown.
    if (now - lastNetworkRender >= 250UL) {
      String ssid, ip;
      webGetApInfo(ssid, ip);
      unsigned long remainingSecs = (networkStatusUntil - now + 999UL) / 1000UL;
      showNetworkStatus(u8g2, ssid, ip, remainingSecs);
      lastNetworkRender = now;
    }
  } else if (freshData) {
    showGraph(u8g2, lastReadings);
  }
}
