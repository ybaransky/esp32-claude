#include <Arduino.h>
#include "graph.h"
#include <string.h>
#include <math.h>

struct GraphState {
  float  history[GRAPH_WIDTH];
  int    historyCount;
  Bounds totalBounds;
  Bounds graphBounds;
};

static GraphState graphState = {
  {},
  0,
  {0.0f, 0.0f, false},
  {0.0f, 0.0f, false},
};

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
  if (graphState.historyCount <= 0) {
    Serial.println("[GRAPH] No graph history; bounds reset skipped");
    return;
  }

  float min = graphState.history[0];
  float max = graphState.history[0];
  for (int i=1; i < graphState.historyCount; i++) {
    min = fminf(min, graphState.history[i]);
    max = fmaxf(max, graphState.history[i]);
  }
  graphState.graphBounds = {min, max, true};
}
void graphUpdateBounds(float data) {
  updateBounds(graphState.totalBounds, data);
  updateBounds(graphState.graphBounds, data);
}

void graphUpdateData(float data) {
  if (graphState.historyCount < GRAPH_WIDTH) {
    graphState.history[graphState.historyCount++] = data;
    return;
  }
  memmove(graphState.history, graphState.history + 1, sizeof(float) * (GRAPH_WIDTH - 1));
  graphState.history[GRAPH_WIDTH - 1] = data;
}

void graphResetState() {
  Serial.println("[GRAPH] Resetting graph bounds/history");
  graphState = {};
}

Bounds graphGetGraphBounds() { return graphState.graphBounds; }
Bounds graphGetTotalBounds() { return graphState.totalBounds; }
const float *graphGetHistory() { return graphState.history; }
int graphGetHistoryCount()    { return graphState.historyCount; }
