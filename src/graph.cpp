#include "graph.h"
#include <string.h>
#include <math.h>

static float graphHistory[GRAPH_WIDTH];
static int   graphHistoryCount = 0;
static Bounds totalBounds = {0.0f, 0.0f, false};
static Bounds graphBounds = {0.0f, 0.0f, false};

static void updateBounds(Bounds &bounds, float data) {
  if (!bounds.initialized) {
    bounds.initialized = true;
    bounds.maximum     = data;
    bounds.minimum     = data;
  } else {
    bounds.minimum = fminf(bounds.minimum,data);
    bounds.maximum = fmaxf(bounds.maximum,data);
  }
}
static Bounds getPaddedBounds(const Bounds &bounds, float data) {
  float span = bounds.maximum - bounds.minimum;
  float pad  = (span > 0.0f) ? span * 0.05f : 0.1f * data;
  return {bounds.minimum - pad, bounds.maximum + pad, true};
}

static int diffToGraphY(float value, const Bounds& bounds) {
  float normalized = (value - bounds.minimum) / (bounds.maximum - bounds.minimum);
  normalized = fmaxf(0.0f, fminf(1.0f, normalized));
  return GRAPH_BOTTOM - static_cast<int>(normalized * (GRAPH_HEIGHT - 1) + 0.5f);
}

void updateDataBounds(float data) {
  updateBounds(totalBounds, data);
  updateBounds(graphBounds, data); 
}

void pushGraphHistory(float data) {
  if (graphHistoryCount < GRAPH_WIDTH) {
    graphHistory[graphHistoryCount++] = data;
    return;
  }
  memmove(graphHistory, graphHistory + 1, sizeof(float) * (GRAPH_WIDTH - 1));
  graphHistory[GRAPH_WIDTH - 1] = data;
}

void resetGraphBounds(void) {
  Serial.println("[GRAPH] Resetting graph bounds");
  // clear out the graph
  float value = graphHistory[graphHistoryCount - 1];
  memset(graphHistory, 0, sizeof(graphHistory));

  // keep the most recent value
  graphHistoryCount = 0;
  graphHistory[0] = value;
  if (value >= 0.0f) {
    graphBounds = {0.99f*value, 1.01f*value, true};
  } else {
    graphBounds = {1.01f*value, 0.99f*value, true};
  }
}

void showGraph(U8G2 &u8g2, const SensorReadings &readings) {
  float value = readings.deltaF;
  Bounds bounds = getPaddedBounds(graphBounds, value);

  bool hasNegative = (value < 0.0f) || (bounds.minimum < 0.0f) || (bounds.maximum < 0.0f);
  int graphLeftOffset  = hasNegative ? 6 : 0;
  int effectiveGraphLeft = GRAPH_LEFT + graphLeftOffset;

  const char *yFmt      = hasNegative ? "%5.2f" : "%4.2f";
  const char *sensorFmt = (readings.bmpF < 0.0f )       | (readings.shtF < 0.0f)       ? "%6.2f" : "%5.2f";
  const char *minmaxFmt = (totalBounds.minimum < 0.0f ) | (totalBounds.maximum < 0.0f) ? "%5.2f" : "%4.2f";

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
  u8g2.drawStr(0, 7,                            maxBuf);
  u8g2.drawStr(0, (GRAPH_TOP + GRAPH_BOTTOM) / 2 + 3, midBuf);
  u8g2.drawStr(0, GRAPH_BOTTOM,                 minBuf);

  u8g2.drawVLine(effectiveGraphLeft - 1, GRAPH_TOP, GRAPH_HEIGHT);
  u8g2.drawHLine(effectiveGraphLeft - 1, GRAPH_BOTTOM, GRAPH_WIDTH);

  u8g2.drawHLine(effectiveGraphLeft - 4, diffToGraphY(value,bounds), 4);
  u8g2.drawHLine(effectiveGraphLeft - 4, diffToGraphY(value,bounds), 4);
  u8g2.drawHLine(effectiveGraphLeft - 4, diffToGraphY(value,bounds), 4);

  if (graphHistoryCount == 1) {
    u8g2.drawPixel(effectiveGraphLeft, diffToGraphY(graphHistory[0], bounds));
  } else {
    for (int i = 1; i < graphHistoryCount; i++) {
      u8g2.drawLine(
        effectiveGraphLeft + i - 1, diffToGraphY(graphHistory[i - 1], bounds),
        effectiveGraphLeft + i,     diffToGraphY(graphHistory[i],     bounds)
      );
    }
  }

  int countX = DISPLAY_WIDTH - u8g2.getStrWidth(countBuf);
  u8g2.drawStr(0,      STATUS_BASELINE_Y, statusBuf);
  u8g2.drawStr(0,      MINMAX_BASELINE_Y, minmaxBuf);
  u8g2.drawStr(countX, MINMAX_BASELINE_Y, countBuf);
  u8g2.sendBuffer();
}
