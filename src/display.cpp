#include "display.h"
#include "graph.h"
#include "histogram.h"
#include "rtc_ds3231.h"
#include "hardware.h"
#include <math.h>

namespace {
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
  U8G2_R0,
  U8X8_PIN_NONE,
  Hardware::Pins::I2C_SCL,
  Hardware::Pins::I2C_SDA
);
}

void displayBegin() {
  // u8g2.begin() initializes Wire using the pins passed in the constructor.
  display.begin();
}

U8G2 &displayDevice() {
  return display;
}

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

static void drawTitleBar(U8G2 &u8g2, const char *title, unsigned long remainingSecs) {
  u8g2.drawBox(0, 0, DISPLAY_WIDTH, 10);
  u8g2.setDrawColor(0);
  u8g2.drawStr(2, 8, title);

  char buf[8];
  snprintf(buf, sizeof(buf), "%lus", remainingSecs);
  int tw = u8g2.getStrWidth(buf);
  u8g2.drawStr(DISPLAY_WIDTH - tw - 1, 8, buf);

  u8g2.setDrawColor(1);
}

// ---------------------------------------------------------------------------
// Graph
// ---------------------------------------------------------------------------

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

static void drawGraphAxes(U8G2 &u8g2, int graphLeft, float value,
                          const Bounds &bounds, const char *yFmt) {
  char maxBuf[12], midBuf[12], minBuf[12];
  snprintf(maxBuf, sizeof(maxBuf), yFmt, bounds.maximum);
  snprintf(midBuf, sizeof(midBuf), yFmt, value);
  snprintf(minBuf, sizeof(minBuf), yFmt, bounds.minimum);

  u8g2.drawStr(0, 7,                                  maxBuf);
  u8g2.drawStr(0, (GRAPH_TOP + GRAPH_BOTTOM) / 2 + 3, midBuf);
  u8g2.drawStr(0, GRAPH_BOTTOM,                        minBuf);

  u8g2.drawVLine(graphLeft - 1, GRAPH_TOP, GRAPH_HEIGHT);
  u8g2.drawHLine(graphLeft - 1, GRAPH_BOTTOM, GRAPH_WIDTH);
  u8g2.drawHLine(graphLeft - 4, diffToGraphY(value, bounds), 4);
}

static void drawGraphHistory(U8G2 &u8g2, int graphLeft,
                             const float *history, int count, const Bounds &bounds) {
  if (count == 1) {
    u8g2.drawPixel(graphLeft, diffToGraphY(history[0], bounds));
    return;
  }
  for (int i = 1; i < count; i++) {
    u8g2.drawLine(
      graphLeft + i - 1, diffToGraphY(history[i - 1], bounds),
      graphLeft + i,     diffToGraphY(history[i],     bounds)
    );
  }
}

static void drawGraphStatusBar(U8G2 &u8g2, const SensorReadings &r,
                               const Bounds &totalBounds) {
  const char *sensorFmt = (r.bmpF < 0.0f || r.shtF < 0.0f)               ? "%6.2f" : "%5.2f";
  const char *minmaxFmt = (totalBounds.minimum < 0.0f || totalBounds.maximum < 0.0f) ? "%5.2f" : "%4.2f";

  char statusBuf[28], minmaxBuf[28], countBuf[12];
  snprintf(countBuf, sizeof(countBuf), "%lu", r.readTime / 1000UL);

  int n;
  n  = snprintf(statusBuf, sizeof(statusBuf), "BMP=");
  n += snprintf(statusBuf + n, sizeof(statusBuf) - n, sensorFmt, r.bmpF);
  n += snprintf(statusBuf + n, sizeof(statusBuf) - n, "  min=");
       snprintf(statusBuf + n, sizeof(statusBuf) - n, minmaxFmt, totalBounds.minimum);

  n  = snprintf(minmaxBuf, sizeof(minmaxBuf), "SHT=");
  n += snprintf(minmaxBuf + n, sizeof(minmaxBuf) - n, sensorFmt, r.shtF);
  n += snprintf(minmaxBuf + n, sizeof(minmaxBuf) - n, "  max=");
       snprintf(minmaxBuf + n, sizeof(minmaxBuf) - n, minmaxFmt, totalBounds.maximum);

  int countX = DISPLAY_WIDTH - u8g2.getStrWidth(countBuf);
  u8g2.drawStr(0,      STATUS_BASELINE_Y, statusBuf);
  u8g2.drawStr(0,      MINMAX_BASELINE_Y, minmaxBuf);
  u8g2.drawStr(countX, MINMAX_BASELINE_Y, countBuf);
}

void showGraph(U8G2 &u8g2, const SensorReadings &readings) {
  Bounds graphBounds = graphGetGraphBounds();
  Bounds totalBounds = graphGetTotalBounds();
  Bounds bounds      = getPaddedBounds(graphBounds, readings.deltaF);

  bool       hasNeg    = readings.deltaF < 0.0f || bounds.minimum < 0.0f || bounds.maximum < 0.0f;
  int        graphLeft = GRAPH_LEFT + (hasNeg ? 6 : 0);
  const char *yFmt     = hasNeg ? "%5.2f" : "%4.2f";

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  drawGraphAxes(u8g2, graphLeft, readings.deltaF, bounds, yFmt);
  drawGraphHistory(u8g2, graphLeft, graphGetHistory(), graphGetHistoryCount(), bounds);
  drawGraphStatusBar(u8g2, readings, totalBounds);
  u8g2.sendBuffer();
}

// ---------------------------------------------------------------------------
// Histogram
// ---------------------------------------------------------------------------

static void buildHistogramBuckets(const int *bins, int startBin, int endBin,
                                  int plotWidth, int *out) {
  const int visibleBinCount = endBin - startBin + 1;
  const bool aggregate      = visibleBinCount > plotWidth;

  for (int col = 0; col < plotWidth; ++col) {
    if (aggregate) {
      const int binStart   = startBin + (col       * visibleBinCount) / plotWidth;
      const int binEndExcl = startBin + ((col + 1) * visibleBinCount) / plotWidth;
      int count = 0;
      for (int i = binStart; i < binEndExcl; ++i) {
        if (i >= 0 && i < HISTOGRAM_BIN_COUNT) count += bins[i];
      }
      out[col] = count;
    } else {
      const int idx = startBin + col;
      out[col] = (idx >= 0 && idx < HISTOGRAM_BIN_COUNT) ? bins[idx] : 0;
    }
  }
}

static void drawHistogramYAxis(U8G2 &u8g2, int axisLeftX,
                               int yAxisMax, int yAxisMid) {
  char yTopBuf[10], yMidBuf[10];
  snprintf(yTopBuf, sizeof(yTopBuf), "%d", yAxisMax);
  snprintf(yMidBuf, sizeof(yMidBuf), "%d", yAxisMid);

  u8g2.drawStr(0, GRAPH_TOP + 7, yTopBuf);
  u8g2.drawStr(0, GRAPH_BOTTOM,  "0");

  if (yAxisMax > 1) {
    const int yMid = GRAPH_BOTTOM - static_cast<int>(
      (static_cast<float>(yAxisMid) / static_cast<float>(yAxisMax)) * (GRAPH_HEIGHT - 1) + 0.5f);
    int yMidLabelY = yMid + 3;
    if (yMidLabelY < GRAPH_TOP + 7) yMidLabelY = GRAPH_TOP + 7;
    if (yMidLabelY > GRAPH_BOTTOM)  yMidLabelY = GRAPH_BOTTOM;
    u8g2.drawHLine(axisLeftX - 1, yMid, 2);
    u8g2.drawStr(0, yMidLabelY, yMidBuf);
  }
}

static void drawHistogramXAxis(U8G2 &u8g2, int axisLeftX, int axisCenterX,
                               int axisRightX, float leftValue,
                               float centerValue, float rightValue) {
  char xMinBuf[10], xMidBuf[10], xMaxBuf[10];
  snprintf(xMinBuf, sizeof(xMinBuf), "%.2f", leftValue);
  snprintf(xMidBuf, sizeof(xMidBuf), "%.2f", centerValue);
  snprintf(xMaxBuf, sizeof(xMaxBuf), "%.2f", rightValue);

  const int xLabelY = GRAPH_BOTTOM + 10;
  u8g2.drawVLine(axisLeftX,   GRAPH_BOTTOM, 2);
  u8g2.drawVLine(axisCenterX, GRAPH_BOTTOM, 2);
  u8g2.drawVLine(axisRightX,  GRAPH_BOTTOM, 2);

  auto clampedX = [&](int x, int labelWidth) {
    if (x < 0) return 0;
    if (x > DISPLAY_WIDTH - labelWidth) return DISPLAY_WIDTH - labelWidth;
    return x;
  };
  u8g2.drawStr(clampedX(axisLeftX   - u8g2.getStrWidth(xMinBuf) / 2, u8g2.getStrWidth(xMinBuf)), xLabelY, xMinBuf);
  u8g2.drawStr(clampedX(axisCenterX - u8g2.getStrWidth(xMidBuf) / 2, u8g2.getStrWidth(xMidBuf)), xLabelY, xMidBuf);
  u8g2.drawStr(clampedX(axisRightX  - u8g2.getStrWidth(xMaxBuf) / 2, u8g2.getStrWidth(xMaxBuf)), xLabelY, xMaxBuf);
}

static void drawHistogramInfoBox(U8G2 &u8g2, int x, int y, int width,
                                 uint32_t sampleCount, float currentDiff,
                                 float maxCountDiff) {
  char sampleBuf[16], currentDiffBuf[16], maxCountDiffBuf[16];
  snprintf(sampleBuf,      sizeof(sampleBuf),      "N=%lu",  static_cast<unsigned long>(sampleCount));
  snprintf(currentDiffBuf, sizeof(currentDiffBuf), "%.2f",   currentDiff);
  snprintf(maxCountDiffBuf,sizeof(maxCountDiffBuf),"%.2f",   maxCountDiff);

  u8g2.setDrawColor(0);
  u8g2.drawBox(x - 1, GRAPH_TOP, width + 2, 24);
  u8g2.setDrawColor(1);
  u8g2.drawStr(x, y,      sampleBuf);
  u8g2.drawStr(x, y +  8, currentDiffBuf);
  u8g2.drawStr(x, y + 16, maxCountDiffBuf);
}

static void drawHistogramBars(U8G2 &u8g2, const int *buckets, int count,
                              int startX, int yAxisMax, int axisCenterX) {
  for (int col = 0; col < count; ++col) {
    if (buckets[col] <= 0) continue;
    int barHeight = static_cast<int>(
      (static_cast<float>(buckets[col]) / static_cast<float>(yAxisMax)) * (GRAPH_HEIGHT - 1) + 0.5f);
    if (barHeight < 1)             barHeight = 1;
    if (barHeight > GRAPH_HEIGHT - 1) barHeight = GRAPH_HEIGHT - 1;
    u8g2.drawVLine(startX + col, GRAPH_BOTTOM - barHeight, barHeight);
  }

  // Dotted vertical line at the fixed center.
  for (int y = GRAPH_TOP; y <= GRAPH_BOTTOM; y += 2) {
    u8g2.drawPixel(axisCenterX, y);
  }
}

void showHistogram(U8G2 &u8g2, const SensorReadings &readings) {
  const int     *bins        = histogramGetBins();
  const uint32_t sampleCount = histogramGetSampleCount();
  const float    centerValue = histogramGetCenterValueF();
  const int32_t  halfRange   = histogramGetHalfRange();
  const int      yAxisMax    = histogramGetLatchedMaxFrequency();
  const int      yAxisMid    = (yAxisMax > 1) ? ((yAxisMax + 1) / 2) : 0;

  // X-axis domain
  const float leftValue  = centerValue - static_cast<float>(halfRange) * HISTOGRAM_BIN_SIZE_F;
  const float rightValue = centerValue + static_cast<float>(halfRange) * HISTOGRAM_BIN_SIZE_F;
  const int   startBin   = HISTOGRAM_CENTER_INDEX - static_cast<int>(halfRange);
  const int   endBin     = HISTOGRAM_CENTER_INDEX + static_cast<int>(halfRange);

  // Y-axis label width determines left margin
  char yTopBuf[10];
  snprintf(yTopBuf, sizeof(yTopBuf), "%d", yAxisMax);
  const int yLabelWidth = u8g2.getStrWidth(yTopBuf);
  const int axisLeftX   = yLabelWidth + 2;
  const int plotLeftX   = axisLeftX + 1;
  int plotWidth = DISPLAY_WIDTH - plotLeftX;
  if (plotWidth > GRAPH_WIDTH) plotWidth = GRAPH_WIDTH;
  if (plotWidth < 1)           plotWidth = 1;

  // Info box width (right-side overlay): widest of the three stat strings
  char currentDiffBuf[16], maxCountDiffBuf[16];
  const int maxBinIndex = [&]() {
    int best = HISTOGRAM_CENTER_INDEX, bestCount = 0;
    for (int i = 0; i < HISTOGRAM_BIN_COUNT; ++i) {
      if (bins[i] > bestCount) { bestCount = bins[i]; best = i; }
    }
    return best;
  }();
  const float maxCountDiffValue = centerValue +
    static_cast<float>(maxBinIndex - HISTOGRAM_CENTER_INDEX) * HISTOGRAM_BIN_SIZE_F;
  snprintf(currentDiffBuf,  sizeof(currentDiffBuf),  "%.2f", readings.deltaF);
  snprintf(maxCountDiffBuf, sizeof(maxCountDiffBuf), "%.2f", maxCountDiffValue);

  char sampleBuf[16];
  snprintf(sampleBuf, sizeof(sampleBuf), "N=%lu", static_cast<unsigned long>(sampleCount));
  int infoWidth = u8g2.getStrWidth(sampleBuf);
  int w = u8g2.getStrWidth(currentDiffBuf);  if (w > infoWidth) infoWidth = w;
      w = u8g2.getStrWidth(maxCountDiffBuf); if (w > infoWidth) infoWidth = w;
  const int infoX = DISPLAY_WIDTH - infoWidth - 1;

  // Build buckets: one pixel per bin when possible, aggregate otherwise
  int visibleBinCount = endBin - startBin + 1;
  int activePlotWidth = (visibleBinCount > plotWidth) ? plotWidth : visibleBinCount;
  int activePlotLeftX = plotLeftX + ((plotWidth - activePlotWidth) / 2);
  int axisRightX      = plotLeftX + plotWidth - 1;
  int axisCenterX     = plotLeftX + (plotWidth - 1) / 2;

  int buckets[GRAPH_WIDTH] = {};
  buildHistogramBuckets(bins, startBin, endBin, activePlotWidth, buckets);

  // Draw
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  u8g2.drawVLine(axisLeftX, GRAPH_TOP, GRAPH_HEIGHT);
  u8g2.drawHLine(axisLeftX, GRAPH_BOTTOM, axisRightX - axisLeftX + 1);

  drawHistogramYAxis(u8g2, axisLeftX, yAxisMax, yAxisMid);
  drawHistogramXAxis(u8g2, axisLeftX, axisCenterX, axisRightX,
                     leftValue, centerValue, rightValue);
  drawHistogramInfoBox(u8g2, infoX, GRAPH_TOP + 6, infoWidth,
                       sampleCount, readings.deltaF, maxCountDiffValue);

  if (yAxisMax > 0) {
    drawHistogramBars(u8g2, buckets, activePlotWidth, activePlotLeftX,
                      yAxisMax, axisCenterX);
  }

  u8g2.sendBuffer();
}

// ---------------------------------------------------------------------------
// Info panels
// ---------------------------------------------------------------------------

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
  I2CScanResult scanResult  = i2cGetLastScanResult();
  int           foundCount  = static_cast<int>(scanResult.count);
  const int     maxWidth    = DISPLAY_WIDTH - 4;

  String foundLine1 = "Found: " + String(foundCount) + " device(s)";
  String foundLine2 = "";

  // Word-wrap the "Found:" line if it overflows (rare but possible with large counts).
  if (u8g2.getStrWidth(foundLine1.c_str()) > maxWidth) {
    for (int split = foundLine1.length() - 1; split > 0; --split) {
      if (foundLine1.charAt(split) != ' ') continue;
      String candidate = foundLine1.substring(0, split);
      if (u8g2.getStrWidth(candidate.c_str()) <= maxWidth) {
        foundLine2 = foundLine1.substring(split + 1);
        foundLine1 = candidate;
        break;
      }
    }
  }

  // Pack addresses into up to two lines, truncating with "..." if needed.
  String line1, line2;
  auto appendToken = [&](String &target, const String &token) -> bool {
    String candidate = target.isEmpty() ? token : (target + " " + token);
    if (u8g2.getStrWidth(candidate.c_str()) <= maxWidth) {
      target = candidate;
      return true;
    }
    return false;
  };

  if (foundCount == 0) {
    line1 = "None";
  } else {
    bool overflow = false;
    for (size_t i = 0; i < scanResult.count; ++i) {
      char addrBuf[8];
      snprintf(addrBuf, sizeof(addrBuf), "0x%02X", scanResult.addresses[i]);
      String token(addrBuf);
      if (!appendToken(line1, token) && !appendToken(line2, token)) {
        overflow = true;
      }
    }
    if (overflow) {
      const String suffix = " ...";
      while (!line2.isEmpty() && u8g2.getStrWidth((line2 + suffix).c_str()) > maxWidth) {
        line2.remove(line2.length() - 1);
      }
      line2.trim();
      line2 += suffix;
    }
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
  RtcStatus   status = rtcGetStatus();
  String      rtcNow = rtcGetCurrentTimeString();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  drawTitleBar(u8g2, "RTC Status", remainingSecs);

  u8g2.drawStr(2, 20, rtcNow.c_str());
  u8g2.drawStr(2, 29, (String("Present:") + (status.present     ? "Yes" : "No")).c_str());
  u8g2.drawStr(2, 38, (String("LostPwr:") + (status.powerLost   ? "Yes" : "No")).c_str());
  u8g2.drawStr(2, 47, (String("Battery:") + (status.lowBattery  ? "Low" : "OK")).c_str());
  u8g2.drawStr(2, 56, (String("SQW 1Hz:") + (status.sqwConfigured ? "Yes" : "No")).c_str());

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
