#include "web.h"
#include "html.h"
#include "graph.h"
#include "histogram.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

static WebServer      server(80);
static DNSServer      dnsServer;
static SensorReadings lastSensorReading;
static bool           dnsRunning = false;

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

void networkGetInfo(String &ssid, String &ip) {
    ssid = WiFi.softAPSSID();
    ip   = WiFi.softAPIP().toString();
}

static const char *methodName(HTTPMethod m) {
    switch (m) {
        case HTTP_GET:    return "GET";
        case HTTP_POST:   return "POST";
        case HTTP_PUT:    return "PUT";
        case HTTP_DELETE: return "DELETE";
        case HTTP_PATCH:  return "PATCH";
        default:          return "OTHER";
    }
}

static void logRequest(int status) {
    Serial.printf("[HTTP] %s %s <- %s  => %d\n",
        methodName(server.method()),
        server.uri().c_str(),
        server.client().remoteIP().toString().c_str(),
        status);
}

static void sendJson(int status, const char *json) {
    logRequest(status);
    server.send(status, "application/json", json);
}

static void sendJson(int status, const String &json) {
    logRequest(status);
    server.send(status, "application/json", json);
}

static String buildSensorReadingJson() {
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

static void handleRoot() {
    logRequest(200);
    server.send(200, "text/html", INDEX_HTML);
}

static void handleApiSensors() {
    sendJson(200, buildSensorReadingJson());
}

static void handleApiBmp() {
    sendJson(200, buildSingleReadingJson("bmp", lastSensorReading.bmpF));
}

static void handleApiSht() {
    sendJson(200, buildSingleReadingJson("sht", lastSensorReading.shtF));
}

static void handleApiDiff() {
    sendJson(200, buildSingleReadingJson("diff", lastSensorReading.deltaF, true));
}

static void handleApiLive() {
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

static void handleApiGraph() {
    sendJson(200, buildGraphJson());
}

static void handleApiHistogram() {
    sendJson(200, buildHistogramJson());
}

static void handleApiState() {
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

static void handleCaptiveRedirect() {
    logRequest(302);
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
}

void webBegin(const char *ssid, const char *password) {
    WiFi.softAP(ssid, password);

    // softAP() returns before the interface is fully up; poll until we have a real IP.
    while (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
        delay(10);
    }
    Serial.printf("AP \"%s\" started  IP: %s\n", ssid, WiFi.softAPIP().toString().c_str());

    // Answer every DNS query with our IP so browsers trigger the captive portal flow.
    dnsRunning = dnsServer.start(53, "*", WiFi.softAPIP());
    if (!dnsRunning) {
        Serial.println("[DNS] Failed to start captive DNS server (no socket available)");
    }

    server.on(Routes::ROOT,          handleRoot);
    server.on(Routes::API_SENSORS,   handleApiSensors);
    server.on(Routes::API_BMP,       handleApiBmp);
    server.on(Routes::API_SHT,       handleApiSht);
    server.on(Routes::API_DIFF,      handleApiDiff);
    server.on(Routes::API_LIVE,      handleApiLive);
    server.on(Routes::API_GRAPH,     handleApiGraph);
    server.on(Routes::API_HISTOGRAM, handleApiHistogram);
    server.on(Routes::API_STATE,     handleApiState);
    server.onNotFound(handleCaptiveRedirect);
    server.begin();
    Serial.println("HTTP server started");
}

void webUpdate(const SensorReadings &readings) {
    lastSensorReading = readings;
}

void webHandleClients() {
    if (dnsRunning) {
        dnsServer.processNextRequest();
    }
    server.handleClient();
}
