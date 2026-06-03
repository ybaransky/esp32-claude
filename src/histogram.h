#pragma once

#include <stdint.h>

// Each bin represents exactly one scaled integer value (reading * 100).
// The first reading anchors the center bin; all others index relative to it.
// Supports ±HISTOGRAM_MAX_HALF_RANGE scaled units (±5.00°F) from center.
constexpr int32_t HISTOGRAM_MAX_HALF_RANGE = 500;
constexpr int     HISTOGRAM_BIN_COUNT      = 2 * HISTOGRAM_MAX_HALF_RANGE + 1;
constexpr int     HISTOGRAM_CENTER_INDEX   = HISTOGRAM_MAX_HALF_RANGE;
constexpr int     HISTOGRAM_VALUE_SCALE    = 100;
constexpr float   HISTOGRAM_BIN_SIZE_F     = 1.0f / HISTOGRAM_VALUE_SCALE;

void histogramUpdateData(float data);
void histogramReset();
void histogramRecenterOnPeak();

const int  *histogramGetBins();
int         histogramGetBinCount();
float       histogramGetCenterValueF();
int32_t     histogramGetHalfRange();
int         histogramGetLatchedMaxFrequency();
uint32_t    histogramGetSampleCount();
int         histogramGetPeakBinIndex();