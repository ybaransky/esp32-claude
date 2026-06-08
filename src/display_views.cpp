#include "display_views.h"

#include <Arduino.h>
#include <math.h>
#include "display_layout.h"
#include "hardware.h"
#include "rtc_ds3231.h"

static Bounds getPaddedBounds(const Bounds &bounds) {
  const float span = bounds.maximum - bounds.minimum;
  const float pad  = (span > 0.0f) ? span * 0.05f : 0.1f;
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
  u8g2.drawStr(0, GRAPH_BOTTOM,                       minBuf);

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

static void drawGraphStatusBar(U8G2 &u8g2, const SensorReadings &readings,
                               const Bounds &totalBounds) {
  const char *sensorFmt = (readings.bmpF < 0.0f || readings.shtF < 0.0f) ? "%6.2f" : "%5.2f";
  const char *minmaxFmt = (totalBounds.minimum < 0.0f || totalBounds.maximum < 0.0f) ? "%5.2f" : "%4.2f";

  char bmpStr[10], shtStr[10], minStr[8], maxStr[8], countBuf[12];
  snprintf(bmpStr,   sizeof(bmpStr),   sensorFmt, readings.bmpF);
  snprintf(shtStr,   sizeof(shtStr),   sensorFmt, readings.shtF);
  snprintf(minStr,   sizeof(minStr),   minmaxFmt, totalBounds.minimum);
  snprintf(maxStr,   sizeof(maxStr),   minmaxFmt, totalBounds.maximum);
  snprintf(countBuf, sizeof(countBuf), "%lu",     readings.readTime / 1000UL);

  char statusBuf[28], minmaxBuf[28];
  snprintf(statusBuf, sizeof(statusBuf), "BMP=%s  min=%s", bmpStr, minStr);
  snprintf(minmaxBuf, sizeof(minmaxBuf), "SHT=%s  max=%s", shtStr, maxStr);

  const int countX = DISPLAY_WIDTH - u8g2.getStrWidth(countBuf);
  u8g2.drawStr(0,      STATUS_BASELINE_Y, statusBuf);
  u8g2.drawStr(0,      MINMAX_BASELINE_Y, minmaxBuf);
  u8g2.drawStr(countX, MINMAX_BASELINE_Y, countBuf);
}

GraphViewData graphBuildViewData(const SensorReadings &readings) {
  return {
    readings,
    graphGetGraphBounds(),
    graphGetTotalBounds(),
    graphGetHistory(),
    graphGetHistoryCount(),
  };
}

void showGraph(U8G2 &u8g2, const GraphViewData &view) {
  const Bounds bounds = getPaddedBounds(view.graphBounds);
  const bool hasNeg = view.readings.deltaF < 0.0f || bounds.minimum < 0.0f || bounds.maximum < 0.0f;
  const int graphLeft = GRAPH_LEFT + (hasNeg ? 6 : 0);
  const char *yFmt = hasNeg ? "%5.2f" : "%4.2f";

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  drawGraphAxes(u8g2, graphLeft, view.readings.deltaF, bounds, yFmt);
  drawGraphHistory(u8g2, graphLeft, view.history, view.historyCount, bounds);
  drawGraphStatusBar(u8g2, view.readings, view.totalBounds);
  u8g2.sendBuffer();
}

static void buildHistogramBuckets(const int *bins, int startBin, int endBin,
                                  int plotWidth, int *out) {
  const int visibleBinCount = endBin - startBin + 1;
  const bool aggregate = visibleBinCount > plotWidth;

  for (int col = 0; col < plotWidth; ++col) {
    if (aggregate) {
      const int binStart = startBin + (col * visibleBinCount) / plotWidth;
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
  u8g2.drawStr(0, GRAPH_BOTTOM, "0");

  if (yAxisMax > 1) {
    const int yMid = GRAPH_BOTTOM - static_cast<int>(
      (static_cast<float>(yAxisMid) / static_cast<float>(yAxisMax)) * (GRAPH_HEIGHT - 1) + 0.5f);
    int yMidLabelY = yMid + 3;
    if (yMidLabelY < GRAPH_TOP + 7) yMidLabelY = GRAPH_TOP + 7;
    if (yMidLabelY > GRAPH_BOTTOM) yMidLabelY = GRAPH_BOTTOM;
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
  u8g2.drawVLine(axisLeftX, GRAPH_BOTTOM, 2);
  u8g2.drawVLine(axisCenterX, GRAPH_BOTTOM, 2);
  u8g2.drawVLine(axisRightX, GRAPH_BOTTOM, 2);

  const int wMin = u8g2.getStrWidth(xMinBuf);
  const int wMid = u8g2.getStrWidth(xMidBuf);
  const int wMax = u8g2.getStrWidth(xMaxBuf);

  auto clampedX = [](int x, int labelWidth) {
    if (x < 0) return 0;
    if (x > DISPLAY_WIDTH - labelWidth) return DISPLAY_WIDTH - labelWidth;
    return x;
  };

  u8g2.drawStr(clampedX(axisLeftX   - wMin / 2, wMin), xLabelY, xMinBuf);
  u8g2.drawStr(clampedX(axisCenterX - wMid / 2, wMid), xLabelY, xMidBuf);
  u8g2.drawStr(clampedX(axisRightX  - wMax / 2, wMax), xLabelY, xMaxBuf);
}

static void drawHistogramInfoBox(U8G2 &u8g2, int x, int y, int width,
                                 const char *line1, const char *line2,
                                 const char *line3) {
  u8g2.setDrawColor(0);
  u8g2.drawBox(x - 1, GRAPH_TOP, width + 2, 24);
  u8g2.setDrawColor(1);
  u8g2.drawStr(x, y,      line1);
  u8g2.drawStr(x, y + 8,  line2);
  u8g2.drawStr(x, y + 16, line3);
}

static void drawHistogramBars(U8G2 &u8g2, const int *buckets, int count,
                              int startX, int yAxisMax) {
  for (int col = 0; col < count; ++col) {
    if (buckets[col] <= 0) continue;

    int barHeight = static_cast<int>(
      (static_cast<float>(buckets[col]) / static_cast<float>(yAxisMax)) * (GRAPH_HEIGHT - 1) + 0.5f);
    if (barHeight < 1) barHeight = 1;
    if (barHeight > GRAPH_HEIGHT - 1) barHeight = GRAPH_HEIGHT - 1;
    u8g2.drawVLine(startX + col, GRAPH_BOTTOM - barHeight, barHeight);
  }
}

HistogramViewData histogramBuildViewData(const SensorReadings &readings) {
  return {
    readings,
    histogramGetBins(),
    histogramGetBinCount(),
    histogramGetCenterValueF(),
    histogramGetHalfRange(),
    histogramGetLatchedMaxFrequency(),
    histogramGetSampleCount(),
    histogramGetPeakBinIndex(),
  };
}

void showHistogram(U8G2 &u8g2, const HistogramViewData &view) {
  const int yAxisMax = view.latchedMaxFrequency;
  const int yAxisMid = (yAxisMax > 1) ? ((yAxisMax + 1) / 2) : 0;

  const float leftValue = view.centerValue - static_cast<float>(view.halfRange) * HISTOGRAM_BIN_SIZE_F;
  const float rightValue = view.centerValue + static_cast<float>(view.halfRange) * HISTOGRAM_BIN_SIZE_F;
  const int startBin = HISTOGRAM_CENTER_INDEX - static_cast<int>(view.halfRange);
  const int endBin = HISTOGRAM_CENTER_INDEX + static_cast<int>(view.halfRange);

  char yTopBuf[10];
  snprintf(yTopBuf, sizeof(yTopBuf), "%d", yAxisMax);
  const int yLabelWidth = u8g2.getStrWidth(yTopBuf);
  const int axisLeftX = yLabelWidth + 2;
  const int plotLeftX = axisLeftX + 1;
  int plotWidth = DISPLAY_WIDTH - plotLeftX;
  if (plotWidth > GRAPH_WIDTH) plotWidth = GRAPH_WIDTH;
  if (plotWidth < 1) plotWidth = 1;

  const float maxCountDiffValue = view.centerValue +
    static_cast<float>(view.peakBinIndex - HISTOGRAM_CENTER_INDEX) * HISTOGRAM_BIN_SIZE_F;

  char sampleBuf[16], currentDiffBuf[16], maxCountDiffBuf[16];
  snprintf(sampleBuf,       sizeof(sampleBuf),       "N=%lu", static_cast<unsigned long>(view.sampleCount));
  snprintf(currentDiffBuf,  sizeof(currentDiffBuf),  "%.2f",  view.readings.deltaF);
  snprintf(maxCountDiffBuf, sizeof(maxCountDiffBuf), "%.2f",  maxCountDiffValue);

  int infoWidth = u8g2.getStrWidth(sampleBuf);
  int w = u8g2.getStrWidth(currentDiffBuf);  if (w > infoWidth) infoWidth = w;
      w = u8g2.getStrWidth(maxCountDiffBuf); if (w > infoWidth) infoWidth = w;
  const int infoX = DISPLAY_WIDTH - infoWidth - 1;

  const int visibleBinCount = endBin - startBin + 1;
  const int activePlotWidth = (visibleBinCount > plotWidth) ? plotWidth : visibleBinCount;
  const int activePlotLeftX = plotLeftX + ((plotWidth - activePlotWidth) / 2);
  const int axisRightX = plotLeftX + plotWidth - 1;
  const int axisCenterX = plotLeftX + (plotWidth - 1) / 2;

  int buckets[GRAPH_WIDTH] = {};
  buildHistogramBuckets(view.bins, startBin, endBin, activePlotWidth, buckets);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);

  u8g2.drawVLine(axisLeftX, GRAPH_TOP, GRAPH_HEIGHT);
  u8g2.drawHLine(axisLeftX, GRAPH_BOTTOM, axisRightX - axisLeftX + 1);

  drawHistogramYAxis(u8g2, axisLeftX, yAxisMax, yAxisMid);
  drawHistogramXAxis(u8g2, axisLeftX, axisCenterX, axisRightX,
                     leftValue, view.centerValue, rightValue);
  drawHistogramInfoBox(u8g2, infoX, GRAPH_TOP + 6, infoWidth,
                       sampleBuf, currentDiffBuf, maxCountDiffBuf);

  if (yAxisMax > 0) {
    drawHistogramBars(u8g2, buckets, activePlotWidth, activePlotLeftX, yAxisMax);
  }

  u8g2.sendBuffer();
}
