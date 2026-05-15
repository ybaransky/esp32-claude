#pragma once
#include <Arduino.h>

struct SensorReadings {
    float bmpF;
    float shtF;
    float diffF;
};

void webBegin(const char *ssid, const char *password);
void webUpdate(const SensorReadings &readings);
void webHandleClients();
