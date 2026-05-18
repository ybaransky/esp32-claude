#pragma once
#include <Arduino.h>
#include "web.h"

void sensorsInit(uint8_t sdaPin, uint8_t sclPin);
const SensorReadings& readSensors(bool force = false);
