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

static void blink(unsigned int ms) {
    digitalWrite(Hardware::Pins::INTERNAL_LED, HIGH);
    delay(ms);
    digitalWrite(Hardware::Pins::INTERNAL_LED, LOW);
}

static void onBtn1Click() {
    Serial.println("[BTN1] Single press");
    blink(200);
    enqueueEvent(ButtonEvent::HISTOGRAM_TOGGLE);
}

static void onBtn1DoubleClick() {
    Serial.println("[BTN1] Double click");
    blink(200);
    enqueueEvent(ButtonEvent::PANEL_DATA_RESET);
}

static void onBtn1MultiClick() {
    Serial.println("[BTN1] Triple click");
    blink(200);
}

static void onBtn1LongPressStart() {
    Serial.println("[BTN1] Long press");
    blink(200);
    enqueueEvent(ButtonEvent::MENU);
}

static void onBtn2Click() {
    Serial.println("[BTN2] Single click");
    blink(200);
    enqueueEvent(ButtonEvent::NETWORK_INFO);
}

static void onBtn2DoubleClick() {
    Serial.println("[BTN2] Double click");
    blink(200);
    enqueueEvent(ButtonEvent::I2C_SCAN);
}

static void onBtn2MultiClick() {
    Serial.println("[BTN2] Triple click");
    blink(200);
}

static void onBtn2LongPressStart() {
    Serial.println("[BTN2] Long press");
    blink(200);
    enqueueEvent(ButtonEvent::RTC_STATUS);
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
