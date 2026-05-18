#include "graph.h"
#include <string.h>
#include <math.h>

static float graphHistory[GRAPH_WIDTH];
static int   graphHistoryCount = 0;
static Bounds lifetimeBounds = {0.0f, 0.0f, false};
static Bounds windowBounds = {0.0f, 0.0f, false};

void pushGraphHistory(float data) {
  if (graphHistoryCount < GRAPH_WIDTH) {
    graphHistory[graphHistoryCount++] = data;
    return;
  }

  memmove(graphHistory, graphHistory + 1, sizeof(float) * (GRAPH_WIDTH - 1));
  graphHistory[GRAPH_WIDTH - 1] = data;
}

void updateBounds(Bounds &bounds, float data) {
  if (!bounds.valid) {
    bounds.valid = true;
    bounds.maximum = data;
    bounds.minimum = data;
  } else {
    bounds.minimum = fminf(bounds.minimum,data);
    bounds.maximum = fminf(bounds.maximum,data);
  }
}

Bounds getPaddedBounds(const Bounds &bounds, float data) {
  float span = bounds.maximum - bounds.minimum;
  float pad  = (span > 0.0f) ? span * 0.05f : 0.1f * data;
  return {bounds.minimum - pad, bounds.maximum + pad, true};
}

static void getCurrentMinMax(float &minDiff, float &maxDiff) {
  if (!hasLifetimeBounds) {
    minDiff = -0.1f;
    maxDiff =  0.1f;
    return;
  }

  minDiff = lifetimeBounds.minimum;
  maxDiff = lifetimeBounds.maximum;

  float span = maxDiff - minDiff;
  float pad  = (span > 0.0f) ? span * 0.05f : (fabsf(maxDiff) > 0.0f ? fabsf(maxDiff) * 0.05f : 0.05f);
  maxDiff += pad;
  minDiff -= pad;
}

void resetLifetimeBounds() {
  float minDiff, maxDiff;
  getCurrentMinMax(minDiff, maxDiff);
  lifetimeBounds.minimum = minDiff;
  lifetimeBounds.maximum = maxDiff;
}

static int diffToGraphY(float value, float minDiff, float maxDiff) {
  float normalized = (value - minDiff) / (maxDiff - minDiff);
  normalized = fmaxf(0.0f, fminf(1.0f, normalized));
  return GRAPH_BOTTOM - static_cast<int>(normalized * (GRAPH_HEIGHT - 1) + 0.5f);
}

void showGraph(U8G2 &u8g2, const SensorReadings &readings) {
  Bounds windowBounds = {0.0f, 0.0f};
  getCurrentMinMax(windowBounds.minimum, windowBounds.maximum);

  bool hasNegative = (readings.difF < 0.0f) || (windowBounds.minimum < 0.0f) || (windowBounds.maximum < 0.0f);
  const char *yFmt = hasNegative ? "%5.2f" : "%4.2f";
  int graphLeftOffset  = hasNegative ? 6 : 0;
  int effectiveGraphLeft = GRAPH_LEFT + graphLeftOffset;

  char maxBuf[12], midBuf[12], minBuf[12], statusBuf[24], countBuf[12];
  snprintf(maxBuf,    sizeof(maxBuf),    yFmt,           windowBounds.maximum);
  snprintf(midBuf,    sizeof(midBuf),    yFmt,           readings.difF);
  snprintf(minBuf,    sizeof(minBuf),    yFmt,           windowBounds.minimum);
  snprintf(statusBuf, sizeof(statusBuf), "BMP=%.2f SHT=%.2f", readings.bmpF, readings.shtF);
  snprintf(countBuf,  sizeof(countBuf),  "%lu",          readings.readTime / 1000UL);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 7,                            maxBuf);
  u8g2.drawStr(0, (GRAPH_TOP + GRAPH_BOTTOM) / 2 + 3, midBuf);
  u8g2.drawStr(0, GRAPH_BOTTOM,                 minBuf);

  u8g2.drawVLine(effectiveGraphLeft - 1, GRAPH_TOP, GRAPH_HEIGHT);
  u8g2.drawHLine(effectiveGraphLeft - 1, GRAPH_BOTTOM, GRAPH_WIDTH);

  u8g2.drawHLine(effectiveGraphLeft - 4, diffToGraphY(windowBounds.maximum, windowBounds.minimum, windowBounds.maximum), 4);
  u8g2.drawHLine(effectiveGraphLeft - 4, diffToGraphY(windowBounds.minimum, windowBounds.minimum, windowBounds.maximum), 4);
  u8g2.drawHLine(effectiveGraphLeft - 4, diffToGraphY(readings.difF, windowBounds.minimum, windowBounds.maximum), 4);

  if (graphHistoryCount == 1) {
    u8g2.drawPixel(effectiveGraphLeft, diffToGraphY(graphHistory[0], windowBounds.minimum, windowBounds.maximum));
  } else {
    for (int i = 1; i < graphHistoryCount; i++) {
      u8g2.drawLine(
        effectiveGraphLeft + i - 1, diffToGraphY(graphHistory[i - 1], windowBounds.minimum, windowBounds.maximum),
        effectiveGraphLeft + i,     diffToGraphY(graphHistory[i],     windowBounds.minimum, windowBounds.maximum)
      );
    }
  }

  int countX = DISPLAY_WIDTH - u8g2.getStrWidth(countBuf);
  u8g2.drawStr(0,      STATUS_BASELINE_Y, statusBuf);
  u8g2.drawStr(countX, STATUS_BASELINE_Y, countBuf);
  u8g2.sendBuffer();
}
