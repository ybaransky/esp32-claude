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

void graphResetBounds() {
  Serial.println("[GRAPH] Resetting graph bounds");
  if (graphHistoryCount <= 0) {
    Serial.println("[GRAPH] No graph history; bounds reset skipped");
    return;
  }

  float min = graphHistory[0];
  float max = graphHistory[0];
  for (int i=1; i < graphHistoryCount; i++) {
    min = fminf(min, graphHistory[i]);
    max = fmaxf(max, graphHistory[i]);
  }
  graphBounds = {min, max, true};
}
void graphUpdateBounds(float data) {
  updateBounds(totalBounds, data);
  updateBounds(graphBounds, data);
}

void graphUpdateData(float data) {
  if (graphHistoryCount < GRAPH_WIDTH) {
    graphHistory[graphHistoryCount++] = data;
    return;
  }
  memmove(graphHistory, graphHistory + 1, sizeof(float) * (GRAPH_WIDTH - 1));
  graphHistory[GRAPH_WIDTH - 1] = data;
}

void graphResetState() {
  Serial.println("[GRAPH] Resetting graph bounds/history");
  memset(graphHistory, 0, sizeof(graphHistory));
  graphHistoryCount = 0;

  totalBounds = {0.0f, 0.0f, false};
  graphBounds = {0.0f, 0.0f, false};
}

Bounds graphGetGraphBounds() { return graphBounds; }
Bounds graphGetTotalBounds() { return totalBounds; }
const float *graphGetHistory() { return graphHistory; }
int graphGetHistoryCount()    { return graphHistoryCount; }
