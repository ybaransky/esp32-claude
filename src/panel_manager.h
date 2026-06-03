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

  static PanelPayload networkInfo(const String &ssid, const String &ip) {
    PanelPayload p;
    p.networkSsid = ssid;
    p.networkIp   = ip;
    return p;
  }

  static PanelPayload error(const String &message) {
    PanelPayload p;
    p.errorMessage = message;
    return p;
  }

  bool operator==(const PanelPayload &other) const {
    return networkSsid  == other.networkSsid  &&
           networkIp    == other.networkIp    &&
           errorMessage == other.errorMessage;
  }
};

void  panelSet(Panel panel, const PanelPayload &data, unsigned long now);
void  panelCheckExpiration(unsigned long now);
void  panelRender(U8G2 &u8g2, unsigned long now);
Panel panelGetCurrent();
Panel panelGetPrimary();
bool  panelHasChanged();
