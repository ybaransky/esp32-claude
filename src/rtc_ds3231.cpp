#include "rtc_ds3231.h"
#include "hardware.h"

#include <Wire.h>
#include <RTClib.h>

namespace {
  constexpr uint8_t RTC_I2C_ADDRESS = Hardware::I2CAddress::DS3231;

  RTC_DS3231 rtc;
  RtcStatus rtcStatus = {false, false, false, false, "Not initialized"};
  volatile bool sqwPulsePending = false;

  bool probeRtcAddress() {
    Wire.beginTransmission(RTC_I2C_ADDRESS);
    return Wire.endTransmission() == 0;
  }

  bool isLikelyInvalidTime(const DateTime &now) {
    return now.year() < 2020 || now.year() > 2099;
  }

  void logRtcTime(const char *label, const DateTime &timeValue) {
    Serial.printf("[RTC] %s %04d-%02d-%02d %02d:%02d:%02d\n",
                  label,
                  timeValue.year(), timeValue.month(), timeValue.day(),
                  timeValue.hour(), timeValue.minute(), timeValue.second());
  }

  void adjustRtcWithLog(const DateTime &newTime, const char *reason) {
    DateTime oldTime = rtc.now();
    Serial.printf("[RTC] Adjusting time (%s)\n", reason);
    logRtcTime("Old:", oldTime);

    rtc.adjust(newTime);

    DateTime updatedTime = rtc.now();
    logRtcTime("New:", updatedTime);
  }
}  // namespace

void IRAM_ATTR onSQWPulse() {
  sqwPulsePending = true;
}

bool rtcBegin() {
  rtcStatus = {false, false, false, false, ""};

  if (!probeRtcAddress()) {
    rtcStatus.error = "DS3231 not found on I2C address 0x68";
    Serial.println("[RTC] ERROR: DS3231 not detected");
    return false;
  }

  if (!rtc.begin()) {
    rtcStatus.error = "rtc.begin() failed";
    Serial.println("[RTC] ERROR: rtc.begin() failed");
    return false;
  }

  rtcStatus.present = true;

  // Print the current RTC time as soon as we know the RTC is present and initialized
  DateTime currentTime = rtc.now();
  Serial.printf("[RTC] Current RTC time: %04d-%02d-%02d %02d:%02d:%02d\n",
                currentTime.year(), currentTime.month(), currentTime.day(),
                currentTime.hour(), currentTime.minute(), currentTime.second());

  // OSF/lostPower indicates RTC stopped, usually due to battery/main power loss.
  if (rtc.lostPower()) {
    rtcStatus.powerLost = true;
    rtcStatus.lowBattery = true;
    Serial.println("[RTC] WARNING: RTC lost power (possible low/dead backup battery)");

    // Set a known-valid time once to clear the DS3231 OSF/lostPower condition.
    adjustRtcWithLog(DateTime(F(__DATE__), F(__TIME__)), "lost power recovery");
    rtcStatus.powerLost = false;
    rtcStatus.lowBattery = false;
    Serial.println("[RTC] INFO: RTC reset to build time to clear lost-power flag");
  }

  DateTime now = rtc.now();
  if (isLikelyInvalidTime(now)) {
    rtcStatus.lowBattery = true;
    Serial.printf("[RTC] WARNING: RTC time looks invalid: %04d-%02d-%02d %02d:%02d:%02d\n",
                  now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  }

  rtc.disable32K();
  rtc.writeSqwPinMode(DS3231_SquareWave1Hz);
  rtcStatus.sqwConfigured = true;

  pinMode(Hardware::Pins::RTC_SQW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(Hardware::Pins::RTC_SQW), onSQWPulse, RISING);

  Serial.println("[RTC] DS3231 initialized, SQW=1Hz on GPIO2 (RISING, INPUT_PULLUP)");
  return true;
}

void rtcHandle() {
  if (sqwPulsePending) {
    sqwPulsePending = false;
    // Placeholder for pulse-driven tasks if needed later.
  }
}

RtcStatus rtcGetStatus() {
  return rtcStatus;
}

String rtcGetCurrentTimeString() {
  if (!rtcStatus.present) {
    return "N/A";
  }

  DateTime now = rtc.now();
  char timeBuf[20];
  snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
  return String(timeBuf);
}
