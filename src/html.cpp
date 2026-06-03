#include "html.h"

const char INDEX_HTML[] = R"rawliteral(
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
    const samplePeriodSec = 1;
    let histogramData = {
      bins: [],
      center: 0,
      halfRange: 1,
      scale: 100,
      latchedMax: 1,
      sampleCount: 0,
    };
    let lastGraphCount = 0;
    let lastHistSampleCount = 0;

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

      if (!histogramData.bins || histogramData.bins.length === 0) {
        return;
      }

      const bins = histogramData.bins;
      const binCount = bins.length;

      const plotWidth = right - left + 1;
      const bucketCounts = new Array(plotWidth).fill(0);
      let maxBucketCount = 0;
      for (let col = 0; col < plotWidth; col++) {
        const start = Math.floor((col * binCount) / plotWidth);
        const end = Math.floor(((col + 1) * binCount) / plotWidth);
        let count = 0;
        for (let i = start; i < end; i++) {
          count += bins[i];
        }
        bucketCounts[col] = count;
        maxBucketCount = Math.max(maxBucketCount, count);
      }

      const yMax = Math.max(1, histogramData.latchedMax, maxBucketCount);

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

      const halfRangeF = Math.max(1, histogramData.halfRange) / Math.max(1, histogramData.scale);
      const xMin = histogramData.center - halfRangeF;
      const xMid = histogramData.center;
      const xMax = histogramData.center + halfRangeF;
      ctx.fillStyle = '#666';
      ctx.font = '9px sans-serif';
      ctx.textAlign = 'center';
      ctx.fillText(xMin.toFixed(2), left, h - 6);
      ctx.fillText(xMid.toFixed(2), centerX, h - 6);
      ctx.fillText(xMax.toFixed(2), right, h - 6);
      ctx.textAlign = 'left';
    }

    function applyState(state) {
      document.getElementById('bmp').textContent  = fmt(state.bmp);
      document.getElementById('sht').textContent  = fmt(state.sht);
      document.getElementById('diff').textContent = fmt(state.diff, true);

      const history = (state.graph && Array.isArray(state.graph.history)) ? state.graph.history : [];
      diffHistory.length = 0;
      for (let i = 0; i < history.length; i++) {
        diffHistory.push(Number(history[i]));
      }

      const h = state.histogram || {};
      histogramData = {
        bins: Array.isArray(h.bins) ? h.bins : [],
        center: Number(h.center || 0),
        halfRange: Number(h.halfRange || 1),
        scale: Number(h.scale || 100),
        latchedMax: Number(h.latchedMax || 1),
        sampleCount: Number(h.sampleCount || 0),
      };

      lastGraphCount = Number((state.graph && state.graph.count) || history.length || 0);
      lastHistSampleCount = histogramData.sampleCount;

      drawDiffChart();
      drawHistogramChart();
    }

    function applyLive(live) {
      const bmp = Number(live.bmp);
      const sht = Number(live.sht);
      const diff = Number(live.diff);
      const graphCount = Number(live.graphCount || 0);
      const histSampleCount = Number(live.histSampleCount || 0);

      document.getElementById('bmp').textContent  = fmt(bmp);
      document.getElementById('sht').textContent  = fmt(sht);
      document.getElementById('diff').textContent = fmt(diff, true);

      // Keep low-bandwidth incremental updates only if we received exactly one new sample.
      if (graphCount === lastGraphCount + 1 && histSampleCount === lastHistSampleCount + 1) {
        diffHistory.push(diff);
        while (diffHistory.length > graphCount) {
          diffHistory.shift();
        }

        if (histogramData.bins && histogramData.bins.length > 0) {
          const centerIndex = Math.floor((histogramData.bins.length - 1) / 2);
          const scaledValue = Math.round(diff * histogramData.scale);
          const centerScaledValue = Math.round(histogramData.center * histogramData.scale);
          const offset = scaledValue - centerScaledValue;
          const index = centerIndex + offset;

          if (index >= 0 && index < histogramData.bins.length) {
            histogramData.bins[index] += 1;
            histogramData.sampleCount = histSampleCount;
            histogramData.halfRange = Math.max(histogramData.halfRange, Math.abs(offset));
            histogramData.latchedMax = Math.max(histogramData.latchedMax, histogramData.bins[index]);
          } else {
            return false;
          }
        }

        lastGraphCount = graphCount;
        lastHistSampleCount = histSampleCount;
        drawDiffChart();
        drawHistogramChart();
        return true;
      }

      // Any gap, reset, or recenter should trigger a full refresh.
      return false;
    }

    async function refreshFullState() {
      const state = await fetch('/api/state').then(r => r.json());
      applyState(state);
    }

    async function refresh() {
      try {
        const live = await fetch('/api/live').then(r => r.json());
        if (!applyLive(live)) {
          await refreshFullState();
        }
      } catch(e) {}
    }

    async function bootstrap() {
      try {
        await refreshFullState();
      } catch (e) {}
      setInterval(refresh, 1000);
    }

    bootstrap();
  </script>
</body>
</html>
)rawliteral";
