#include <Arduino.h>
#include <string.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_SHT31.h>

// Shared I2C pins for display and sensors.
constexpr uint8_t I2C_SDA_PIN = 22;
constexpr uint8_t I2C_SCL_PIN = 21;

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, I2C_SCL_PIN, I2C_SDA_PIN);
Adafruit_BMP280 bme;
Adafruit_SHT31 sht31;

unsigned long lastRead = 0;
unsigned long startupTime = 0;
int readCount = 0;
constexpr unsigned long READ_INTERVAL_SECONDS = 2;

constexpr int DISPLAY_WIDTH = 128;
constexpr int GRAPH_LEFT = 24;
constexpr int GRAPH_TOP = 2;
constexpr int STATUS_BASELINE_Y = 63;
constexpr int GRAPH_BOTTOM = 51;
constexpr int GRAPH_HEIGHT = GRAPH_BOTTOM - GRAPH_TOP + 1;
constexpr int GRAPH_WIDTH = DISPLAY_WIDTH - GRAPH_LEFT;

float diffHistory[GRAPH_WIDTH];
int diffHistoryCount = 0;
float lifetimeMinDiff = 0.0f;
float lifetimeMaxDiff = 0.0f;
bool hasLifetimeDiff = false;

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

  if (diffF < lifetimeMinDiff) {
    lifetimeMinDiff = diffF;
  }
  if (diffF > lifetimeMaxDiff) {
    lifetimeMaxDiff = diffF;
  }
}

void getDiffMinMax(float &minDiff, float &maxDiff) {
  if (!hasLifetimeDiff) {
    minDiff = -1.0f;
    maxDiff = 1.0f;
    return;
  }

  minDiff = lifetimeMinDiff;
  maxDiff = lifetimeMaxDiff;

  float span = maxDiff - minDiff;
  float pad = 0.0f;
  if (span > 0.0f) {
    pad = span * 0.05f;
  } else {
    float base = fabsf(maxDiff);
    pad = (base > 0.0f) ? (base * 0.05f) : 0.05f;
  }
  maxDiff += pad;
  minDiff -= pad;
}

int diffToGraphY(float value, float minDiff, float maxDiff) {
  float normalized = (value - minDiff) / (maxDiff - minDiff);
  if (normalized < 0.0f) {
    normalized = 0.0f;
  }
  if (normalized > 1.0f) {
    normalized = 1.0f;
  }
  return GRAPH_BOTTOM - static_cast<int>(normalized * (GRAPH_HEIGHT - 1) + 0.5f);
}

void scanI2C() {
  Serial.println("Scanning I2C bus...");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Found device at 0x%02X\n", addr);
      found++;
    }
  }
  Serial.printf("Scan complete. %d device(s) found.\n\n", found);
}

void showGraph(float bmpF, float shtF, float diffF, unsigned long elapsedSeconds) {
  float minDiff = 0.0f;
  float maxDiff = 0.0f;
  getDiffMinMax(minDiff, maxDiff);

  char maxBuf[12];
  char midBuf[12];
  char minBuf[12];
  char statusBuf[24];
  char countBuf[12];
  // Use 6.2f if any value is negative, else 5.2f
  bool hasNegative = (diffF < 0.0f) || (minDiff < 0.0f) || (maxDiff < 0.0f);
  const char *yFmt = hasNegative ? "%5.2f" : "%4.2f";
  snprintf(maxBuf, sizeof(maxBuf), yFmt, maxDiff);
  snprintf(midBuf, sizeof(midBuf), yFmt, diffF);
  snprintf(minBuf, sizeof(minBuf), yFmt, minDiff);
  snprintf(statusBuf, sizeof(statusBuf), "BMP=%.2f SHT=%.2f", bmpF, shtF);
  snprintf(countBuf, sizeof(countBuf), "%lu", elapsedSeconds);

  // If any Y-axis value is negative, shift graph by 1 character (6 pixels)
  int graphLeftOffset = hasNegative ? 6 : 0; // 6 pixels = 1 character in 5x7 font + 1px space
  int effectiveGraphLeft = GRAPH_LEFT + graphLeftOffset;

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 7, maxBuf);
  u8g2.drawStr(0, (GRAPH_TOP + GRAPH_BOTTOM) / 2 + 3, midBuf);
  u8g2.drawStr(0, GRAPH_BOTTOM, minBuf);

  // Draw Y axis and graph boundary lines.
  u8g2.drawVLine(effectiveGraphLeft - 1, GRAPH_TOP, GRAPH_HEIGHT);
  u8g2.drawHLine(effectiveGraphLeft - 1, GRAPH_BOTTOM, GRAPH_WIDTH);

  int yMaxTick = diffToGraphY(maxDiff, minDiff, maxDiff);
  int yMinTick = diffToGraphY(minDiff, minDiff, maxDiff);
  int yDiffTick = diffToGraphY(diffF, minDiff, maxDiff);
  u8g2.drawHLine(effectiveGraphLeft - 4, yMaxTick, 4);
  u8g2.drawHLine(effectiveGraphLeft - 4, yMinTick, 4);
  u8g2.drawHLine(effectiveGraphLeft - 4, yDiffTick, 4);

  if (diffHistoryCount == 1) {
    int y = diffToGraphY(diffHistory[0], minDiff, maxDiff);
    u8g2.drawPixel(effectiveGraphLeft, y);
  } else if (diffHistoryCount > 1) {
    for (int i = 1; i < diffHistoryCount; i++) {
      int x0 = effectiveGraphLeft + i - 1;
      int x1 = effectiveGraphLeft + i;
      int y0 = diffToGraphY(diffHistory[i - 1], minDiff, maxDiff);
      int y1 = diffToGraphY(diffHistory[i], minDiff, maxDiff);
      u8g2.drawLine(x0, y0, x1, y1);
    }
  }

  u8g2.drawStr(0, STATUS_BASELINE_Y, statusBuf);
  int countX = DISPLAY_WIDTH - u8g2.getStrWidth(countBuf);
  u8g2.drawStr(countX, STATUS_BASELINE_Y, countBuf);
  u8g2.sendBuffer();
}

void readAndDisplay() {
  readCount++;
  if (readCount == 1 || (readCount % 10) == 0) {
    scanI2C();
  }
  float bmpF = bme.readTemperature()   * 9.0f / 5.0f + 32.0f;
  float shtF = sht31.readTemperature() * 9.0f / 5.0f + 32.0f;
  float diffF = bmpF - shtF;
  updateLifetimeDiff(diffF);
  pushDiffHistory(diffF);
  unsigned long elapsedSeconds = (millis() - startupTime) / 1000UL;
  Serial.printf("Read #%d - BMP: %.2f F  SHT: %.2f F  Diff: %+.2f F\n", readCount, bmpF, shtF, diffF);
  showGraph(bmpF, shtF, diffF, elapsedSeconds);
}

void setup() {
  Serial.begin(115200);

  // u8g2.begin() initializes Wire using the pins passed in the constructor.
  u8g2.begin();

  // BMP280 default address is 0x76; use 0x77 if sensor not found
  bme.begin(0x76);
  sht31.begin(0x44);

  startupTime = millis();
  readAndDisplay();
  lastRead = millis();
}

void loop() {
  if (millis() - lastRead >= (READ_INTERVAL_SECONDS * 1000UL)) {
    lastRead = millis();
    readAndDisplay();
  }
}
