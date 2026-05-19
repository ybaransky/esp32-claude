#pragma once
#include <U8g2lib.h>
#include "web.h"

void showGraph(U8G2 &u8g2, const SensorReadings &readings);
void showNetworkStatus(U8G2 &u8g2, const String &ssid, const String &ip, unsigned long remainingSecs);
