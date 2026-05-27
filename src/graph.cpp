#include <Arduino.h>
#include "graph.h"
#include <string.h>
#include <math.h>

static float graphHistory[GRAPH_WIDTH];
static int   graphHistoryCount = 0;
static Bounds totalBounds = {0.0f, 0.0f, false};
static Bounds graphBounds = {0.0f, 0.0f, false};
static int histogramBins[HISTOGRAM_BIN_COUNT] = {0};
static double histogramMeanScaled = 0.0;
static uint32_t histogramSampleCount = 0;
static double histogramHalfRangeScaled = 1.0;
static int32_t histogramMinScaled = 0;
static int32_t histogramMaxScaled = 0;
static bool histogramRangeInitialized = false;
static int histogramLatchedMaxFrequency = 0;

static double computeHalfRangeScaled(int32_t minScaled, int32_t maxScaled, double meanScaled) {
  const double minDelta = fabs(meanScaled - static_cast<double>(minScaled));
  const double maxDelta = fabs(static_cast<double>(maxScaled) - meanScaled);
  const double halfRange = fmax(minDelta, maxDelta);
  return (halfRange < 1.0) ? 1.0 : halfRange;
}

static int histogramIndexForScaledValue(double scaledValue, double meanScaled, double halfRangeScaled) {
  if (halfRangeScaled <= 0.0) {
    return HISTOGRAM_CENTER_INDEX;
  }

  const double normalized = (scaledValue - (meanScaled - halfRangeScaled)) / (2.0 * halfRangeScaled);
  int index = static_cast<int>(lround(normalized * static_cast<double>(HISTOGRAM_BIN_COUNT - 1)));
  if (index < 0) {
    index = 0;
  } else if (index >= HISTOGRAM_BIN_COUNT) {
    index = HISTOGRAM_BIN_COUNT - 1;
  }
  return index;
}

static double histogramScaledValueForIndex(int index, double meanScaled, double halfRangeScaled) {
  if (HISTOGRAM_BIN_COUNT <= 1) {
    return meanScaled;
  }

  const double ratio = static_cast<double>(index) / static_cast<double>(HISTOGRAM_BIN_COUNT - 1);
  return (meanScaled - halfRangeScaled) + (2.0 * halfRangeScaled * ratio);
}

static void remapHistogramBins(double oldMeanScaled, double oldHalfRangeScaled, double newMeanScaled, double newHalfRangeScaled) {
  int remapped[HISTOGRAM_BIN_COUNT] = {0};

  for (int i = 0; i < HISTOGRAM_BIN_COUNT; ++i) {
    const int count = histogramBins[i];
    if (count <= 0) {
      continue;
    }

    const double scaledValue = histogramScaledValueForIndex(i, oldMeanScaled, oldHalfRangeScaled);
    const int newIndex = histogramIndexForScaledValue(scaledValue, newMeanScaled, newHalfRangeScaled);
    remapped[newIndex] += count;
  }

  memcpy(histogramBins, remapped, sizeof(histogramBins));
}

static void updateHistogramLatchedMaxFrequency() {
  int currentMax = 0;
  for (int i = 0; i < HISTOGRAM_BIN_COUNT; ++i) {
    if (histogramBins[i] > currentMax) {
      currentMax = histogramBins[i];
    }
  }
  if (currentMax > histogramLatchedMaxFrequency) {
    histogramLatchedMaxFrequency = currentMax;
  }
}

static void pushHistogram(float data) {
  const int32_t scaledValue = static_cast<int32_t>(lround(static_cast<double>(data) * HISTOGRAM_VALUE_SCALE));

  if (!histogramRangeInitialized) {
    histogramRangeInitialized = true;
    histogramMinScaled = scaledValue;
    histogramMaxScaled = scaledValue;
  } else {
    if (scaledValue < histogramMinScaled) {
      histogramMinScaled = scaledValue;
    }
    if (scaledValue > histogramMaxScaled) {
      histogramMaxScaled = scaledValue;
    }
  }

  const double oldMeanScaled = histogramMeanScaled;
  const double oldHalfRangeScaled = histogramHalfRangeScaled;

  histogramSampleCount++;
  histogramMeanScaled += (static_cast<double>(scaledValue) - histogramMeanScaled) / static_cast<double>(histogramSampleCount);
  histogramHalfRangeScaled = computeHalfRangeScaled(histogramMinScaled, histogramMaxScaled, histogramMeanScaled);

  if (histogramSampleCount > 1) {
    remapHistogramBins(oldMeanScaled, oldHalfRangeScaled, histogramMeanScaled, histogramHalfRangeScaled);
  }

  const int index = histogramIndexForScaledValue(static_cast<double>(scaledValue), histogramMeanScaled, histogramHalfRangeScaled);
  histogramBins[index]++;
  updateHistogramLatchedMaxFrequency();
}

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

void resetGraphBoundsType1(void) {
  Serial.println("[GRAPH] Resetting graph bounds Type 1");
  float min = graphHistory[0];
  float max = graphHistory[0];
  for (int i=1; i < graphHistoryCount; i++) {
    min = fminf(min, graphHistory[i]);
    max = fmaxf(max, graphHistory[i]);
  }
  graphBounds = {min, max, true};
}

void resetGraphBoundsType2(void) {
  Serial.println("[GRAPH] Resetting graph bounds Type 2");
  float value = graphHistory[graphHistoryCount - 1];
  memset(graphHistory, 0, sizeof(graphHistory));
  graphHistoryCount = 0;
  graphHistory[0] = value;
  if (value >= 0.0f) {
    graphBounds = {0.99f*value, 1.01f*value, true};
  } else {
    graphBounds = {1.01f*value, 0.99f*value, true};
  }
}
void updateDataBounds(float data) {
  updateBounds(totalBounds, data);
  updateBounds(graphBounds, data);
  pushHistogram(data);
}

void pushGraphHistory(float data) {
  if (graphHistoryCount < GRAPH_WIDTH) {
    graphHistory[graphHistoryCount++] = data;
    return;
  }
  memmove(graphHistory, graphHistory + 1, sizeof(float) * (GRAPH_WIDTH - 1));
  graphHistory[GRAPH_WIDTH - 1] = data;
}

void resetGraphBounds(int type) {
  Serial.println("[GRAPH] Resetting graph bounds");
  if (type==1) {
    resetGraphBoundsType1();
  } else if (type==2) {
    resetGraphBoundsType2();
  } else {
    Serial.println("[GRAPH] Invalid graph bounds reset type");
  }
}

void resetGraphAndHistogram() {
  Serial.println("[GRAPH] Full reset: bounds/history/histogram");
  memset(graphHistory, 0, sizeof(graphHistory));
  graphHistoryCount = 0;

  totalBounds = {0.0f, 0.0f, false};
  graphBounds = {0.0f, 0.0f, false};

  memset(histogramBins, 0, sizeof(histogramBins));
  histogramMeanScaled = 0.0;
  histogramSampleCount = 0;
  histogramHalfRangeScaled = 1.0;
  histogramMinScaled = 0;
  histogramMaxScaled = 0;
  histogramRangeInitialized = false;
  histogramLatchedMaxFrequency = 0;
}

Bounds graphGetGraphBounds() { return graphBounds; }
Bounds graphGetTotalBounds() { return totalBounds; }
const float *graphGetHistory() { return graphHistory; }
int graphGetHistoryCount()    { return graphHistoryCount; }
const int *graphGetHistogram() { return histogramBins; }
int graphGetHistogramBinCount() { return HISTOGRAM_BIN_COUNT; }
double graphGetHistogramMean() { return histogramMeanScaled * HISTOGRAM_BIN_SIZE_F; }
double graphGetHistogramMeanScaled() { return histogramMeanScaled; }
double graphGetHistogramHalfRangeScaled() { return histogramHalfRangeScaled; }
int graphGetHistogramLatchedMaxFrequency() { return histogramLatchedMaxFrequency; }
uint32_t graphGetHistogramSampleCount() { return histogramSampleCount; }
