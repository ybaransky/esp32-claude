#pragma once
#include "sensors.h"

void webBegin(const char *ssid, const char *password);
void webUpdate(const SensorReadings &readings);
void webHandleClients();
void networkGetInfo(String &ssid, String &ip);
