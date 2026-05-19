#include <Arduino.h>
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

void resetGraphBoundsType1(void) {
  Serial.println("[GRAPH] Resetting graph bounds Type 1");
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

void resetGraphBoundsType2(void) {
  Serial.println("[GRAPH] Resetting graph bounds Type 2");
  float min = graphHistory[0];
  float max = graphHistory[0];
  for (int i=1; i < graphHistoryCount; i++) {
    min = fminf(min, graphHistory[i]);
    max = fmaxf(max, graphHistory[i]);
  }
  graphBounds = {min, max, true};
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

Bounds graphGetGraphBounds() { return graphBounds; }
Bounds graphGetTotalBounds() { return totalBounds; }
const float *graphGetHistory() { return graphHistory; }
int graphGetHistoryCount()    { return graphHistoryCount; }
