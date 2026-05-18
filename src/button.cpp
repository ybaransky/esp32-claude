#include "button.h"
#include <OneButton.h>  // mathertel/OneButton
#include <Arduino.h>

constexpr uint8_t BUTTON_PIN = 23;
constexpr uint8_t LED_PIN    = 2;   // ESP32 dev board built-in LED

// Pin 23, active-low, internal pull-up enabled.
static OneButton btn(BUTTON_PIN, true, true);

static void onClick() {
    Serial.println("[BTN] Single press detected");
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
}

static void onDoubleClick() {
    Serial.println("[BTN] Double click detected");
}

void buttonSetup() {
    pinMode(LED_PIN, OUTPUT);
    btn.attachClick(onClick);
    btn.attachDoubleClick(onDoubleClick);
}

void buttonTick() {
    btn.tick();
}
