#pragma once
#include <U8g2lib.h>
#include "web.h"

struct Bounds {
  float minimum;
  float maximum;
  bool  initialized;
};

constexpr int DISPLAY_WIDTH  = 128;
constexpr int GRAPH_LEFT     = 24;
constexpr int GRAPH_TOP      = 2;
constexpr int GRAPH_BOTTOM      = 45;
constexpr int GRAPH_HEIGHT      = GRAPH_BOTTOM - GRAPH_TOP + 1;
constexpr int GRAPH_WIDTH       = DISPLAY_WIDTH - GRAPH_LEFT;
constexpr int STATUS_BASELINE_Y = 55;
constexpr int MINMAX_BASELINE_Y = 63;

void updateDataBounds(float data);
void resetGraphBounds(void);
void pushGraphHistory(float data);

void showGraph(U8G2 &u8g2, const SensorReadings &readings);
