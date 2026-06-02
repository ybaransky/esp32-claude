#include "button.h"
#include "hardware.h"
#include <OneButton.h>
#include <Arduino.h>

static OneButton btn1(Hardware::Pins::BUTTON_1, true, true);
static OneButton btn2(Hardware::Pins::BUTTON_2, true, true);

constexpr int BUTTON_EVENT_QUEUE_CAPACITY = 8;
static volatile ButtonEvent eventQueue[BUTTON_EVENT_QUEUE_CAPACITY];
static volatile int eventHead = 0;
static volatile int eventTail = 0;
static bool ledPulseActive = false;
static unsigned long ledOffAtMs = 0;

static void enqueueEvent(ButtonEvent evt) {
    int next = (eventTail + 1) % BUTTON_EVENT_QUEUE_CAPACITY;
    if (next != eventHead) {
        eventQueue[eventTail] = evt;
        eventTail = next;
    }
}

bool buttonHasEvent() {
    return eventHead != eventTail;
}

ButtonEvent buttonNextEvent() {
    if (eventHead == eventTail) return ButtonEvent::NONE;
    ButtonEvent evt = eventQueue[eventHead];
    eventHead = (eventHead + 1) % BUTTON_EVENT_QUEUE_CAPACITY;
    return evt;
}

static void startLedPulse(unsigned long durationMs) {
    digitalWrite(Hardware::Pins::INTERNAL_LED, HIGH);
    ledPulseActive = true;
    ledOffAtMs = millis() + durationMs;
}

static void handleButtonAction(const char *message, ButtonEvent event = ButtonEvent::NONE) {
    Serial.println(message);
    startLedPulse(200);
    if (event != ButtonEvent::NONE) {
        enqueueEvent(event);
    }
}

static void onBtn1Click() {
    handleButtonAction("[BTN1] Single press", ButtonEvent::HISTOGRAM_TOGGLE);
}

static void onBtn1DoubleClick() {
    handleButtonAction("[BTN1] Double click", ButtonEvent::PANEL_DATA_RESET);
}

static void onBtn1MultiClick() {
    handleButtonAction("[BTN1] Triple click");
}

static void onBtn1LongPressStart() {
    handleButtonAction("[BTN1] Long press", ButtonEvent::MENU);
}

static void onBtn2Click() {
    handleButtonAction("[BTN2] Single click", ButtonEvent::NETWORK_INFO);
}

static void onBtn2DoubleClick() {
    handleButtonAction("[BTN2] Double click", ButtonEvent::I2C_SCAN);
}

static void onBtn2MultiClick() {
    handleButtonAction("[BTN2] Triple click");
}

static void onBtn2LongPressStart() {
    handleButtonAction("[BTN2] Long press", ButtonEvent::RTC_STATUS);
}

void buttonBegin() {
    pinMode(Hardware::Pins::INTERNAL_LED, OUTPUT);
    digitalWrite(Hardware::Pins::INTERNAL_LED, LOW);

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

void buttonLedTick(unsigned long now) {
    if (!ledPulseActive || static_cast<long>(now - ledOffAtMs) < 0) return;

    digitalWrite(Hardware::Pins::INTERNAL_LED, LOW);
    ledPulseActive = false;
}
