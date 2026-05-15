#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

ApConfig loadApConfig() {
    ApConfig cfg{"ESP32-Sensor", "12345678"};

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed, using default AP config");
        return cfg;
    }

    File f = LittleFS.open("/config.json", "r");
    if (!f) {
        Serial.println("config.json not found, using default AP config");
        return cfg;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("config.json parse error: %s, using defaults\n", err.c_str());
        return cfg;
    }

    cfg.ssid     = doc["ssid"]     | "ESP32-Sensor";
    cfg.password = doc["password"] | "12345678";
    return cfg;
}
