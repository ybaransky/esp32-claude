#pragma once
#include <U8g2lib.h>
#include "web.h"

enum class Panel {
  GRAPH,
  SPLASH,
  MENU,
  NETWORK_INFO,
  I2C_SCAN,
  ERROR_MESSAGE,
};

// Returns the panel to fall back to when 'panel' expires.
inline Panel nextPanel(Panel panel) {
  switch (panel) {
    case Panel::SPLASH:       return Panel::GRAPH;
    case Panel::MENU:         return Panel::GRAPH;
    case Panel::NETWORK_INFO: return Panel::GRAPH;
    case Panel::I2C_SCAN:       return Panel::GRAPH;
    case Panel::ERROR_MESSAGE:  return Panel::GRAPH;
    default:                    return Panel::GRAPH;
  }
}

void showGraph(U8G2 &u8g2, const SensorReadings &readings);
void showSplash(U8G2 &u8g2, unsigned long remainingSecs);
void showMenu(U8G2 &u8g2, unsigned long remainingSecs);
void showNetworkInfo(U8G2 &u8g2, const String &ssid, const String &ip, unsigned long remainingSecs);
void showI2CScan(U8G2 &u8g2, const String &addresses, unsigned long remainingSecs);
void showErrorMessage(U8G2 &u8g2, const String &message, unsigned long remainingSecs);
