#pragma once

#include <stdint.h>
#include "display_layout.h"

struct Bounds {
  float minimum;
  float maximum;
  bool  initialized;
};

void graphUpdateBounds(float data);
void graphResetBounds();
void graphResetState();
void graphUpdateData(float data);

Bounds          graphGetGraphBounds();
Bounds          graphGetTotalBounds();
const float    *graphGetHistory();
int             graphGetHistoryCount();
