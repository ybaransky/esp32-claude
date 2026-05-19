#include "web.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

static WebServer      server(80);
static DNSServer      dnsServer;
static SensorReadings latest;

static const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 Sensors</title>
  <style>
    body { font-family: sans-serif; max-width: 420px; margin: 40px auto; padding: 0 16px; background: #fff; }
    h1   { font-size: 1.3rem; color: #333; }
    .card { background: #f4f4f4; border-radius: 8px; padding: 16px 20px; margin: 12px 0; }
    .label { color: #777; font-size: 0.8rem; text-transform: uppercase; letter-spacing: .05em; margin-bottom: 4px; }
    .value { font-size: 2.2rem; font-weight: 700; color: #111; }
    .unit  { font-size: 1rem; color: #999; margin-left: 2px; }
  </style>
</head>
<body>
  <h1>ESP32 Sensor Readings</h1>
  <div class="card">
    <div class="label">BMP280 Temperature</div>
    <span class="value" id="bmp">--</span><span class="unit">&deg;F</span>
  </div>
  <div class="card">
    <div class="label">SHT31 Temperature</div>
    <span class="value" id="sht">--</span><span class="unit">&deg;F</span>
  </div>
  <div class="card">
    <div class="label">Difference (BMP &minus; SHT)</div>
    <span class="value" id="diff">--</span><span class="unit">&deg;F</span>
  </div>
  <script>
    function fmt(v, sign) {
      return (sign && v >= 0 ? '+' : '') + v.toFixed(2);
    }
    async function refresh() {
      try {
        const d = await fetch('/api/sensors').then(r => r.json());
        document.getElementById('bmp').textContent  = fmt(d.bmp);
        document.getElementById('sht').textContent  = fmt(d.sht);
        document.getElementById('diff').textContent = fmt(d.diff, true);
      } catch(e) {}
    }
    refresh();
    setInterval(refresh, 2000);
  </script>
</body>
</html>
)rawliteral";

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

static void handleRoot() {
    logRequest(200);
    server.send(200, "text/html", INDEX_HTML);
}

static void handleApiSensors() {
    logRequest(200);
    char buf[72];
    snprintf(buf, sizeof(buf),
             "{\"bmp\":%.2f,\"sht\":%.2f,\"diff\":%.2f}",
             latest.bmpF, latest.shtF, latest.deltaF);
    server.send(200, "application/json", buf);
}

static void handleApiBmp() {
    logRequest(200);
    char buf[24];
    snprintf(buf, sizeof(buf), "{\"bmp\":%.2f}", latest.bmpF);
    server.send(200, "application/json", buf);
}

static void handleApiSht() {
    logRequest(200);
    char buf[24];
    snprintf(buf, sizeof(buf), "{\"sht\":%.2f}", latest.shtF);
    server.send(200, "application/json", buf);
}

static void handleApiDiff() {
    logRequest(200);
    char buf[24];
    snprintf(buf, sizeof(buf), "{\"diff\":%.2f}", latest.deltaF);
    server.send(200, "application/json", buf);
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
    dnsServer.start(53, "*", WiFi.softAPIP());

    server.on("/",            handleRoot);
    server.on("/api/sensors", handleApiSensors);
    server.on("/api/bmp",     handleApiBmp);
    server.on("/api/sht",     handleApiSht);
    server.on("/api/diff",    handleApiDiff);
    server.onNotFound(handleCaptiveRedirect);
    server.begin();
    Serial.println("HTTP server started");
}

void webUpdate(const SensorReadings &readings) {
    latest = readings;
}

void webHandleClients() {
    dnsServer.processNextRequest();
    server.handleClient();
}
