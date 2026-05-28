#include "histogram.h"

#include <math.h>
#include <string.h>
#include <Arduino.h>

static int      histogramBins[HISTOGRAM_BIN_COUNT] = {0};
static int32_t  centerScaledValue   = 0;
static bool     initialized         = false;
static int32_t  minScaledOffset     = 0;  // most negative offset seen
static int32_t  maxScaledOffset     = 0;  // most positive offset seen
static int      latchedMaxFrequency = 0;
static uint32_t sampleCount         = 0;

void histogramUpdateData(float data) {
  const int32_t scaledValue = static_cast<int32_t>(lround(static_cast<double>(data) * HISTOGRAM_VALUE_SCALE));

  if (!initialized) {
    centerScaledValue = scaledValue;
    initialized       = true;
  }

  const int32_t offset = scaledValue - centerScaledValue;
  const int index = HISTOGRAM_CENTER_INDEX + static_cast<int>(offset);

  if (index < 0 || index >= HISTOGRAM_BIN_COUNT) {
    Serial.printf("[HIST] Value out of range: %.2f (offset %ld)\n",
                  data, static_cast<long>(offset));
    return;
  }

  histogramBins[index]++;
  sampleCount++;

  if (sampleCount == 1) {
    minScaledOffset = offset;
    maxScaledOffset = offset;
  } else {
    if (offset < minScaledOffset) minScaledOffset = offset;
    if (offset > maxScaledOffset) maxScaledOffset = offset;
  }

  if (histogramBins[index] > latchedMaxFrequency) {
    latchedMaxFrequency = histogramBins[index];
  }
}

void histogramRecenterOnPeak() {
  if (!initialized || sampleCount == 0) return;

  int peakIndex = HISTOGRAM_CENTER_INDEX;
  int peakCount = 0;
  for (int i = 0; i < HISTOGRAM_BIN_COUNT; ++i) {
    if (histogramBins[i] > peakCount) {
      peakCount = histogramBins[i];
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
      newBins[dest] = histogramBins[i];
    }
  }
  memcpy(histogramBins, newBins, sizeof(histogramBins));

  // The new center is the peak's former scaled value.
  centerScaledValue += shift;
  // Offsets are relative to center, so they shrink by the same shift.
  minScaledOffset -= shift;
  maxScaledOffset -= shift;

  Serial.printf("[HIST] Recentered on peak at %.2f\n",
                static_cast<float>(centerScaledValue) * HISTOGRAM_BIN_SIZE_F);
}

void histogramReset() {
  memset(histogramBins, 0, sizeof(histogramBins));
  centerScaledValue   = 0;
  initialized         = false;
  minScaledOffset     = 0;
  maxScaledOffset     = 0;
  latchedMaxFrequency = 0;
  sampleCount         = 0;
}

const int *histogramGetBins()  { return histogramBins; }
int  histogramGetBinCount()    { return HISTOGRAM_BIN_COUNT; }

float histogramGetCenterValueF() {
  return static_cast<float>(centerScaledValue) * HISTOGRAM_BIN_SIZE_F;
}

// Symmetric half-width: the larger of the two extremes so the display is balanced.
int32_t histogramGetHalfRange() {
  const int32_t lo = (minScaledOffset < 0) ? -minScaledOffset : minScaledOffset;
  const int32_t hi = (maxScaledOffset < 0) ? -maxScaledOffset : maxScaledOffset;
  const int32_t half = (lo > hi) ? lo : hi;
  return (half < 1) ? 1 : half;
}

int      histogramGetLatchedMaxFrequency() { return latchedMaxFrequency; }
uint32_t histogramGetSampleCount()         { return sampleCount; }