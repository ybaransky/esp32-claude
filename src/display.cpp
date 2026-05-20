#include "display.h"
#include "graph.h"
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

  u8g2.drawStr(2, 19, "btn1 1x: Normalize");
  u8g2.drawStr(2, 29, "btn1 2x: Zero start");
  u8g2.drawStr(2, 39, "btn2 1x: Network Info");
  u8g2.drawStr(2, 49, "btn2 2x: I2C scan");

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

void showI2CScan(U8G2 &u8g2, const String &addresses, unsigned long remainingSecs) {
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%lus", remainingSecs);

  String line = addresses;
  const int maxTextWidth = DISPLAY_WIDTH - 4;

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);

  while (line.length() > 0 && u8g2.getStrWidth(line.c_str()) > maxTextWidth) {
    line.remove(line.length() - 1);
  }
  if (line != addresses && line.length() > 3) {
    line.remove(line.length() - 3);
    line += "...";
  }

  u8g2.drawBox(0, 0, DISPLAY_WIDTH, 10);
  u8g2.setDrawColor(0);
  u8g2.drawStr(2, 8, "I2C Scan");
  u8g2.setDrawColor(1);

  u8g2.drawStr(2, 24, "Found:");
  u8g2.drawStr(2, 38, line.c_str());

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
