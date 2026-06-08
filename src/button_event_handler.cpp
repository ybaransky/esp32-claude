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

static bool showMenuOrRecenterHistogram(unsigned long now) {
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
      case ButtonEvent::SHOW_MENU_OR_RECENTER_HISTOGRAM:
        forceRedraw |= showMenuOrRecenterHistogram(now);
        break;

      case ButtonEvent::SHOW_I2C_SCAN:
        i2cScan();
        showPanel(Panel::I2C_SCAN, now);
        break;

      case ButtonEvent::SHOW_RTC_STATUS:
        showPanel(Panel::RTC_STATUS, now);
        break;

      case ButtonEvent::SHOW_NETWORK_INFO:
        showNetworkInfo(now);
        break;

      case ButtonEvent::TOGGLE_PRIMARY_PANEL:
        forceRedraw |= togglePrimaryPanel(now);
        break;

      case ButtonEvent::RESET_CURRENT_PANEL_DATA:
        forceRedraw |= resetCurrentPanelData();
        break;

      default:
        break;
    }
  }

  return forceRedraw;
}
