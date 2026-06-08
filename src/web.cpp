#include "web.h"

#include "graph.h"
#include "histogram.h"
#include "html.h"
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

namespace Routes {
  constexpr const char *ROOT = "/";
  constexpr const char *API_SENSORS = "/api/sensors";
  constexpr const char *API_BMP = "/api/bmp";
  constexpr const char *API_SHT = "/api/sht";
  constexpr const char *API_DIFF = "/api/diff";
  constexpr const char *API_LIVE = "/api/live";
  constexpr const char *API_GRAPH = "/api/graph";
  constexpr const char *API_HISTOGRAM = "/api/histogram";
  constexpr const char *API_STATE = "/api/state";
}

static void handleRootRoute();
static void handleApiSensorsRoute();
static void handleApiBmpRoute();
static void handleApiShtRoute();
static void handleApiDiffRoute();
static void handleApiLiveRoute();
static void handleApiGraphRoute();
static void handleApiHistogramRoute();
static void handleApiStateRoute();
static void handleCaptiveRedirectRoute();

class WebPortal {
public:
  WebPortal() : server(80) {}

  void begin(const char *ssid, const char *password) {
    WiFi.softAP(ssid, password);
    waitForSoftApIp();

    Serial.printf("AP \"%s\" started  IP: %s\n", ssid, WiFi.softAPIP().toString().c_str());

    dnsRunning = dnsServer.start(53, "*", WiFi.softAPIP());
    if (!dnsRunning) {
      Serial.println("[DNS] Failed to start captive DNS server (no socket available)");
    }

    registerRoutes();
    server.begin();
    Serial.println("HTTP server started");
  }

  void update(const SensorReadings &readings) {
    lastSensorReading = readings;
  }

  void handleClients() {
    if (dnsRunning) {
      dnsServer.processNextRequest();
    }
    server.handleClient();
  }

  void getNetworkInfo(String &ssid, String &ip) {
    ssid = WiFi.softAPSSID();
    ip   = WiFi.softAPIP().toString();
  }

  void handleRoot() {
    logRequest(200);
    server.send(200, "text/html", INDEX_HTML);
  }

  void handleApiSensors() {
    sendJson(200, buildSensorReadingJson());
  }

  void handleApiBmp() {
    sendJson(200, buildSingleReadingJson("bmp", lastSensorReading.bmpF));
  }

  void handleApiSht() {
    sendJson(200, buildSingleReadingJson("sht", lastSensorReading.shtF));
  }

  void handleApiDiff() {
    sendJson(200, buildSingleReadingJson("diff", lastSensorReading.deltaF, true));
  }

  void handleApiLive() {
    char buf[112];
    snprintf(buf, sizeof(buf),
             "{\"bmp\":%.2f,\"sht\":%.2f,\"diff\":%.2f,\"graphCount\":%d,\"histSampleCount\":%lu}",
             lastSensorReading.bmpF,
             lastSensorReading.shtF,
             lastSensorReading.deltaF,
             graphGetHistoryCount(),
             static_cast<unsigned long>(histogramGetSampleCount()));
    sendJson(200, buf);
  }

  void handleApiGraph() {
    sendJson(200, buildGraphJson());
  }

  void handleApiHistogram() {
    sendJson(200, buildHistogramJson());
  }

  void handleApiState() {
    String json;
    json.reserve(15000);
    json += "{\"bmp\":";
    json += String(lastSensorReading.bmpF, 2);
    json += ",\"sht\":";
    json += String(lastSensorReading.shtF, 2);
    json += ",\"diff\":";
    json += String(lastSensorReading.deltaF, 2);
    json += ",\"graph\":";
    appendGraphJson(json);
    json += ",\"histogram\":";
    appendHistogramJson(json);
    json += '}';

    sendJson(200, json);
  }

  void handleCaptiveRedirect() {
    logRequest(302);
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  }

private:
  static const char *methodName(HTTPMethod method) {
    switch (method) {
      case HTTP_GET:    return "GET";
      case HTTP_POST:   return "POST";
      case HTTP_PUT:    return "PUT";
      case HTTP_DELETE: return "DELETE";
      case HTTP_PATCH:  return "PATCH";
      default:          return "OTHER";
    }
  }

  void waitForSoftApIp() {
    // softAP() returns before the interface is fully up; poll until we have a real IP.
    while (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
      delay(10);
    }
  }

  void registerRoutes() {
    server.on(Routes::ROOT,          handleRootRoute);
    server.on(Routes::API_SENSORS,   handleApiSensorsRoute);
    server.on(Routes::API_BMP,       handleApiBmpRoute);
    server.on(Routes::API_SHT,       handleApiShtRoute);
    server.on(Routes::API_DIFF,      handleApiDiffRoute);
    server.on(Routes::API_LIVE,      handleApiLiveRoute);
    server.on(Routes::API_GRAPH,     handleApiGraphRoute);
    server.on(Routes::API_HISTOGRAM, handleApiHistogramRoute);
    server.on(Routes::API_STATE,     handleApiStateRoute);
    server.onNotFound(handleCaptiveRedirectRoute);
  }

  void logRequest(int status) {
    Serial.printf("[HTTP] %s %s <- %s  => %d\n",
                  methodName(server.method()),
                  server.uri().c_str(),
                  server.client().remoteIP().toString().c_str(),
                  status);
  }

  void sendJson(int status, const char *json) {
    logRequest(status);
    server.send(status, "application/json", json);
  }

  void sendJson(int status, const String &json) {
    logRequest(status);
    server.send(status, "application/json", json);
  }

  String buildSensorReadingJson() const {
    char buf[72];
    snprintf(buf, sizeof(buf),
             "{\"bmp\":%.2f,\"sht\":%.2f,\"diff\":%.2f}",
             lastSensorReading.bmpF, lastSensorReading.shtF, lastSensorReading.deltaF);
    return String(buf);
  }

  static String buildSingleReadingJson(const char *name, float value, bool forceSign = false) {
    char buf[32];
    snprintf(buf,
             sizeof(buf),
             forceSign ? "{\"%s\":%+.2f}" : "{\"%s\":%.2f}",
             name,
             value);
    return String(buf);
  }

  static void appendGraphJson(String &json) {
    const float *history = graphGetHistory();
    const int count = graphGetHistoryCount();

    json += "{\"count\":";
    json += count;
    json += ",\"history\":[";
    for (int i = 0; i < count; ++i) {
      if (i > 0) json += ',';
      json += String(history[i], 2);
    }
    json += "]}";
  }

  static String buildGraphJson() {
    String json;
    json.reserve(1800);
    appendGraphJson(json);
    return json;
  }

  static void appendHistogramJson(String &json) {
    const int *bins = histogramGetBins();
    const int binCount = histogramGetBinCount();

    json += "{\"center\":";
    json += String(histogramGetCenterValueF(), 2);
    json += ",\"halfRange\":";
    json += histogramGetHalfRange();
    json += ",\"scale\":";
    json += HISTOGRAM_VALUE_SCALE;
    json += ",\"sampleCount\":";
    json += static_cast<unsigned long>(histogramGetSampleCount());
    json += ",\"latchedMax\":";
    json += histogramGetLatchedMaxFrequency();
    json += ",\"bins\":[";

    for (int i = 0; i < binCount; ++i) {
      if (i > 0) json += ',';
      json += bins[i];
    }
    json += "]}";
  }

  static String buildHistogramJson() {
    String json;
    json.reserve(12000);
    appendHistogramJson(json);
    return json;
  }

  WebServer      server;
  DNSServer      dnsServer;
  SensorReadings lastSensorReading = {};
  bool           dnsRunning = false;
};

static WebPortal webPortal;

static void handleRootRoute() { webPortal.handleRoot(); }
static void handleApiSensorsRoute() { webPortal.handleApiSensors(); }
static void handleApiBmpRoute() { webPortal.handleApiBmp(); }
static void handleApiShtRoute() { webPortal.handleApiSht(); }
static void handleApiDiffRoute() { webPortal.handleApiDiff(); }
static void handleApiLiveRoute() { webPortal.handleApiLive(); }
static void handleApiGraphRoute() { webPortal.handleApiGraph(); }
static void handleApiHistogramRoute() { webPortal.handleApiHistogram(); }
static void handleApiStateRoute() { webPortal.handleApiState(); }
static void handleCaptiveRedirectRoute() { webPortal.handleCaptiveRedirect(); }

void webBegin(const char *ssid, const char *password) {
  webPortal.begin(ssid, password);
}

void webUpdate(const SensorReadings &readings) {
  webPortal.update(readings);
}

void webHandleClients() {
  webPortal.handleClients();
}

void networkGetInfo(String &ssid, String &ip) {
  webPortal.getNetworkInfo(ssid, ip);
}
