#include "button.h"
#include "graph.h"
#include "i2c_scanner.h"
#include "hardware.h"
#include <OneButton.h>  // mathertel/OneButton
#include <Arduino.h>

// Pin 23, active-low, internal pull-up enabled.
static OneButton btn1(Hardware::Pins::BUTTON_1, true, true);

// Pin 19, active-low, internal pull-up enabled.
static OneButton btn2(Hardware::Pins::BUTTON_2, true, true);

static volatile bool splashPending = false;
static volatile bool menuPending = false;
static volatile bool networkInfoPending = false;
static volatile bool i2cScanPending = false;
static volatile bool rtcStatusPending = false;
static volatile bool histogramTogglePending = false;
static volatile bool panelDataResetPending = false;

bool buttonSplashPending() {
    return splashPending;
}

void buttonClearSplashPending() {
    splashPending = false;
}

bool buttonMenuPending() {
    return menuPending;
}

void buttonClearMenuPending() {
    menuPending = false;
}

bool buttonNetworkInfoPending() {
    return networkInfoPending;
}

void buttonClearNetworkInfoPending() {
    networkInfoPending = false;
}

bool buttonI2cScanPending() {
    return i2cScanPending;
}

void buttonClearI2cScanPending() {
    i2cScanPending = false;
}

bool buttonRtcStatusPending() {
    return rtcStatusPending;
}

void buttonClearRtcStatusPending() {
    rtcStatusPending = false;
}

bool buttonHistogramTogglePending() {
    return histogramTogglePending;
}

void buttonClearHistogramTogglePending() {
    histogramTogglePending = false;
}

bool buttonPanelDataResetPending() {
    return panelDataResetPending;
}

void buttonClearPanelDataResetPending() {
    panelDataResetPending = false;
}

static void blink(unsigned int ms) {
    digitalWrite(Hardware::Pins::INTERNAL_LED, HIGH);
    delay(ms);
    digitalWrite(Hardware::Pins::INTERNAL_LED, LOW);
}

static void onBtn1Click() {
    Serial.println("[BTN1] Single press detected");
    blink(200);
    histogramTogglePending = true;
}

static void onBtn1DoubleClick() {
    Serial.println("[BTN1] Double click detected");
    blink(200);
    panelDataResetPending = true;
}

static void onBtn1MultiClick() {
    Serial.println("[BTN1] Triple click detected");
    blink(200);
}

static void onBtn1LongPressStart() {
    Serial.println("[BTN1] Long press detected");
    blink(200);
    menuPending = true;
}
static void onBtn2Click() {
    Serial.println("[BTN2] Single click detected");
    blink(200);
    networkInfoPending = true;
}

static void onBtn2DoubleClick() {
    Serial.println("[BTN2] Double click detected");
    blink(200);
    i2cScanPending = true;
    // TODO: define action for button 2 double-click
}

static void onBtn2MultiClick() {
    Serial.println("[BTN2] Triple click detected");
    blink(200);
}

static void onBtn2LongPressStart() {
    Serial.println("[BTN2] Long press detected");
    blink(200);
    rtcStatusPending = true;
}

void buttonBegin() {
    pinMode(Hardware::Pins::INTERNAL_LED, OUTPUT);

    btn1.attachClick(onBtn1Click);
    btn1.attachDoubleClick(onBtn1DoubleClick);
    btn1.attachLongPressStart(onBtn1LongPressStart);
    btn1.attachMultiClick(onBtn1MultiClick);

    btn2.attachClick(onBtn2Click);
    btn2.attachDoubleClick(onBtn2DoubleClick);
    btn2.attachLongPressStart(onBtn2LongPressStart);
    btn2.attachMultiClick(onBtn2MultiClick);
}

void buttonTick() {
    btn1.tick();
    btn2.tick();
}
