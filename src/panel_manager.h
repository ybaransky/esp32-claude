#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "display.h"

struct PanelPayload {
  String networkSsid;
  String networkIp;
  String errorMessage;

  bool operator==(const PanelPayload &other) const {
    return networkSsid == other.networkSsid &&
           networkIp == other.networkIp &&
           errorMessage == other.errorMessage;
  }
};

struct PanelRequest {
  Panel panel;
  unsigned long durationMs;
  PanelPayload data;
};

struct PanelManager {
  Panel currentPanel;
  Panel prevPanel;
  unsigned long panelUntil;
  unsigned long lastRender;
  PanelPayload panelData;
  PanelRequest queuedPanels[6];
  uint8_t queuedPanelCount;

  // Panel-specific timings
  unsigned long splashMs;
  unsigned long menuMs;
  unsigned long networkInfoMs;
  unsigned long i2cScanMs;
  unsigned long rtcStatusMs;
  unsigned long errorMessageMs;

  PanelManager();


  // Set/update panel based on priority; returns true if panel changed
  bool setPanel(Panel panel, unsigned long durationMs, const PanelPayload &data, unsigned long now);

  // Check if panel has expired; if so, advance to next panel
  bool checkExpiration(unsigned long now);

  // Render current panel; respects change flags and refresh intervals
  void render(U8G2 &u8g2, unsigned long now);

  // Allow queueing panels from outside
  bool enqueuePanelBack(Panel panel, unsigned long durationMs, const PanelPayload &data);
  bool enqueuePanelFront(Panel panel, unsigned long durationMs, const PanelPayload &data);

 private:
  void activatePanel(Panel panel, unsigned long durationMs, const PanelPayload &data, unsigned long now);
  bool dequeuePanel(PanelRequest &request);
  static int panelPriority(Panel panel);
};
