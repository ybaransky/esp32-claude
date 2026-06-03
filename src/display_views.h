#pragma once

#include <U8g2lib.h>
#include "graph.h"
#include "histogram.h"
#include "sensors.h"

struct GraphViewData {
  SensorReadings readings;
  Bounds graphBounds;
  Bounds totalBounds;
  const float *history;
  int historyCount;
};

GraphViewData graphBuildViewData(const SensorReadings &readings);
void showGraph(U8G2 &u8g2, const GraphViewData &view);

struct HistogramViewData {
  SensorReadings readings;
  const int *bins;
  int binCount;
  float centerValue;
  int32_t halfRange;
  int latchedMaxFrequency;
  uint32_t sampleCount;
  int peakBinIndex;
};

HistogramViewData histogramBuildViewData(const SensorReadings &readings);
void showHistogram(U8G2 &u8g2, const HistogramViewData &view);

void showSplash(U8G2 &u8g2, unsigned long remainingSecs);
void showMenu(U8G2 &u8g2, unsigned long remainingSecs);
void showNetworkInfo(U8G2 &u8g2, const String &ssid, const String &ip, unsigned long remainingSecs);
void showI2CScan(U8G2 &u8g2, unsigned long remainingSecs);
void showRtcStatus(U8G2 &u8g2, unsigned long remainingSecs);
void showErrorMessage(U8G2 &u8g2, const String &message, unsigned long remainingSecs);
