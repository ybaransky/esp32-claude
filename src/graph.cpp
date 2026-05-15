#include "graph.h"
#include <string.h>

static float diffHistory[GRAPH_WIDTH];
static int   diffHistoryCount = 0;
static float lifetimeMinDiff  = 0.0f;
static float lifetimeMaxDiff  = 0.0f;
static bool  hasLifetimeDiff  = false;

void pushDiffHistory(float diffF) {
  if (diffHistoryCount < GRAPH_WIDTH) {
    diffHistory[diffHistoryCount++] = diffF;
    return;
  }

  memmove(diffHistory, diffHistory + 1, sizeof(float) * (GRAPH_WIDTH - 1));
  diffHistory[GRAPH_WIDTH - 1] = diffF;
}

void updateLifetimeDiff(float diffF) {
  if (!hasLifetimeDiff) {
    lifetimeMinDiff = diffF;
    lifetimeMaxDiff = diffF;
    hasLifetimeDiff = true;
    return;
  }

  if (diffF < lifetimeMinDiff) lifetimeMinDiff = diffF;
  if (diffF > lifetimeMaxDiff) lifetimeMaxDiff = diffF;
}

static void getDiffMinMax(float &minDiff, float &maxDiff) {
  if (!hasLifetimeDiff) {
    minDiff = -0.1f;
    maxDiff =  0.1f;
    return;
  }

  minDiff = lifetimeMinDiff;
  maxDiff = lifetimeMaxDiff;

  float span = maxDiff - minDiff;
  float pad  = (span > 0.0f) ? span * 0.05f : (fabsf(maxDiff) > 0.0f ? fabsf(maxDiff) * 0.05f : 0.05f);
  maxDiff += pad;
  minDiff -= pad;
}

static int diffToGraphY(float value, float minDiff, float maxDiff) {
  float normalized = (value - minDiff) / (maxDiff - minDiff);
  if (normalized < 0.0f) normalized = 0.0f;
  if (normalized > 1.0f) normalized = 1.0f;
  return GRAPH_BOTTOM - static_cast<int>(normalized * (GRAPH_HEIGHT - 1) + 0.5f);
}

void showGraph(U8G2 &u8g2, float bmpF, float shtF, float diffF, unsigned long elapsedSeconds) {
  float minDiff = 0.0f;
  float maxDiff = 0.0f;
  getDiffMinMax(minDiff, maxDiff);

  bool hasNegative = (diffF < 0.0f) || (minDiff < 0.0f) || (maxDiff < 0.0f);
  const char *yFmt = hasNegative ? "%5.2f" : "%4.2f";
  int graphLeftOffset  = hasNegative ? 6 : 0;
  int effectiveGraphLeft = GRAPH_LEFT + graphLeftOffset;

  char maxBuf[12], midBuf[12], minBuf[12], statusBuf[24], countBuf[12];
  snprintf(maxBuf,    sizeof(maxBuf),    yFmt,           maxDiff);
  snprintf(midBuf,    sizeof(midBuf),    yFmt,           diffF);
  snprintf(minBuf,    sizeof(minBuf),    yFmt,           minDiff);
  snprintf(statusBuf, sizeof(statusBuf), "BMP=%.2f SHT=%.2f", bmpF, shtF);
  snprintf(countBuf,  sizeof(countBuf),  "%lu",          elapsedSeconds);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 7,                            maxBuf);
  u8g2.drawStr(0, (GRAPH_TOP + GRAPH_BOTTOM) / 2 + 3, midBuf);
  u8g2.drawStr(0, GRAPH_BOTTOM,                 minBuf);

  u8g2.drawVLine(effectiveGraphLeft - 1, GRAPH_TOP, GRAPH_HEIGHT);
  u8g2.drawHLine(effectiveGraphLeft - 1, GRAPH_BOTTOM, GRAPH_WIDTH);

  u8g2.drawHLine(effectiveGraphLeft - 4, diffToGraphY(maxDiff, minDiff, maxDiff), 4);
  u8g2.drawHLine(effectiveGraphLeft - 4, diffToGraphY(minDiff, minDiff, maxDiff), 4);
  u8g2.drawHLine(effectiveGraphLeft - 4, diffToGraphY(diffF,   minDiff, maxDiff), 4);

  if (diffHistoryCount == 1) {
    u8g2.drawPixel(effectiveGraphLeft, diffToGraphY(diffHistory[0], minDiff, maxDiff));
  } else {
    for (int i = 1; i < diffHistoryCount; i++) {
      u8g2.drawLine(
        effectiveGraphLeft + i - 1, diffToGraphY(diffHistory[i - 1], minDiff, maxDiff),
        effectiveGraphLeft + i,     diffToGraphY(diffHistory[i],     minDiff, maxDiff)
      );
    }
  }

  int countX = DISPLAY_WIDTH - u8g2.getStrWidth(countBuf);
  u8g2.drawStr(0,      STATUS_BASELINE_Y, statusBuf);
  u8g2.drawStr(countX, STATUS_BASELINE_Y, countBuf);
  u8g2.sendBuffer();
}
