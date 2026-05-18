#include "button.h"
#include <OneButton.h>  // mathertel/OneButton
#include <Arduino.h>

// Pin 23, active-low, internal pull-up enabled.
static OneButton btn(23, true, true);

static void onClick() {
    Serial.println("[BTN] Single press detected");
}

static void onDoubleClick() {
    Serial.println("[BTN] Double click detected");
}

void buttonSetup() {
    btn.attachClick(onClick);
    btn.attachDoubleClick(onDoubleClick);
}

void buttonTick() {
    btn.tick();
}
