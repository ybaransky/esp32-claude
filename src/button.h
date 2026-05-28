#pragma once

enum class ButtonEvent {
  NONE,
  MENU,
  NETWORK_INFO,
  I2C_SCAN,
  RTC_STATUS,
  HISTOGRAM_TOGGLE,
  PANEL_DATA_RESET,
};

void buttonBegin();
void buttonTick();
bool buttonHasEvent();
ButtonEvent buttonNextEvent();
