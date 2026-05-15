#pragma once
#include <U8g2lib.h>

constexpr int DISPLAY_WIDTH  = 128;
constexpr int GRAPH_LEFT     = 24;
constexpr int GRAPH_TOP      = 2;
constexpr int GRAPH_BOTTOM   = 51;
constexpr int GRAPH_HEIGHT   = GRAPH_BOTTOM - GRAPH_TOP + 1;
constexpr int GRAPH_WIDTH    = DISPLAY_WIDTH - GRAPH_LEFT;
constexpr int STATUS_BASELINE_Y = 63;

void pushDiffHistory(float diffF);
void updateLifetimeDiff(float diffF);
void showGraph(U8G2 &u8g2, float bmpF, float shtF, float diffF, unsigned long elapsedSeconds);
