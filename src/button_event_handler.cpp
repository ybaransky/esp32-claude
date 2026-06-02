#include "button_event_handler.h"

#include "button.h"
#include "graph.h"
#include "hardware.h"
#include "histogram.h"
#include "web.h"

static void showPanel(PanelManager &panelMgr, Panel panel, unsigned long now) {
  panelMgr.setPanel(panel, PanelPayload{}, now);
}

static void showNetworkInfo(PanelManager &panelMgr, unsigned long now) {
  String ssid;
  String ip;
  networkGetInfo(ssid, ip);
  panelMgr.setPanel(Panel::NETWORK_INFO, PanelPayload::networkInfo(ssid, ip), now);
}

static bool handleMenuCommand(PanelManager &panelMgr, unsigned long now) {
  if (panelMgr.getCurrentPanel() != Panel::HISTOGRAM) {
    showPanel(panelMgr, Panel::MENU, now);
    return false;
  }

  histogramRecenterOnPeak();
  return true;
}

static bool togglePrimaryPanel(PanelManager &panelMgr, unsigned long now) {
  const Panel next = (panelMgr.getPrimaryPanel() == Panel::GRAPH)
                   ? Panel::HISTOGRAM : Panel::GRAPH;
  panelMgr.setPanel(next, PanelPayload{}, now);
  Serial.printf("[BTN1] Base view: %s\n",
                next == Panel::HISTOGRAM ? "Histogram" : "Graph");
  return true;
}

static bool resetCurrentPanelData(PanelManager &panelMgr) {
  const Panel current = panelMgr.getCurrentPanel();

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

bool processButtonEvents(PanelManager &panelMgr, unsigned long now) {
  bool forceRedraw = false;

  while (buttonHasEvent()) {
    switch (buttonNextEvent()) {
      case ButtonEvent::MENU:
        forceRedraw |= handleMenuCommand(panelMgr, now);
        break;

      case ButtonEvent::I2C_SCAN:
        i2cScan();
        showPanel(panelMgr, Panel::I2C_SCAN, now);
        break;

      case ButtonEvent::RTC_STATUS:
        showPanel(panelMgr, Panel::RTC_STATUS, now);
        break;

      case ButtonEvent::NETWORK_INFO:
        showNetworkInfo(panelMgr, now);
        break;

      case ButtonEvent::HISTOGRAM_TOGGLE:
        forceRedraw |= togglePrimaryPanel(panelMgr, now);
        break;

      case ButtonEvent::PANEL_DATA_RESET:
        forceRedraw |= resetCurrentPanelData(panelMgr);
        break;

      default:
        break;
    }
  }

  return forceRedraw;
}
