#include "button_event_handler.h"
#include "button.h"
#include "graph.h"
#include "hardware.h"
#include "histogram.h"
#include "panel_manager.h"
#include "web.h"

static void showPanel(Panel panel, unsigned long now) {
  panelSet(panel, PanelPayload{}, now);
}

static void showNetworkInfo(unsigned long now) {
  String ssid;
  String ip;
  networkGetInfo(ssid, ip);
  panelSet(Panel::NETWORK_INFO, PanelPayload::networkInfo(ssid, ip), now);
}

static bool handleMenuCommand(unsigned long now) {
  if (panelGetCurrent() != Panel::HISTOGRAM) {
    showPanel(Panel::MENU, now);
    return false;
  }
  histogramRecenterOnPeak();
  return true;
}

static bool togglePrimaryPanel(unsigned long now) {
  const Panel next = (panelGetPrimary() == Panel::GRAPH) ? Panel::HISTOGRAM : Panel::GRAPH;
  panelSet(next, PanelPayload{}, now);
  Serial.printf("[BTN1] Base view: %s\n", next == Panel::HISTOGRAM ? "Histogram" : "Graph");
  return true;
}

static bool resetCurrentPanelData() {
  const Panel current = panelGetCurrent();

  if (current == Panel::GRAPH) {
    graphResetBounds();
    Serial.println("[BTN1] Graph bounds reset");
    return true;
  }

  if (current == Panel::HISTOGRAM) {
    histogramReset();
    Serial.println("[BTN1] Histogram reset");
    return true;
  }

  Serial.println("[BTN1] Reset ignored: not on graph or histogram panel");
  return false;
}

bool buttonHandleEvents(unsigned long now) {
  bool forceRedraw = false;

  while (buttonHasEvent()) {
    switch (buttonNextEvent()) {
      case ButtonEvent::MENU:
        forceRedraw |= handleMenuCommand(now);
        break;

      case ButtonEvent::I2C_SCAN:
        i2cScan();
        showPanel(Panel::I2C_SCAN, now);
        break;

      case ButtonEvent::RTC_STATUS:
        showPanel(Panel::RTC_STATUS, now);
        break;

      case ButtonEvent::NETWORK_INFO:
        showNetworkInfo(now);
        break;

      case ButtonEvent::HISTOGRAM_TOGGLE:
        forceRedraw |= togglePrimaryPanel(now);
        break;

      case ButtonEvent::PANEL_DATA_RESET:
        forceRedraw |= resetCurrentPanelData();
        break;

      default:
        break;
    }
  }

  return forceRedraw;
}
