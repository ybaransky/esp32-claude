#pragma once
#include <Arduino.h>

struct SensorReadings {
    float bmpF;
    float shtF;
    float deltaF;
    unsigned long readTime;
};

void sensorsBegin();
const SensorReadings& readSensors(bool force = false);
