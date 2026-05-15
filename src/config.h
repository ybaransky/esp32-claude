#pragma once
#include <Arduino.h>

struct ApConfig {
    String ssid;
    String password;
};

ApConfig loadApConfig();
