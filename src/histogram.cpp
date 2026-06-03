#include "histogram.h"

#include <math.h>
#include <string.h>
#include <Arduino.h>

struct HistogramState {
  int      bins[HISTOGRAM_BIN_COUNT];
  int32_t  centerScaledValue;
  bool     initialized;
  int32_t  minScaledOffset;
  int32_t  maxScaledOffset;
  int      latchedMaxFrequency;
  uint32_t sampleCount;
};

static HistogramState histogramState = {};

void histogramUpdateData(float data) {
  const int32_t scaledValue = static_cast<int32_t>(lround(static_cast<double>(data) * HISTOGRAM_VALUE_SCALE));

  if (!histogramState.initialized) {
    histogramState.centerScaledValue = scaledValue;
    histogramState.initialized       = true;
  }

  const int32_t offset = scaledValue - histogramState.centerScaledValue;
  const int index = HISTOGRAM_CENTER_INDEX + static_cast<int>(offset);

  if (index < 0 || index >= HISTOGRAM_BIN_COUNT) {
    Serial.printf("[HIST] Value out of range: %.2f (offset %ld)\n",
                  data, static_cast<long>(offset));
    return;
  }

  histogramState.bins[index]++;
  histogramState.sampleCount++;

  if (histogramState.sampleCount == 1) {
    histogramState.minScaledOffset = offset;
    histogramState.maxScaledOffset = offset;
  } else {
    if (offset < histogramState.minScaledOffset) histogramState.minScaledOffset = offset;
    if (offset > histogramState.maxScaledOffset) histogramState.maxScaledOffset = offset;
  }

  if (histogramState.bins[index] > histogramState.latchedMaxFrequency) {
    histogramState.latchedMaxFrequency = histogramState.bins[index];
  }
}

void histogramRecenterOnPeak() {
  if (!histogramState.initialized || histogramState.sampleCount == 0) return;

  int peakIndex = HISTOGRAM_CENTER_INDEX;
  int peakCount = 0;
  for (int i = 0; i < HISTOGRAM_BIN_COUNT; ++i) {
    if (histogramState.bins[i] > peakCount) {
      peakCount = histogramState.bins[i];
      peakIndex = i;
    }
  }

  if (peakIndex == HISTOGRAM_CENTER_INDEX) return;

  // Shift all bins so the peak lands at HISTOGRAM_CENTER_INDEX.
  const int shift = peakIndex - HISTOGRAM_CENTER_INDEX;
  int newBins[HISTOGRAM_BIN_COUNT] = {};
  for (int i = 0; i < HISTOGRAM_BIN_COUNT; ++i) {
    const int dest = i - shift;
    if (dest >= 0 && dest < HISTOGRAM_BIN_COUNT) {
      newBins[dest] = histogramState.bins[i];
    }
  }
  memcpy(histogramState.bins, newBins, sizeof(histogramState.bins));

  // The new center is the peak's former scaled value.
  histogramState.centerScaledValue += shift;
  // Offsets are relative to center, so they shrink by the same shift.
  histogramState.minScaledOffset -= shift;
  histogramState.maxScaledOffset -= shift;

  Serial.printf("[HIST] Recentered on peak at %.2f\n",
                static_cast<float>(histogramState.centerScaledValue) * HISTOGRAM_BIN_SIZE_F);
}

void histogramReset() {
  histogramState = {};
}

const int *histogramGetBins()  { return histogramState.bins; }
int  histogramGetBinCount()    { return HISTOGRAM_BIN_COUNT; }

float histogramGetCenterValueF() {
  return static_cast<float>(histogramState.centerScaledValue) * HISTOGRAM_BIN_SIZE_F;
}

// Symmetric half-width: the larger of the two extremes so the display is balanced.
int32_t histogramGetHalfRange() {
  const int32_t lo = (histogramState.minScaledOffset < 0) ? -histogramState.minScaledOffset : histogramState.minScaledOffset;
  const int32_t hi = (histogramState.maxScaledOffset < 0) ? -histogramState.maxScaledOffset : histogramState.maxScaledOffset;
  const int32_t half = (lo > hi) ? lo : hi;
  return (half < 1) ? 1 : half;
}

int      histogramGetLatchedMaxFrequency() { return histogramState.latchedMaxFrequency; }
uint32_t histogramGetSampleCount()         { return histogramState.sampleCount; }

int histogramGetPeakBinIndex() {
  int peakIndex = HISTOGRAM_CENTER_INDEX;
  int peakCount = 0;
  for (int i = 0; i < HISTOGRAM_BIN_COUNT; ++i) {
    if (histogramState.bins[i] > peakCount) {
      peakCount = histogramState.bins[i];
      peakIndex = i;
    }
  }
  return peakIndex;
}
