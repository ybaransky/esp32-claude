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

class GraphModel {
public:
  void resetViewBounds() {
    Serial.println("[GRAPH] Resetting graph bounds");
    if (state.historyCount <= 0) {
      Serial.println("[GRAPH] No graph history; bounds reset skipped");
      return;
    }

    float min = state.history[0];
    float max = state.history[0];
    for (int i = 1; i < state.historyCount; i++) {
      min = fminf(min, state.history[i]);
      max = fmaxf(max, state.history[i]);
    }
    state.graphBounds = {min, max, true};
  }

  void updateBounds(float data) {
    ::updateBounds(state.totalBounds, data);
    ::updateBounds(state.graphBounds, data);
  }

  void appendSample(float data) {
    if (state.historyCount < GRAPH_WIDTH) {
      state.history[state.historyCount++] = data;
      return;
    }
    memmove(state.history, state.history + 1, sizeof(float) * (GRAPH_WIDTH - 1));
    state.history[GRAPH_WIDTH - 1] = data;
  }

  void reset() {
    state = {};
  }

  Bounds graphBounds() const { return state.graphBounds; }
  Bounds totalBounds() const { return state.totalBounds; }
  const float *history() const { return state.history; }
  int historyCount() const { return state.historyCount; }

private:
  GraphState state = {
    {},
    0,
    {0.0f, 0.0f, false},
    {0.0f, 0.0f, false},
  };
};

static GraphModel graphModel;

void graphResetBounds() {
  graphModel.resetViewBounds();
}

void graphUpdateBounds(float data) {
  graphModel.updateBounds(data);
}

void graphUpdateData(float data) {
  graphModel.appendSample(data);
}

void graphResetState() {
  Serial.println("[GRAPH] Resetting graph bounds/history");
  graphModel.reset();
}

Bounds graphGetGraphBounds() { return graphModel.graphBounds(); }
Bounds graphGetTotalBounds() { return graphModel.totalBounds(); }
const float *graphGetHistory() { return graphModel.history(); }
int graphGetHistoryCount()    { return graphModel.historyCount(); }
