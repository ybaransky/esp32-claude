#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

enum class Panel {
  GRAPH,
  HISTOGRAM,
  SPLASH,
  MENU,
  NETWORK_INFO,
  I2C_SCAN,
  RTC_STATUS,
  ERROR_MESSAGE,
};

struct PanelPayload {
  String networkSsid;
  String networkIp;
  String errorMessage;

  bool operator==(const PanelPayload &other) const {
    return networkSsid  == other.networkSsid  &&
           networkIp    == other.networkIp    &&
           errorMessage == other.errorMessage;
  }
};

struct PanelManager {
  PanelManager();

  // Request a panel. Duration is determined internally per panel type.
  // Returns true if the panel became active immediately.
  bool setPanel(Panel panel, const PanelPayload &data, unsigned long now);

  // Advance to the next panel if the current one has expired.
  bool checkExpiration(unsigned long now);

  // Render the current panel; respects change detection and 250 ms refresh rate.
  void render(U8G2 &u8g2, unsigned long now);

  Panel getCurrentPanel() const { return currentPanel; }
  Panel getPrimaryPanel() const { return primaryPanel; }
  bool  panelChanged()    const { return currentPanel != prevPanel; }

 private:
  struct PanelRequest {
    Panel panel;
    unsigned long durationMs;
    PanelPayload data;
  };

  Panel         currentPanel;
  Panel         prevPanel;
  Panel         primaryPanel;
  unsigned long panelUntil;
  unsigned long lastRender;
  PanelPayload  panelData;
  PanelRequest  queuedPanels[6];
  uint8_t       queuedPanelCount;

  void activatePanel(Panel panel, unsigned long durationMs, const PanelPayload &data, unsigned long now);
  bool enqueuePanelBack(Panel panel, unsigned long durationMs, const PanelPayload &data);
  bool enqueuePanelFront(Panel panel, unsigned long durationMs, const PanelPayload &data);
  bool dequeuePanel(PanelRequest &request);

  static int           panelPriority(Panel panel);
  static unsigned long panelDuration(Panel panel);
};
