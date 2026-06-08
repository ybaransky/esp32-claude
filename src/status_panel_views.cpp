#include "display_views.h"

#include <Arduino.h>
#include "display_layout.h"
#include "hardware.h"
#include "rtc_ds3231.h"

static void drawTitleBar(U8G2 &u8g2, const char *title, unsigned long remainingSecs) {
  u8g2.drawBox(0, 0, DISPLAY_WIDTH, 10);
  u8g2.setDrawColor(0);
  u8g2.drawStr(2, 8, title);

  char buf[8];
  snprintf(buf, sizeof(buf), "%lus", remainingSecs);
  const int tw = u8g2.getStrWidth(buf);
  u8g2.drawStr(DISPLAY_WIDTH - tw - 1, 8, buf);

  u8g2.setDrawColor(1);
}

static bool appendTokenWithinWidth(U8G2 &u8g2, String &target,
                                   const String &token, int maxWidth) {
  const String candidate = target.isEmpty() ? token : (target + " " + token);
  if (u8g2.getStrWidth(candidate.c_str()) > maxWidth) return false;

  target = candidate;
  return true;
}

static void trimToSuffixWidth(U8G2 &u8g2, String &text,
                              const String &suffix, int maxWidth) {
  while (!text.isEmpty() && u8g2.getStrWidth((text + suffix).c_str()) > maxWidth) {
    text.remove(text.length() - 1);
  }
  text.trim();
  text += suffix;
}

void showSplash(U8G2 &u8g2, unsigned long remainingSecs) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  drawTitleBar(u8g2, "Explanation", remainingSecs);

  u8g2.drawStr(2, 22, "Testing the precision");
  u8g2.drawStr(2, 32, "and consistency of");
  u8g2.drawStr(2, 42, "common, off the shelf");
  u8g2.drawStr(2, 52, "temperature sensors");

  u8g2.sendBuffer();
}

void showMenu(U8G2 &u8g2, unsigned long remainingSecs) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  drawTitleBar(u8g2, "Menu", remainingSecs);

  u8g2.drawStr(2, 19, "btn1 1x: Graph/Histogram");
  u8g2.drawStr(2, 29, "btn1 2x: Reset per scrn");
  u8g2.drawStr(2, 39, "btn2 1x: Network Info");
  u8g2.drawStr(2, 49, "btn2 2x: I2C scan");
  u8g2.drawStr(2, 59, "btn2 --: RTC status");

  u8g2.sendBuffer();
}

void showNetworkInfo(U8G2 &u8g2, const String &ssid, const String &ip,
                     unsigned long remainingSecs) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  drawTitleBar(u8g2, "Network Info", remainingSecs);

  u8g2.drawStr(2, 22, "SSID:");
  u8g2.drawStr(2, 31, ssid.c_str());
  u8g2.drawStr(2, 45, "IP:");
  u8g2.drawStr(2, 54, ip.c_str());

  u8g2.sendBuffer();
}

void showI2CScan(U8G2 &u8g2, unsigned long remainingSecs) {
  const I2CScanResult scanResult = i2cGetLastScanResult();
  const int foundCount = static_cast<int>(scanResult.count);
  const int maxWidth = DISPLAY_WIDTH - 4;

  String foundLine1 = "Found: " + String(foundCount) + " device(s)";
  String foundLine2;

  if (u8g2.getStrWidth(foundLine1.c_str()) > maxWidth) {
    for (int split = foundLine1.length() - 1; split > 0; --split) {
      if (foundLine1.charAt(split) != ' ') continue;
      const String candidate = foundLine1.substring(0, split);
      if (u8g2.getStrWidth(candidate.c_str()) <= maxWidth) {
        foundLine2 = foundLine1.substring(split + 1);
        foundLine1 = candidate;
        break;
      }
    }
  }

  String line1, line2;
  if (foundCount == 0) {
    line1 = "None";
  } else {
    bool overflow = false;
    for (size_t i = 0; i < scanResult.count; ++i) {
      char addrBuf[8];
      snprintf(addrBuf, sizeof(addrBuf), "0x%02X", scanResult.addresses[i]);
      const String token(addrBuf);
      if (!appendTokenWithinWidth(u8g2, line1, token, maxWidth) &&
          !appendTokenWithinWidth(u8g2, line2, token, maxWidth)) {
        overflow = true;
      }
    }

    if (overflow) trimToSuffixWidth(u8g2, line2, " ...", maxWidth);
    if (line1.isEmpty()) line1 = "None";
  }

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  drawTitleBar(u8g2, "I2C Scan", remainingSecs);

  u8g2.drawStr(2, 24, foundLine1.c_str());
  int addressY = 38;
  if (!foundLine2.isEmpty()) {
    u8g2.drawStr(2, 34, foundLine2.c_str());
    addressY = 48;
  }
  u8g2.drawStr(2, addressY, line1.c_str());
  if (!line2.isEmpty()) {
    u8g2.drawStr(2, addressY + 10, line2.c_str());
  }

  u8g2.sendBuffer();
}

void showRtcStatus(U8G2 &u8g2, unsigned long remainingSecs) {
  const RtcStatus status = rtcGetStatus();
  const String rtcNow = rtcGetCurrentTimeString();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  drawTitleBar(u8g2, "RTC Status", remainingSecs);

  char buf[20];
  u8g2.drawStr(2, 20, rtcNow.c_str());
  snprintf(buf, sizeof(buf), "Present:%s",  status.present       ? "Yes" : "No");
  u8g2.drawStr(2, 29, buf);
  snprintf(buf, sizeof(buf), "LostPwr:%s",  status.powerLost     ? "Yes" : "No");
  u8g2.drawStr(2, 38, buf);
  snprintf(buf, sizeof(buf), "Battery:%s",  status.lowBattery    ? "Low" : "OK");
  u8g2.drawStr(2, 47, buf);
  snprintf(buf, sizeof(buf), "SQW 1Hz:%s",  status.sqwConfigured ? "Yes" : "No");
  u8g2.drawStr(2, 56, buf);

  u8g2.sendBuffer();
}

void showErrorMessage(U8G2 &u8g2, const String &message, unsigned long remainingSecs) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  drawTitleBar(u8g2, "Sensor Error", remainingSecs);

  u8g2.drawStr(2, 24, "Read failed:");
  u8g2.drawStr(2, 38, message.c_str());

  u8g2.sendBuffer();
}
