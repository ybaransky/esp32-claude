#include "histogram.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

struct HistogramState {
  int      bins[HISTOGRAM_BIN_COUNT];
  int32_t  centerScaledValue;
  bool     initialized;
  int32_t  minScaledOffset;
  int32_t  maxScaledOffset;
  int      latchedMaxFrequency;
  uint32_t sampleCount;
};

class HistogramModel {
public:
  void update(float data) {
    const int32_t scaledValue = scaleReading(data);

    if (!state.initialized) {
      state.centerScaledValue = scaledValue;
      state.initialized       = true;
    }

    const int32_t offset = scaledValue - state.centerScaledValue;
    const int index = HISTOGRAM_CENTER_INDEX + static_cast<int>(offset);

    if (index < 0 || index >= HISTOGRAM_BIN_COUNT) {
      Serial.printf("[HIST] Value out of range: %.2f (offset %ld)\n",
                    data, static_cast<long>(offset));
      return;
    }

    state.bins[index]++;
    state.sampleCount++;
    updateObservedRange(offset);
    latchMaxFrequency(index);
  }

  void recenterOnPeak() {
    if (!state.initialized || state.sampleCount == 0) return;

    const int peakIndex = peakBinIndex();
    if (peakIndex == HISTOGRAM_CENTER_INDEX) return;

    shiftBinsToCenter(peakIndex);
    Serial.printf("[HIST] Recentered on peak at %.2f\n", centerValueF());
  }

  void reset() {
    state = {};
  }

  const int *bins() const { return state.bins; }
  int binCount() const { return HISTOGRAM_BIN_COUNT; }

  float centerValueF() const {
    return static_cast<float>(state.centerScaledValue) * HISTOGRAM_BIN_SIZE_F;
  }

  // Symmetric half-width keeps the display balanced around the current center.
  int32_t halfRange() const {
    const int32_t lo = absoluteOffset(state.minScaledOffset);
    const int32_t hi = absoluteOffset(state.maxScaledOffset);
    const int32_t half = (lo > hi) ? lo : hi;
    return (half < 1) ? 1 : half;
  }

  int latchedMaxFrequency() const { return state.latchedMaxFrequency; }
  uint32_t sampleCount() const { return state.sampleCount; }

  int peakBinIndex() const {
    int peakIndex = HISTOGRAM_CENTER_INDEX;
    int peakCount = 0;
    for (int i = 0; i < HISTOGRAM_BIN_COUNT; ++i) {
      if (state.bins[i] > peakCount) {
        peakCount = state.bins[i];
        peakIndex = i;
      }
    }
    return peakIndex;
  }

private:
  static int32_t scaleReading(float data) {
    return static_cast<int32_t>(lround(static_cast<double>(data) * HISTOGRAM_VALUE_SCALE));
  }

  static int32_t absoluteOffset(int32_t offset) {
    return (offset < 0) ? -offset : offset;
  }

  void updateObservedRange(int32_t offset) {
    if (state.sampleCount == 1) {
      state.minScaledOffset = offset;
      state.maxScaledOffset = offset;
      return;
    }

    if (offset < state.minScaledOffset) state.minScaledOffset = offset;
    if (offset > state.maxScaledOffset) state.maxScaledOffset = offset;
  }

  void latchMaxFrequency(int index) {
    if (state.bins[index] > state.latchedMaxFrequency) {
      state.latchedMaxFrequency = state.bins[index];
    }
  }

  void shiftBinsToCenter(int peakIndex) {
    const int shift = peakIndex - HISTOGRAM_CENTER_INDEX;
    int newBins[HISTOGRAM_BIN_COUNT] = {};
    for (int i = 0; i < HISTOGRAM_BIN_COUNT; ++i) {
      const int dest = i - shift;
      if (dest >= 0 && dest < HISTOGRAM_BIN_COUNT) {
        newBins[dest] = state.bins[i];
      }
    }
    memcpy(state.bins, newBins, sizeof(state.bins));

    state.centerScaledValue += shift;
    state.minScaledOffset -= shift;
    state.maxScaledOffset -= shift;
  }

  HistogramState state = {};
};

static HistogramModel histogramModel;

void histogramUpdateData(float data) {
  histogramModel.update(data);
}

void histogramRecenterOnPeak() {
  histogramModel.recenterOnPeak();
}

void histogramReset() {
  histogramModel.reset();
}

const int *histogramGetBins() { return histogramModel.bins(); }
int histogramGetBinCount() { return histogramModel.binCount(); }
float histogramGetCenterValueF() { return histogramModel.centerValueF(); }
int32_t histogramGetHalfRange() { return histogramModel.halfRange(); }
int histogramGetLatchedMaxFrequency() { return histogramModel.latchedMaxFrequency(); }
uint32_t histogramGetSampleCount() { return histogramModel.sampleCount(); }
int histogramGetPeakBinIndex() { return histogramModel.peakBinIndex(); }
