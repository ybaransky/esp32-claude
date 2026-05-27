#include "web.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

static WebServer      server(80);
static DNSServer      dnsServer;
static SensorReadings lastSensorReading;

static const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Typcial Temperature Sensor Variability</title>
  <style>
    body { font-family: sans-serif; max-width: 420px; margin: 40px auto; padding: 0 16px; background: #fff; }
    h1   { font-size: 1.3rem; color: #333; }
    .card { background: #f4f4f4; border-radius: 8px; padding: 16px 20px; margin: 12px 0; }
    .label { color: #777; font-size: 0.8rem; text-transform: uppercase; letter-spacing: .05em; margin-bottom: 4px; }
    .value { font-size: 2.2rem; font-weight: 700; color: #111; }
    .unit  { font-size: 1rem; color: #999; margin-left: 2px; }
    #diffChart { width: 100%; height: 140px; display: block; background: #fff; border-radius: 6px; }
    #histChart { width: 100%; height: 140px; display: block; background: #fff; border-radius: 6px; }
  </style>
</head>
<body>
  <h1>Temperature Sensor Variability</h1>
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
  <div class="card">
    <div class="label">Difference Trend</div>
    <canvas id="diffChart" width="360" height="140"></canvas>
  </div>
  <div class="card">
    <div class="label">Difference Histogram</div>
    <canvas id="histChart" width="360" height="140"></canvas>
  </div>
  <script>
    const diffHistory = [];
    const maxPoints = 90;
    const samplePeriodSec = 2;
    const histogramBinCount = 451;
    const histogramScale = 100;
    let histogramLatchedMax = 0;

    function fmt(v, sign) {
      return (sign && v >= 0 ? '+' : '') + v.toFixed(2);
    }

    function drawDiffChart() {
      const canvas = document.getElementById('diffChart');
      const ctx = canvas.getContext('2d');
      const w = canvas.width;
      const h = canvas.height;

      ctx.clearRect(0, 0, w, h);

      const left = 24;
      const right = w - 6;
      const top = 8;
      const bottom = h - 22;

      ctx.strokeStyle = '#cfcfcf';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(left, top);
      ctx.lineTo(left, bottom);
      ctx.lineTo(right, bottom);
      ctx.stroke();

      if (diffHistory.length === 0) {
        return;
      }

      let minV = diffHistory[0];
      let maxV = diffHistory[0];
      for (let i = 1; i < diffHistory.length; i++) {
        minV = Math.min(minV, diffHistory[i]);
        maxV = Math.max(maxV, diffHistory[i]);
      }

      const span = Math.max(0.2, maxV - minV);
      const pad = span * 0.08;
      minV -= pad;
      maxV += pad;

      function mapY(v) {
        const t = (v - minV) / (maxV - minV);
        return bottom - t * (bottom - top);
      }

      if (minV < 0 && maxV > 0) {
        const y0 = mapY(0);
        ctx.strokeStyle = '#e0e0e0';
        ctx.beginPath();
        ctx.moveTo(left, y0);
        ctx.lineTo(right, y0);
        ctx.stroke();
      }

      ctx.strokeStyle = '#00796b';
      ctx.lineWidth = 2;
      ctx.beginPath();

      const n = diffHistory.length;
      for (let i = 0; i < n; i++) {
        const x = left + (i * (right - left)) / Math.max(1, n - 1);
        const y = mapY(diffHistory[i]);
        if (i === 0) {
          ctx.moveTo(x, y);
        } else {
          ctx.lineTo(x, y);
        }
      }
      ctx.stroke();

      // Point markers: draw all points for short traces, decimate for long traces.
      const markerStep = (n <= 40) ? 1 : Math.ceil(n / 40);
      ctx.fillStyle = '#00796b';
      for (let i = 0; i < n; i += markerStep) {
        const x = left + (i * (right - left)) / Math.max(1, n - 1);
        const y = mapY(diffHistory[i]);
        ctx.beginPath();
        ctx.arc(x, y, 1.7, 0, Math.PI * 2);
        ctx.fill();
      }

      // X-axis time ticks based on sample cadence.
      const tickCount = 4;
      const oldestAgeSec = Math.max(0, (n - 1) * samplePeriodSec);
      ctx.fillStyle = '#666';
      ctx.font = '9px sans-serif';
      ctx.textAlign = 'center';
      for (let t = 0; t < tickCount; t++) {
        const ratio = t / (tickCount - 1);
        const x = left + ratio * (right - left);
        const age = Math.round((1 - ratio) * oldestAgeSec);
        const label = (t === tickCount - 1) ? 'now' : ('-' + age + 's');
        ctx.fillText(label, x, h - 6);
      }
      ctx.textAlign = 'left';

      ctx.fillStyle = '#666';
      ctx.font = '10px sans-serif';
      ctx.fillText(maxV.toFixed(2), 2, top + 8);
      ctx.fillText(minV.toFixed(2), 2, bottom);
    }

    function drawHistogramChart() {
      const canvas = document.getElementById('histChart');
      const ctx = canvas.getContext('2d');
      const w = canvas.width;
      const h = canvas.height;

      ctx.clearRect(0, 0, w, h);

      const left = 28;
      const right = w - 6;
      const top = 8;
      const bottom = h - 22;

      ctx.strokeStyle = '#cfcfcf';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(left, top);
      ctx.lineTo(left, bottom);
      ctx.lineTo(right, bottom);
      ctx.stroke();

      if (diffHistory.length === 0) {
        return;
      }

      const scaled = new Array(diffHistory.length);
      let sumScaled = 0;
      let minScaled = Math.round(diffHistory[0] * histogramScale);
      let maxScaled = minScaled;

      for (let i = 0; i < diffHistory.length; i++) {
        const v = Math.round(diffHistory[i] * histogramScale);
        scaled[i] = v;
        sumScaled += v;
        minScaled = Math.min(minScaled, v);
        maxScaled = Math.max(maxScaled, v);
      }

      const meanScaled = sumScaled / scaled.length;
      const halfRangeScaled = Math.max(1, Math.max(Math.abs(meanScaled - minScaled), Math.abs(maxScaled - meanScaled)));

      const bins = new Array(histogramBinCount).fill(0);
      const minDomain = meanScaled - halfRangeScaled;
      const span = 2 * halfRangeScaled;

      for (let i = 0; i < scaled.length; i++) {
        const normalized = (scaled[i] - minDomain) / span;
        let idx = Math.round(normalized * (histogramBinCount - 1));
        idx = Math.max(0, Math.min(histogramBinCount - 1, idx));
        bins[idx]++;
      }

      const plotWidth = right - left + 1;
      const bucketCounts = new Array(plotWidth).fill(0);
      let maxBucketCount = 0;
      for (let col = 0; col < plotWidth; col++) {
        const start = Math.floor((col * histogramBinCount) / plotWidth);
        const end = Math.floor(((col + 1) * histogramBinCount) / plotWidth);
        let count = 0;
        for (let i = start; i < end; i++) {
          count += bins[i];
        }
        bucketCounts[col] = count;
        maxBucketCount = Math.max(maxBucketCount, count);
      }

      histogramLatchedMax = Math.max(histogramLatchedMax, maxBucketCount);
      const yMax = Math.max(1, histogramLatchedMax);

      ctx.fillStyle = '#666';
      ctx.font = '10px sans-serif';
      ctx.fillText(String(yMax), 2, top + 8);
      ctx.fillText('0', 2, bottom);

      ctx.strokeStyle = '#1976d2';
      for (let col = 0; col < plotWidth; col++) {
        const count = bucketCounts[col];
        if (count <= 0) continue;
        const barHeight = Math.max(1, Math.round((count / yMax) * (bottom - top)));
        const x = left + col;
        const y = bottom - barHeight;
        ctx.beginPath();
        ctx.moveTo(x + 0.5, bottom);
        ctx.lineTo(x + 0.5, y);
        ctx.stroke();
      }

      const centerX = left + Math.floor((plotWidth - 1) / 2);
      ctx.strokeStyle = '#b0bec5';
      ctx.beginPath();
      for (let y = top; y <= bottom; y += 4) {
        ctx.moveTo(centerX, y);
        ctx.lineTo(centerX, Math.min(bottom, y + 1));
      }
      ctx.stroke();

      const xMin = (meanScaled - halfRangeScaled) / histogramScale;
      const xMid = meanScaled / histogramScale;
      const xMax = (meanScaled + halfRangeScaled) / histogramScale;
      ctx.fillStyle = '#666';
      ctx.font = '9px sans-serif';
      ctx.textAlign = 'center';
      ctx.fillText(xMin.toFixed(2), left, h - 6);
      ctx.fillText(xMid.toFixed(2), centerX, h - 6);
      ctx.fillText(xMax.toFixed(2), right, h - 6);
      ctx.textAlign = 'left';
    }

    async function refresh() {
      try {
        const d = await fetch('/api/sensors').then(r => r.json());
        document.getElementById('bmp').textContent  = fmt(d.bmp);
        document.getElementById('sht').textContent  = fmt(d.sht);
        document.getElementById('diff').textContent = fmt(d.diff, true);

        diffHistory.push(d.diff);
        if (diffHistory.length > maxPoints) {
          diffHistory.shift();
        }
        drawDiffChart();
        drawHistogramChart();
      } catch(e) {}
    }
    refresh();
    setInterval(refresh, 2000);
  </script>
</body>
</html>
)rawliteral";

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

static void handleRoot() {
    logRequest(200);
    server.send(200, "text/html", INDEX_HTML);
}

static void handleApiSensors() {
    logRequest(200);
    char buf[72];
    snprintf(buf, sizeof(buf),
             "{\"bmp\":%.2f,\"sht\":%.2f,\"diff\":%.2f}",
             lastSensorReading.bmpF, lastSensorReading.shtF, lastSensorReading.deltaF);
    server.send(200, "application/json", buf);
}

static void handleApiBmp() {
    logRequest(200);
    char buf[24];
    snprintf(buf, sizeof(buf), "{\"bmp\":%.2f}", lastSensorReading.bmpF);
    server.send(200, "application/json", buf);
}

static void handleApiSht() {
    logRequest(200);
    char buf[24];
    snprintf(buf, sizeof(buf), "{\"sht\":%.2f}", lastSensorReading.shtF);
    server.send(200, "application/json", buf);
}

static void handleApiDiff() {
    logRequest(200);
    char buf[24];
    snprintf(buf, sizeof(buf), "{\"diff\":%+.2f}", lastSensorReading.deltaF);
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
    lastSensorReading = readings;
}

void webHandleClients() {
    dnsServer.processNextRequest();
    server.handleClient();
}
