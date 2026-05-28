#pragma once
#include <U8g2lib.h>
#include "sensors.h"

void displayBegin();
U8G2 &displayDevice();

void showGraph(U8G2 &u8g2, const SensorReadings &readings);
void showHistogram(U8G2 &u8g2, const SensorReadings &readings);
void showSplash(U8G2 &u8g2, unsigned long remainingSecs);
void showMenu(U8G2 &u8g2, unsigned long remainingSecs);
void showNetworkInfo(U8G2 &u8g2, const String &ssid, const String &ip, unsigned long remainingSecs);
void showI2CScan(U8G2 &u8g2, unsigned long remainingSecs);
void showRtcStatus(U8G2 &u8g2, unsigned long remainingSecs);
void showErrorMessage(U8G2 &u8g2, const String &message, unsigned long remainingSecs);
