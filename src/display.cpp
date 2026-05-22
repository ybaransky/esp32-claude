#include "display.h"
#include "graph.h"
#include "i2c_scanner.h"
#include "rtc_ds3231.h"
#include <math.h>

static Bounds getPaddedBounds(const Bounds &bounds, float data) {
  float span = bounds.maximum - bounds.minimum;
  float pad  = (span > 0.0f) ? span * 0.05f : 0.1f * data;
  return {bounds.minimum - pad, bounds.maximum + pad, true};
}

static int diffToGraphY(float value, const Bounds &bounds) {
  float normalized = (value - bounds.minimum) / (bounds.maximum - bounds.minimum);
  normalized = fmaxf(0.0f, fminf(1.0f, normalized));
  return GRAPH_BOTTOM - static_cast<int>(normalized * (GRAPH_HEIGHT - 1) + 0.5f);
}

void showGraph(U8G2 &u8g2, const SensorReadings &readings) {
  float value = readings.deltaF;
  Bounds graphBounds = graphGetGraphBounds();
  Bounds totalBounds = graphGetTotalBounds();
  Bounds bounds      = getPaddedBounds(graphBounds, value);

  const float *history      = graphGetHistory();
  int          historyCount = graphGetHistoryCount();

  bool hasNegative = (value < 0.0f) || (bounds.minimum < 0.0f) || (bounds.maximum < 0.0f);
  int effectiveGraphLeft = GRAPH_LEFT + (hasNegative ? 6 : 0);

  const char *yFmt      = hasNegative ? "%5.2f" : "%4.2f";
  const char *sensorFmt = (readings.bmpF < 0.0f) | (readings.shtF < 0.0f) ? "%6.2f" : "%5.2f";
  const char *minmaxFmt = (totalBounds.minimum < 0.0f) | (totalBounds.maximum < 0.0f) ? "%5.2f" : "%4.2f";

  char maxBuf[12], midBuf[12], minBuf[12], statusBuf[28], countBuf[12], minmaxBuf[28];
  snprintf(maxBuf,   sizeof(maxBuf),   yFmt, bounds.maximum);
  snprintf(midBuf,   sizeof(midBuf),   yFmt, value);
  snprintf(minBuf,   sizeof(minBuf),   yFmt, bounds.minimum);
  snprintf(countBuf, sizeof(countBuf), "%lu", readings.readTime / 1000UL);

  int n = snprintf(statusBuf, sizeof(statusBuf), "BMP=");
  n    += snprintf(statusBuf + n, sizeof(statusBuf) - n, sensorFmt, readings.bmpF);
  n    += snprintf(statusBuf + n, sizeof(statusBuf) - n, "  min=");
         snprintf(statusBuf + n, sizeof(statusBuf) - n, minmaxFmt, totalBounds.minimum);

  n  = snprintf(minmaxBuf, sizeof(minmaxBuf), "SHT=");
  n += snprintf(minmaxBuf + n, sizeof(minmaxBuf) - n, sensorFmt, readings.shtF);
  n += snprintf(minmaxBuf + n, sizeof(minmaxBuf) - n, "  max=");
       snprintf(minmaxBuf + n, sizeof(minmaxBuf) - n, minmaxFmt, totalBounds.maximum);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 7,                                maxBuf);
  u8g2.drawStr(0, (GRAPH_TOP + GRAPH_BOTTOM) / 2 + 3, midBuf);
  u8g2.drawStr(0, GRAPH_BOTTOM,                     minBuf);

  u8g2.drawVLine(effectiveGraphLeft - 1, GRAPH_TOP, GRAPH_HEIGHT);
  u8g2.drawHLine(effectiveGraphLeft - 1, GRAPH_BOTTOM, GRAPH_WIDTH);
  u8g2.drawHLine(effectiveGraphLeft - 4, diffToGraphY(value, bounds), 4);

  if (historyCount == 1) {
    u8g2.drawPixel(effectiveGraphLeft, diffToGraphY(history[0], bounds));
  } else {
    for (int i = 1; i < historyCount; i++) {
      u8g2.drawLine(
        effectiveGraphLeft + i - 1, diffToGraphY(history[i - 1], bounds),
        effectiveGraphLeft + i,     diffToGraphY(history[i],     bounds)
      );
    }
  }

  int countX = DISPLAY_WIDTH - u8g2.getStrWidth(countBuf);
  u8g2.drawStr(0,      STATUS_BASELINE_Y, statusBuf);
  u8g2.drawStr(0,      MINMAX_BASELINE_Y, minmaxBuf);
  u8g2.drawStr(countX, MINMAX_BASELINE_Y, countBuf);
  u8g2.sendBuffer();
}

void showSplash(U8G2 &u8g2, unsigned long remainingSecs) {
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%lus", remainingSecs);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.drawBox(0, 0, DISPLAY_WIDTH, 10);
  u8g2.setDrawColor(0);
  u8g2.drawStr(2, 8, "Explanation");
  u8g2.setDrawColor(1);

  u8g2.drawStr(2, 22, "Testing the precision");
  u8g2.drawStr(2, 32, "and consistency of");
  u8g2.drawStr(2, 42, "common, off the shelf");
  u8g2.drawStr(2, 52, "temperature sensors");

  int tw = u8g2.getStrWidth(timeBuf);
  u8g2.drawStr(DISPLAY_WIDTH - tw - 1, MINMAX_BASELINE_Y, timeBuf);

  u8g2.sendBuffer();
}

void showMenu(U8G2 &u8g2, unsigned long remainingSecs) {
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%lus", remainingSecs);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  u8g2.drawBox(0, 0, DISPLAY_WIDTH, 10);
  u8g2.setDrawColor(0);
  u8g2.drawStr(2, 8, "Menu");
  u8g2.setDrawColor(1);

  u8g2.drawStr(2, 19, "btn1 1x: rescale Y-axis");
  u8g2.drawStr(2, 29, "btn1 2x: start from 0");
  u8g2.drawStr(2, 39, "btn2 1x: Network Info");
  u8g2.drawStr(2, 49, "btn2 2x: I2C scan");
  u8g2.drawStr(2, 59, "btn2 --: RTC status");

  int tw = u8g2.getStrWidth(timeBuf);
  u8g2.drawStr(DISPLAY_WIDTH - tw - 1, MINMAX_BASELINE_Y, timeBuf);

  u8g2.sendBuffer();
}

void showNetworkInfo(U8G2 &u8g2, const String &ssid, const String &ip, unsigned long remainingSecs) {
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%lus", remainingSecs);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr); // Restore previous font size for network status values

  // Title bar
  u8g2.drawBox(0, 0, DISPLAY_WIDTH, 10);
  u8g2.setDrawColor(0);
  u8g2.drawStr(2, 8, "Network Info");
  u8g2.setDrawColor(1);

  // SSID label + value
  u8g2.drawStr(2, 22, "SSID:");
  u8g2.drawStr(2, 31, ssid.c_str());

  // IP label + value
  u8g2.drawStr(2, 45, "IP:");
  u8g2.drawStr(2, 54, ip.c_str());

  // Countdown bottom-right
  int tw = u8g2.getStrWidth(timeBuf);
  u8g2.drawStr(DISPLAY_WIDTH - tw - 1, MINMAX_BASELINE_Y, timeBuf);

  u8g2.sendBuffer();
}

void showI2CScan(U8G2 &u8g2, unsigned long remainingSecs) {
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%lus", remainingSecs);

  String line1 = "";
  String line2 = "";
  const int maxTextWidth = DISPLAY_WIDTH - 4;

  I2CScanResult scanResult = i2cGetLastScanResult();
  int foundCount = static_cast<int>(scanResult.count);

  String foundLine1 = "Found: " + String(foundCount) + " device(s)";
  String foundLine2 = "";

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);

  if (u8g2.getStrWidth(foundLine1.c_str()) > maxTextWidth) {
    int split = foundLine1.length() - 1;
    while (split > 0) {
      if (foundLine1.charAt(split) == ' ') {
        String candidate = foundLine1.substring(0, split);
        if (u8g2.getStrWidth(candidate.c_str()) <= maxTextWidth) {
          foundLine2 = foundLine1.substring(split + 1);
          foundLine1 = candidate;
          break;
        }
      }
      split--;
    }
  }

  auto appendToken = [&](String &target, const String &token) {
    String candidate = target.length() == 0 ? token : (target + " " + token);
    if (u8g2.getStrWidth(candidate.c_str()) <= maxTextWidth) {
      target = candidate;
      return true;
    }
    return false;
  };

  if (scanResult.count == 0) {
    line1 = "None";
  } else {
    bool hasOverflow = false;
    for (size_t i = 0; i < scanResult.count; ++i) {
      char addrBuf[8];
      snprintf(addrBuf, sizeof(addrBuf), "0x%02X", scanResult.addresses[i]);
      String token(addrBuf);

      if (!appendToken(line1, token)) {
        if (!appendToken(line2, token)) {
          hasOverflow = true;
        }
      }
    }

    if (hasOverflow) {
      const String suffix = " ...";
      while (line2.length() > 0 && u8g2.getStrWidth((line2 + suffix).c_str()) > maxTextWidth) {
        line2.remove(line2.length() - 1);
      }
      line2.trim();
      line2 += suffix;
    }

    if (line1.length() == 0) {
      line1 = "None";
    }
  }

  u8g2.drawBox(0, 0, DISPLAY_WIDTH, 10);
  u8g2.setDrawColor(0);
  u8g2.drawStr(2, 8, "I2C Scan");
  u8g2.setDrawColor(1);

  u8g2.drawStr(2, 24, foundLine1.c_str());
  int addressY = 38;
  if (foundLine2.length() > 0) {
    u8g2.drawStr(2, 34, foundLine2.c_str());
    addressY = 48;
  }
  u8g2.drawStr(2, addressY, line1.c_str());
  if (line2.length() > 0) {
    u8g2.drawStr(2, addressY + 10, line2.c_str());
  }

  int tw = u8g2.getStrWidth(timeBuf);
  u8g2.drawStr(DISPLAY_WIDTH - tw - 1, MINMAX_BASELINE_Y, timeBuf);

  u8g2.sendBuffer();
}

void showRtcStatus(U8G2 &u8g2, unsigned long remainingSecs) {
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%lus", remainingSecs);

  RtcStatus status = rtcGetStatus();
  String rtcNow = rtcGetCurrentTimeString();

  const char *presentText = status.present ? "Yes" : "No";
  const char *powerText   = status.powerLost ? "Yes" : "No";
  const char *batteryText = status.lowBattery ? "Low" : "OK";
  const char *sqwText     = status.sqwConfigured ? "Yes" : "No";

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.drawBox(0, 0, DISPLAY_WIDTH, 10);
  u8g2.setDrawColor(0);
  u8g2.drawStr(2, 8, "RTC Status");
  u8g2.setDrawColor(1);

  u8g2.drawStr(2, 20, rtcNow.c_str());
  u8g2.drawStr(2, 29, (String("Present:") + presentText).c_str());
  u8g2.drawStr(2, 38, (String("LostPwr:") + powerText).c_str());
  u8g2.drawStr(2, 47, (String("Battery:") + batteryText).c_str());
  u8g2.drawStr(2, 56, (String("SQW 1Hz:") + sqwText).c_str());

  String nowLine = "T:" + rtcNow;
  if (u8g2.getStrWidth(nowLine.c_str()) > DISPLAY_WIDTH - 4) {
    nowLine = rtcNow.substring(rtcNow.length() - 8);
  }
  //u8g2.drawStr(2, 56, nowLine.c_str());

  int tw = u8g2.getStrWidth(timeBuf);
  u8g2.drawStr(DISPLAY_WIDTH - tw - 1, MINMAX_BASELINE_Y, timeBuf);

  u8g2.sendBuffer();
}

void showErrorMessage(U8G2 &u8g2, const String &message, unsigned long remainingSecs) {
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%lus", remainingSecs);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.drawBox(0, 0, DISPLAY_WIDTH, 10);
  u8g2.setDrawColor(0);
  u8g2.drawStr(2, 8, "Sensor Error");
  u8g2.setDrawColor(1);

  u8g2.drawStr(2, 24, "Read failed:");
  u8g2.drawStr(2, 38, message.c_str());

  int tw = u8g2.getStrWidth(timeBuf);
  u8g2.drawStr(DISPLAY_WIDTH - tw - 1, MINMAX_BASELINE_Y, timeBuf);

  u8g2.sendBuffer();
}
