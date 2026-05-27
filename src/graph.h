#pragma once

#include <stdint.h>

struct Bounds {
  float minimum;
  float maximum;
  bool  initialized;
};

constexpr int DISPLAY_WIDTH     = 128;
constexpr int GRAPH_LEFT        = 24;
constexpr int GRAPH_TOP         = 2;
constexpr int GRAPH_BOTTOM      = 45;
constexpr int GRAPH_HEIGHT      = GRAPH_BOTTOM - GRAPH_TOP + 1;
constexpr int GRAPH_WIDTH       = DISPLAY_WIDTH - GRAPH_LEFT;
constexpr int STATUS_BASELINE_Y = 55;
constexpr int MINMAX_BASELINE_Y = 63;

constexpr int   HISTOGRAM_BIN_COUNT    = 451;
constexpr int   HISTOGRAM_CENTER_INDEX = HISTOGRAM_BIN_COUNT / 2;
constexpr int   HISTOGRAM_VALUE_SCALE  = 100;
constexpr float HISTOGRAM_BIN_SIZE_F   = 0.01f;

void updateDataBounds(float data);
void resetGraphBounds(int type);
void resetGraphAndHistogram();
void pushGraphHistory(float data);

Bounds          graphGetGraphBounds();
Bounds          graphGetTotalBounds();
const float    *graphGetHistory();
int             graphGetHistoryCount();

const int      *graphGetHistogram();
int             graphGetHistogramBinCount();
double          graphGetHistogramMean();
double          graphGetHistogramMeanScaled();
double          graphGetHistogramHalfRangeScaled();
int             graphGetHistogramLatchedMaxFrequency();
uint32_t        graphGetHistogramSampleCount();
