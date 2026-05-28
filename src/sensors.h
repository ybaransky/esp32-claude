#pragma once
#include <Arduino.h>

struct SensorReadings {
    float bmpF;
    float shtF;
    float deltaF;
    unsigned long readTime;
};

void sensorsBegin(uint8_t sdaPin, uint8_t sclPin);
const SensorReadings& readSensors(bool force = false);
