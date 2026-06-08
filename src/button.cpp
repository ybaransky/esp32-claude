#include "button.h"

#include "hardware.h"
#include <Arduino.h>
#include <OneButton.h>

static void onBtn1Click();
static void onBtn1DoubleClick();
static void onBtn1MultiClick();
static void onBtn1LongPressStart();
static void onBtn2Click();
static void onBtn2DoubleClick();
static void onBtn2MultiClick();
static void onBtn2LongPressStart();

class ButtonController {
public:
  void begin() {
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

  void tick() {
    btn1.tick();
    btn2.tick();
  }

  void ledTick(unsigned long now) {
    if (!ledPulseActive || static_cast<long>(now - ledOffAtMs) < 0) return;

    digitalWrite(Hardware::Pins::INTERNAL_LED, LOW);
    ledPulseActive = false;
  }

  bool hasEvent() const {
    return eventHead != eventTail;
  }

  ButtonEvent nextEvent() {
    if (eventHead == eventTail) return ButtonEvent::NONE;

    const ButtonEvent event = eventQueue[eventHead];
    eventHead = (eventHead + 1) % EVENT_QUEUE_CAPACITY;
    return event;
  }

  void handleAction(const char *message, ButtonEvent event = ButtonEvent::NONE) {
    Serial.println(message);
    startLedPulse(LED_PULSE_MS);
    if (event != ButtonEvent::NONE) {
      enqueueEvent(event);
    }
  }

private:
  static constexpr int EVENT_QUEUE_CAPACITY = 8;
  static constexpr unsigned long LED_PULSE_MS = 200;

  void enqueueEvent(ButtonEvent event) {
    const int nextTail = (eventTail + 1) % EVENT_QUEUE_CAPACITY;
    if (nextTail == eventHead) return;

    eventQueue[eventTail] = event;
    eventTail = nextTail;
  }

  void startLedPulse(unsigned long durationMs) {
    digitalWrite(Hardware::Pins::INTERNAL_LED, HIGH);
    ledPulseActive = true;
    ledOffAtMs = millis() + durationMs;
  }

  OneButton btn1 = OneButton(Hardware::Pins::BUTTON_1, true, true);
  OneButton btn2 = OneButton(Hardware::Pins::BUTTON_2, true, true);
  volatile ButtonEvent eventQueue[EVENT_QUEUE_CAPACITY] = {};
  volatile int eventHead = 0;
  volatile int eventTail = 0;
  bool ledPulseActive = false;
  unsigned long ledOffAtMs = 0;
};

static ButtonController buttonController;

static void onBtn1Click() {
  buttonController.handleAction("[BTN1] Single press", ButtonEvent::TOGGLE_PRIMARY_PANEL);
}

static void onBtn1DoubleClick() {
  buttonController.handleAction("[BTN1] Double click", ButtonEvent::RESET_CURRENT_PANEL_DATA);
}

static void onBtn1MultiClick() {
  buttonController.handleAction("[BTN1] Triple click");
}

static void onBtn1LongPressStart() {
  buttonController.handleAction("[BTN1] Long press", ButtonEvent::SHOW_MENU_OR_RECENTER_HISTOGRAM);
}

static void onBtn2Click() {
  buttonController.handleAction("[BTN2] Single click", ButtonEvent::SHOW_NETWORK_INFO);
}

static void onBtn2DoubleClick() {
  buttonController.handleAction("[BTN2] Double click", ButtonEvent::SHOW_I2C_SCAN);
}

static void onBtn2MultiClick() {
  buttonController.handleAction("[BTN2] Triple click");
}

static void onBtn2LongPressStart() {
  buttonController.handleAction("[BTN2] Long press", ButtonEvent::SHOW_RTC_STATUS);
}

void buttonBegin() {
  buttonController.begin();
}

void buttonTick() {
  buttonController.tick();
}

void buttonLedTick(unsigned long now) {
  buttonController.ledTick(now);
}

bool buttonHasEvent() {
  return buttonController.hasEvent();
}

ButtonEvent buttonNextEvent() {
  return buttonController.nextEvent();
}
