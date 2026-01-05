let config = {};
let savedConfig = {}; // Track the last saved/loaded config
let outputEnabled = true;
let linkedTransition = false; // Track if rise/fall times are linked
let linkedHigh = false; // Track if min/max High times are linked
let linkedLow = false;  // Track if min/max Low times are linked
const canvas = document.getElementById('waveform');
const ctx = canvas.getContext('2d');
const liveCanvas = document.getElementById('liveTrace');
const liveCtx = liveCanvas.getContext('2d');

// API base URL - will be set to IP address for reliability
let apiBase = '';

// Get the server's IP address and use it for all API calls
function initApiBase() {
  // If we loaded via IP already, just use current origin
  if (/^\d+\.\d+\.\d+\.\d+/.test(location.hostname)) {
    apiBase = '';
    return Promise.resolve();
  }
  // Otherwise, fetch the IP from the status endpoint and switch to it
  return fetch('/status')
    .then(r => r.json())
    .then(data => {
      if (data.ip && data.ip !== '0.0.0.0') {
        apiBase = 'http://' + data.ip;
        console.log('Using IP for API calls:', apiBase);
      }
    })
    .catch(() => {
      apiBase = ''; // Fallback to relative URLs
    });
}

// Fetch with retry for reliability
function fetchWithRetry(url, options = {}, retries = 2) {
  const fullUrl = apiBase + url;
  return fetch(fullUrl, options).catch(err => {
    if (retries > 0) {
      return new Promise(resolve => setTimeout(resolve, 200))
        .then(() => fetchWithRetry(url, options, retries - 1));
    }
    throw err;
  });
}

// Toast notification
function showToast(message, isError = false) {
  const existing = document.querySelector('.toast');
  if (existing) existing.remove();

  const toast = document.createElement('div');
  toast.className = 'toast' + (isError ? ' toast-error' : '');
  toast.textContent = message;
  document.body.appendChild(toast);

  setTimeout(() => toast.classList.add('show'), 10);
  setTimeout(() => {
    toast.classList.remove('show');
    setTimeout(() => toast.remove(), 300);
  }, 2000);
}

function exportConfig() {
  fetchWithRetry('/export')
    .then(r => r.blob())
    .then(blob => {
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'o2sim_config.json';
      a.click();
      URL.revokeObjectURL(url);
      showToast('Config exported successfully');
    })
    .catch(() => showToast('Export failed', true));
}

// Live trace data - circular buffer for efficiency (no shift() needed)
// 1000 points = 5 seconds of history at 200 samples/sec
const maxPoints = 1000;
const traceV = new Float32Array(maxPoints); // Voltage values
const traceS = new Uint8Array(maxPoints);   // State values
let traceHead = 0;  // Write position (oldest data)
let traceCount = 0; // Number of valid samples (0 to maxPoints)
const stateColors = ['#2196F3', '#FF9800', '#4CAF50', '#E91E63']; // Low, Rise, High, Fall
let coloredTrace = true; // Toggle between colored and solid green

// Helper to get array index from logical index (0 = oldest, traceCount-1 = newest)
function traceIdx(i) {
  return (traceHead + i) % maxPoints;
}

// Add a sample to the circular buffer
function addTrace(v, s) {
  const writeIdx = (traceHead + traceCount) % maxPoints;
  traceV[writeIdx] = v;
  traceS[writeIdx] = s;
  if (traceCount < maxPoints) {
    traceCount++;
  } else {
    traceHead = (traceHead + 1) % maxPoints; // Overwrite oldest
  }
}

// Double-click live graph to toggle colored mode
document.getElementById('liveTrace').addEventListener('dblclick', () => {
  coloredTrace = !coloredTrace;
  drawLiveTrace();
});

function resizeCanvas() {
  canvas.width = canvas.offsetWidth * 2;
  canvas.height = canvas.offsetHeight * 2;
  ctx.scale(2, 2);

  liveCanvas.width = liveCanvas.offsetWidth * 2;
  liveCanvas.height = liveCanvas.offsetHeight * 2;
  liveCtx.scale(2, 2);

  drawWaveform();
  drawLiveTrace();
}

function loadConfig() {
  return fetchWithRetry('/config')
    .then(r => r.json())
    .then(data => {
      updateConfigUI(data);
    });
}

function updateConfigUI(data, isSaved = true) {
  config = data;
  if (isSaved) {
    savedConfig = { ...data }; // Store as saved state
  }
  document.getElementById('maxVoltage').value = data.maxVoltage;
  document.getElementById('minVoltage').value = data.minVoltage;
  document.getElementById('riseTime').value = data.riseTime;
  document.getElementById('fallTime').value = data.fallTime;
  document.getElementById('minHighTime').value = data.minHighTime;
  document.getElementById('maxHighTime').value = data.maxHighTime;
  document.getElementById('minLowTime').value = data.minLowTime;
  document.getElementById('maxLowTime').value = data.maxLowTime;
  outputEnabled = data.outputEnabled !== false;
  updateToggleButton();
  // Reset all link states
  resetLinkStates();
  drawWaveform();
  updateButtonStates();
}

// Reset all link buttons to unlinked state
function resetLinkStates() {
  linkedTransition = false;
  linkedHigh = false;
  linkedLow = false;
  document.getElementById('linkTransition').classList.remove('linked');
  document.getElementById('linkHigh').classList.remove('linked');
  document.getElementById('linkLow').classList.remove('linked');
}

// Check if current form values differ from saved config
function hasUnsavedChanges() {
  const fields = ['maxVoltage', 'minVoltage', 'riseTime', 'fallTime', 'minHighTime', 'maxHighTime', 'minLowTime', 'maxLowTime'];
  for (const field of fields) {
    const current = parseFloat(document.getElementById(field).value);
    const saved = savedConfig[field];
    if (current !== saved) return true;
  }
  return false;
}

// Update Apply/Reset button states based on changes
function updateButtonStates() {
  const hasChanges = hasUnsavedChanges();
  const applyBtn = document.querySelector('.btn-group-right button:last-child');
  const resetBtn = document.querySelector('.btn-group-right .btn-secondary');
  if (applyBtn) {
    applyBtn.disabled = !hasChanges;
    applyBtn.style.opacity = hasChanges ? '1' : '0.5';
  }
  if (resetBtn) {
    resetBtn.disabled = !hasChanges;
    resetBtn.style.opacity = hasChanges ? '1' : '0.5';
  }
}

function updateToggleButton() {
  const btn = document.getElementById('toggleBtn');
  const status = document.getElementById('outputStatus');
  if (outputEnabled) {
    btn.textContent = 'ON';
    btn.classList.remove('off');
    status.textContent = 'Active';
    status.className = 'output-active';
  } else {
    btn.textContent = 'OFF';
    btn.classList.add('off');
    status.textContent = 'Disabled';
    status.className = 'output-disabled';
  }
}

function toggleOutput() {
  fetchWithRetry('/toggle', { method: 'POST' })
    .then(r => r.json())
    .then(data => {
      outputEnabled = data.enabled;
      updateToggleButton();
    });
}

// Preset values (must match ESP32 presets)
const presets = {
  normal: { minVoltage: 0.1, maxVoltage: 0.8, riseTime: 0.7, fallTime: 1.1, minHighTime: 1.25, maxHighTime: 10, minLowTime: 1.25, maxLowTime: 5 },
  aggressive: { minVoltage: 0.1, maxVoltage: 0.9, riseTime: 0.3, fallTime: 0.5, minHighTime: 0.5, maxHighTime: 3, minLowTime: 0.5, maxLowTime: 2 },
  slow: { minVoltage: 0.2, maxVoltage: 0.7, riseTime: 2.0, fallTime: 2.5, minHighTime: 3, maxHighTime: 15, minLowTime: 3, maxLowTime: 10 }
};

function loadPreset(name) {
  // Only update UI with preset values - user must click Apply to send to ESP32
  if (presets[name]) {
    const preset = presets[name];
    // Reset link states so preset values aren't overwritten
    resetLinkStates();
    document.getElementById('maxVoltage').value = preset.maxVoltage;
    document.getElementById('minVoltage').value = preset.minVoltage;
    document.getElementById('riseTime').value = preset.riseTime;
    document.getElementById('fallTime').value = preset.fallTime;
    document.getElementById('minHighTime').value = preset.minHighTime;
    document.getElementById('maxHighTime').value = preset.maxHighTime;
    document.getElementById('minLowTime').value = preset.minLowTime;
    document.getElementById('maxLowTime').value = preset.maxLowTime;
    // Update preview waveform with new values
    config = { ...config, ...preset };
    drawWaveform();
    updateButtonStates();
  }
}

// Toggle link between paired values
function toggleLink(type) {
  const btn = document.getElementById('link' + type);
  if (type === 'Transition') {
    linkedTransition = !linkedTransition;
    btn.classList.toggle('linked', linkedTransition);
    if (linkedTransition) {
      // Sync to rise time when linking
      const riseVal = document.getElementById('riseTime').value;
      document.getElementById('fallTime').value = riseVal;
      config.riseTime = parseFloat(riseVal);
      config.fallTime = parseFloat(riseVal);
    } else {
      // Restore original saved values when unlinking
      document.getElementById('riseTime').value = savedConfig.riseTime;
      document.getElementById('fallTime').value = savedConfig.fallTime;
      config.riseTime = savedConfig.riseTime;
      config.fallTime = savedConfig.fallTime;
    }
    drawWaveform();
    updateButtonStates();
  } else if (type === 'High') {
    linkedHigh = !linkedHigh;
    btn.classList.toggle('linked', linkedHigh);
    if (linkedHigh) {
      // Sync to the min value when linking
      const minVal = document.getElementById('minHighTime').value;
      document.getElementById('maxHighTime').value = minVal;
      config.maxHighTime = parseFloat(minVal);
      config.minHighTime = parseFloat(minVal);
    } else {
      // Restore original saved values when unlinking
      document.getElementById('minHighTime').value = savedConfig.minHighTime;
      document.getElementById('maxHighTime').value = savedConfig.maxHighTime;
      config.minHighTime = savedConfig.minHighTime;
      config.maxHighTime = savedConfig.maxHighTime;
    }
    drawWaveform();
    updateButtonStates();
  } else if (type === 'Low') {
    linkedLow = !linkedLow;
    btn.classList.toggle('linked', linkedLow);
    if (linkedLow) {
      // Sync to the min value when linking
      const minVal = document.getElementById('minLowTime').value;
      document.getElementById('maxLowTime').value = minVal;
      config.maxLowTime = parseFloat(minVal);
      config.minLowTime = parseFloat(minVal);
    } else {
      // Restore original saved values when unlinking
      document.getElementById('minLowTime').value = savedConfig.minLowTime;
      document.getElementById('maxLowTime').value = savedConfig.maxLowTime;
      config.minLowTime = savedConfig.minLowTime;
      config.maxLowTime = savedConfig.maxLowTime;
    }
    drawWaveform();
    updateButtonStates();
  }
}

function importConfig(input) {
  const file = input.files[0];
  if (!file) return;

  const reader = new FileReader();
  reader.onload = (e) => {
    fetchWithRetry('/import', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: e.target.result
    }).then(r => {
      if (r.ok) {
        showToast('Config imported successfully');
        loadConfig();
      } else {
        showToast('Import failed', true);
      }
      input.value = '';
    });
  };
  reader.readAsText(file);
}

function loadStatus() {
  fetchWithRetry('/status')
    .then(r => r.json())
    .then(data => {
      document.getElementById('versionInfo').textContent = 'v' + data.version;
    })
    .catch(() => {});
}

function validateMinMax() {
  const minVEl = document.getElementById('minVoltage');
  const maxVEl = document.getElementById('maxVoltage');
  const minHighEl = document.getElementById('minHighTime');
  const maxHighEl = document.getElementById('maxHighTime');
  const minLowEl = document.getElementById('minLowTime');
  const maxLowEl = document.getElementById('maxLowTime');

  const minV = parseFloat(minVEl.value);
  const maxV = parseFloat(maxVEl.value);
  const minHigh = parseFloat(minHighEl.value);
  const maxHigh = parseFloat(maxHighEl.value);
  const minLow = parseFloat(minLowEl.value);
  const maxLow = parseFloat(maxLowEl.value);

  // Clear previous error styling
  [minVEl, maxVEl, minHighEl, maxHighEl, minLowEl, maxLowEl].forEach(el => {
    el.style.borderColor = '#555';
  });

  let valid = true;
  let errors = [];

  if (minV > maxV) {
    errors.push('Min Voltage > Max Voltage');
    minVEl.style.borderColor = '#e74c3c';
    maxVEl.style.borderColor = '#e74c3c';
    valid = false;
  }
  if (minHigh > maxHigh) {
    errors.push('Min High Time > Max High Time');
    minHighEl.style.borderColor = '#e74c3c';
    maxHighEl.style.borderColor = '#e74c3c';
    valid = false;
  }
  if (minLow > maxLow) {
    errors.push('Min Low Time > Max Low Time');
    minLowEl.style.borderColor = '#e74c3c';
    maxLowEl.style.borderColor = '#e74c3c';
    valid = false;
  }

  return { valid, errors };
}

function applyConfig() {
  const validation = validateMinMax();
  if (!validation.valid) {
    alert('Invalid config:\n' + validation.errors.join('\n'));
    return;
  }

  const data = {
    maxVoltage: parseFloat(document.getElementById('maxVoltage').value),
    minVoltage: parseFloat(document.getElementById('minVoltage').value),
    riseTime: parseFloat(document.getElementById('riseTime').value),
    fallTime: parseFloat(document.getElementById('fallTime').value),
    minHighTime: parseFloat(document.getElementById('minHighTime').value),
    maxHighTime: parseFloat(document.getElementById('maxHighTime').value),
    minLowTime: parseFloat(document.getElementById('minLowTime').value),
    maxLowTime: parseFloat(document.getElementById('maxLowTime').value)
  };

  // Optimistic UI - update immediately
  config = data;
  savedConfig = { ...data };
  drawWaveform();
  updateButtonStates();
  showToast('Settings saved');

  // Send to server in background
  fetchWithRetry('/config', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(data)
  }).then(r => {
    if (!r.ok) {
      showToast('Save failed - reload to restore', true);
    }
  }).catch(() => showToast('Save failed - reload to restore', true));
}

function drawWaveform() {
  const w = canvas.offsetWidth, h = canvas.offsetHeight;
  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = '#222';
  ctx.fillRect(0, 0, w, h);

  // Grid lines at 0.25V intervals
  ctx.strokeStyle = '#444';
  ctx.lineWidth = 0.5;
  ctx.beginPath();
  for (let i = 0; i <= 4; i++) {
    const y = 10 + (h - 20) * i / 4;
    ctx.moveTo(30, y);
    ctx.lineTo(w, y);
  }
  ctx.stroke();

  // Voltage labels
  ctx.fillStyle = '#666';
  ctx.font = '10px sans-serif';
  ctx.fillText('1V', 3, 14);
  ctx.fillText('.75', 3, 10 + (h - 20) * 0.25 + 4);
  ctx.fillText('.5V', 3, 10 + (h - 20) * 0.5 + 4);
  ctx.fillText('.25', 3, 10 + (h - 20) * 0.75 + 4);
  ctx.fillText('0V', 3, h - 3);

  const minV = config.minVoltage ?? 0;
  const maxV = config.maxVoltage ?? 0.8;
  const riseT = config.riseTime ?? 0.7;
  const fallT = config.fallTime ?? 1.1;
  const highT = config.minHighTime ?? 1.25;
  const lowT = config.minLowTime ?? 1.25;

  // Match live trace exactly: same graphWidth and step calculation
  const graphWidth = w - 35; // Same as live trace
  const step = graphWidth / (maxPoints - 1); // Same step as live trace
  const pxPerSec = step * samplesPerSecond; // Convert to pixels per second

  const voltToY = (v) => h - 10 - (v / 1.0 * (h - 20));

  // Colors for each segment
  const colors = {
    low: '#2196F3',    // Blue
    rise: '#FF9800',   // Orange
    high: '#4CAF50',   // Green
    fall: '#E91E63'    // Pink
  };

  ctx.lineWidth = 2;
  let x = 30;
  let isFirstCycle = true;

  // Draw cycles: first one starts with short lead-in, then full low times after
  while (x < w - 5) {
    // Low hold - short lead-in for first cycle, full duration after
    const thisLowT = isFirstCycle ? 0.2 : lowT;
    ctx.strokeStyle = colors.low;
    ctx.beginPath();
    ctx.moveTo(x, voltToY(minV));
    const lowEnd = Math.min(x + thisLowT * pxPerSec, w - 5);
    ctx.lineTo(lowEnd, voltToY(minV));
    ctx.stroke();
    x += thisLowT * pxPerSec;
    if (x >= w - 5) break;

    // Rise - same formula as ESP32: sin(p * PI/2)
    const risePoints = Math.max(40, Math.ceil(riseT * pxPerSec));
    ctx.strokeStyle = colors.rise;
    ctx.beginPath();
    ctx.moveTo(x, voltToY(minV));
    for (let i = 0; i <= risePoints; i++) {
      let p = i / risePoints;
      let px = x + riseT * pxPerSec * p;
      if (px > w - 5) break;
      let v = minV + (maxV - minV) * Math.sin(p * Math.PI / 2);
      ctx.lineTo(px, voltToY(v));
    }
    ctx.stroke();
    x += riseT * pxPerSec;
    if (x >= w - 5) break;

    // High hold
    ctx.strokeStyle = colors.high;
    ctx.beginPath();
    ctx.moveTo(x, voltToY(maxV));
    const highEnd = Math.min(x + highT * pxPerSec, w - 5);
    ctx.lineTo(highEnd, voltToY(maxV));
    ctx.stroke();
    x += highT * pxPerSec;
    if (x >= w - 5) break;

    // Fall - same formula as ESP32: 1 - cos(p * PI/2)
    const fallPoints = Math.max(40, Math.ceil(fallT * pxPerSec));
    ctx.strokeStyle = colors.fall;
    ctx.beginPath();
    ctx.moveTo(x, voltToY(maxV));
    for (let i = 0; i <= fallPoints; i++) {
      let p = i / fallPoints;
      let px = x + fallT * pxPerSec * p;
      if (px > w - 5) break;
      let v = maxV - (maxV - minV) * (1 - Math.cos(p * Math.PI / 2));
      ctx.lineTo(px, voltToY(v));
    }
    ctx.stroke();
    x += fallT * pxPerSec;

    isFirstCycle = false;
  }

  // Legend
  ctx.font = '9px sans-serif';
  const legendY = h - 3;
  const legendItems = [
    { label: 'Low', color: colors.low },
    { label: 'Rise', color: colors.rise },
    { label: 'High', color: colors.high },
    { label: 'Fall', color: colors.fall }
  ];
  let legendX = 35;
  legendItems.forEach(item => {
    ctx.fillStyle = item.color;
    ctx.fillText(item.label, legendX, legendY);
    legendX += ctx.measureText(item.label).width + 10;
  });
}

function drawLiveTrace() {
  const w = liveCanvas.offsetWidth, h = liveCanvas.offsetHeight;
  liveCtx.clearRect(0, 0, w, h);
  liveCtx.fillStyle = '#222';
  liveCtx.fillRect(0, 0, w, h);

  // Grid lines at 0.25V intervals
  liveCtx.strokeStyle = '#444';
  liveCtx.lineWidth = 0.5;
  liveCtx.beginPath();
  for (let i = 0; i <= 4; i++) {
    const y = 8 + (h - 16) * i / 4;
    liveCtx.moveTo(30, y);
    liveCtx.lineTo(w, y);
  }
  liveCtx.stroke();

  // Voltage labels
  liveCtx.fillStyle = '#666';
  liveCtx.font = '10px sans-serif';
  liveCtx.fillText('1V', 3, 12);
  liveCtx.fillText('.75', 3, 8 + (h - 16) * 0.25 + 4);
  liveCtx.fillText('.5V', 3, 8 + (h - 16) * 0.5 + 4);
  liveCtx.fillText('.25', 3, 8 + (h - 16) * 0.75 + 4);
  liveCtx.fillText('0V', 3, h - 3);

  if (traceCount < 2) return;

  const graphHeight = h - 16;
  const graphWidth = w - 35;
  const step = graphWidth / (maxPoints - 1);
  const startX = 30;

  // Draw trace - colored or solid green based on toggle
  liveCtx.lineWidth = 2;

  // Get first sample - direct array access, no object creation
  let idx = traceIdx(0);
  let v = traceV[idx];
  let s = traceS[idx];
  let y = h - 8 - (v * graphHeight);

  if (!coloredTrace) {
    // Simple solid green line
    liveCtx.strokeStyle = '#4CAF50';
    liveCtx.beginPath();
    liveCtx.moveTo(startX, y);
    for (let i = 1; i < traceCount; i++) {
      idx = traceIdx(i);
      y = h - 8 - (traceV[idx] * graphHeight);
      liveCtx.lineTo(startX + i * step, y);
    }
    liveCtx.stroke();
  } else {
    // Colored segments based on state
    let curState = s;
    liveCtx.strokeStyle = stateColors[curState] || '#4CAF50';
    liveCtx.beginPath();
    liveCtx.moveTo(startX, y);

    for (let i = 1; i < traceCount; i++) {
      idx = traceIdx(i);
      v = traceV[idx];
      s = traceS[idx];
      const x = startX + i * step;
      y = h - 8 - (v * graphHeight);

      if (s !== curState) {
        liveCtx.lineTo(x, y);
        liveCtx.stroke();
        curState = s;
        liveCtx.strokeStyle = stateColors[curState] || '#4CAF50';
        liveCtx.beginPath();
        liveCtx.moveTo(x, y);
      } else {
        liveCtx.lineTo(x, y);
      }
    }
    liveCtx.stroke();
  }
}

// Client-side animation with pre-buffered waveform
let animationId = null;
let lastAnimTime = 0;
let sampleAccumulator = 0;
let drawAccumulator = 0;
const samplesPerSecond = 200; // Must match ESP32's 5ms sample interval
const msPerSample = 1000 / samplesPerSecond; // 5ms per sample
const drawInterval = 16; // ~60fps for smooth drawing

// Pre-buffered waveform - generate ahead, playback is just array reads
const waveBufferSize = 8000; // ~40 seconds of samples at 200/sec
const waveBufferV = new Float32Array(waveBufferSize);
const waveBufferS = new Uint8Array(waveBufferSize);
let waveBufferLen = 0;
let wavePlayIdx = 0;

// Generate one complete cycle and append to buffer
function generateCycle() {
  const minV = config.minVoltage || 0;
  const maxV = config.maxVoltage || 0.8;
  const riseMs = (config.riseTime || 0.7) * 1000;
  const fallMs = (config.fallTime || 1.1) * 1000;
  const minHighMs = (config.minHighTime || 1.25) * 1000;
  const maxHighMs = (config.maxHighTime || 10) * 1000;
  const minLowMs = (config.minLowTime || 1.25) * 1000;
  const maxLowMs = (config.maxLowTime || 5) * 1000;

  // Random hold times for this cycle
  const lowHoldMs = minLowMs + Math.random() * (maxLowMs - minLowMs);
  const highHoldMs = minHighMs + Math.random() * (maxHighMs - minHighMs);

  // Calculate samples for each phase
  const lowSamples = Math.round(lowHoldMs / msPerSample);
  const riseSamples = Math.round(riseMs / msPerSample);
  const highSamples = Math.round(highHoldMs / msPerSample);
  const fallSamples = Math.round(fallMs / msPerSample);

  let idx = waveBufferLen;

  // Low phase
  for (let i = 0; i < lowSamples && idx < waveBufferSize; i++, idx++) {
    waveBufferV[idx] = minV;
    waveBufferS[idx] = 0;
  }

  // Rise phase
  for (let i = 0; i < riseSamples && idx < waveBufferSize; i++, idx++) {
    const p = i / riseSamples;
    waveBufferV[idx] = minV + (maxV - minV) * Math.sin(p * Math.PI / 2);
    waveBufferS[idx] = 1;
  }

  // High phase
  for (let i = 0; i < highSamples && idx < waveBufferSize; i++, idx++) {
    waveBufferV[idx] = maxV;
    waveBufferS[idx] = 2;
  }

  // Fall phase
  for (let i = 0; i < fallSamples && idx < waveBufferSize; i++, idx++) {
    const p = i / fallSamples;
    waveBufferV[idx] = maxV - (maxV - minV) * (1 - Math.cos(p * Math.PI / 2));
    waveBufferS[idx] = 3;
  }

  waveBufferLen = idx;
}

// Ensure we have enough samples buffered ahead
function ensureBuffer(needed) {
  while (waveBufferLen - wavePlayIdx < needed + 400 && waveBufferLen < waveBufferSize) {
    generateCycle();
  }

  // If buffer is getting full, shift data to reclaim space
  if (wavePlayIdx > waveBufferSize / 2) {
    const remaining = waveBufferLen - wavePlayIdx;
    waveBufferV.copyWithin(0, wavePlayIdx, waveBufferLen);
    waveBufferS.copyWithin(0, wavePlayIdx, waveBufferLen);
    waveBufferLen = remaining;
    wavePlayIdx = 0;
  }
}

function animateTrace(timestamp) {
  if (!lastAnimTime) {
    lastAnimTime = timestamp;
    ensureBuffer(200);
    return requestAnimationFrame(animateTrace);
  }

  const elapsed = timestamp - lastAnimTime;
  lastAnimTime = timestamp;

  sampleAccumulator += elapsed;
  drawAccumulator += elapsed;

  if (sampleAccumulator > 500) sampleAccumulator = 500;
  if (drawAccumulator > 500) drawAccumulator = 500;

  const samplesToAdd = Math.floor(sampleAccumulator / msPerSample);

  if (samplesToAdd > 0) {
    sampleAccumulator -= samplesToAdd * msPerSample;

    let lastV = 0;
    if (outputEnabled) {
      ensureBuffer(samplesToAdd);
      for (let i = 0; i < samplesToAdd && wavePlayIdx < waveBufferLen; i++) {
        lastV = waveBufferV[wavePlayIdx];
        addTrace(lastV, waveBufferS[wavePlayIdx]);
        wavePlayIdx++;
      }
    } else {
      // Output disabled - show 0V
      for (let i = 0; i < samplesToAdd; i++) {
        addTrace(0, 0);
      }
    }

    // Update voltage display to match the chart
    document.getElementById('voltage').textContent = lastV.toFixed(2);

    if (drawAccumulator >= drawInterval) {
      drawAccumulator = 0;
      drawLiveTrace();
    }
  }

  animationId = requestAnimationFrame(animateTrace);
}

function startAnimation() {
  if (animationId) return;

  waveBufferLen = 0;
  wavePlayIdx = 0;

  animationId = requestAnimationFrame(animateTrace);
}

// No-op for compatibility (voltage updates from animation now)
function updateVoltage() {}

// Initialize
window.addEventListener('resize', resizeCanvas);
resizeCanvas();

// Init API base (get IP), then start everything
initApiBase().then(() => {
  loadStatus();
  // Wait for config before starting animation (so first cycle uses correct values)
  loadConfig().then(() => {
    startAnimation();
  });
});

// Live preview on input change with auto-correction
document.querySelectorAll('input').forEach(el => {
  el.addEventListener('input', () => {
    const val = parseFloat(el.value);

    // Sync linked values
    if (el.id === 'riseTime' && linkedTransition) {
      document.getElementById('fallTime').value = el.value;
    } else if (el.id === 'fallTime' && linkedTransition) {
      document.getElementById('riseTime').value = el.value;
    } else if (el.id === 'minHighTime' && linkedHigh) {
      document.getElementById('maxHighTime').value = el.value;
    } else if (el.id === 'maxHighTime' && linkedHigh) {
      document.getElementById('minHighTime').value = el.value;
    } else if (el.id === 'minLowTime' && linkedLow) {
      document.getElementById('maxLowTime').value = el.value;
    } else if (el.id === 'maxLowTime' && linkedLow) {
      document.getElementById('minLowTime').value = el.value;
    }

    // Auto-correct min/max pairs (only when not linked)
    if (el.id === 'minVoltage') {
      const maxEl = document.getElementById('maxVoltage');
      if (val > parseFloat(maxEl.value)) maxEl.value = val;
    } else if (el.id === 'maxVoltage') {
      const minEl = document.getElementById('minVoltage');
      if (val < parseFloat(minEl.value)) minEl.value = val;
    } else if (el.id === 'minHighTime' && !linkedHigh) {
      const maxEl = document.getElementById('maxHighTime');
      if (val > parseFloat(maxEl.value)) maxEl.value = val;
    } else if (el.id === 'maxHighTime' && !linkedHigh) {
      const minEl = document.getElementById('minHighTime');
      if (val < parseFloat(minEl.value)) minEl.value = val;
    } else if (el.id === 'minLowTime' && !linkedLow) {
      const maxEl = document.getElementById('maxLowTime');
      if (val > parseFloat(maxEl.value)) maxEl.value = val;
    } else if (el.id === 'maxLowTime' && !linkedLow) {
      const minEl = document.getElementById('minLowTime');
      if (val < parseFloat(minEl.value)) minEl.value = val;
    }

    // Update config object with all current values
    config.minVoltage = parseFloat(document.getElementById('minVoltage').value);
    config.maxVoltage = parseFloat(document.getElementById('maxVoltage').value);
    config.riseTime = parseFloat(document.getElementById('riseTime').value);
    config.fallTime = parseFloat(document.getElementById('fallTime').value);
    config.minHighTime = parseFloat(document.getElementById('minHighTime').value);
    config.maxHighTime = parseFloat(document.getElementById('maxHighTime').value);
    config.minLowTime = parseFloat(document.getElementById('minLowTime').value);
    config.maxLowTime = parseFloat(document.getElementById('maxLowTime').value);

    drawWaveform();
    updateButtonStates();
  });
});

// Animate settings panel close
const settingsPanel = document.querySelector('.settings-panel');
const settingsContent = document.querySelector('.settings-content');

settingsPanel.addEventListener('click', (e) => {
  if (settingsPanel.open && e.target.closest('summary')) {
    e.preventDefault();
    settingsContent.classList.add('closing');
    settingsPanel.classList.add('closing');
    // Scroll to top immediately before closing
    window.scrollTo({ top: 0, behavior: 'instant' });
    settingsContent.addEventListener('animationend', () => {
      settingsContent.classList.remove('closing');
      settingsPanel.classList.remove('closing');
      settingsPanel.open = false;
    }, { once: true });
  }
});

// Auto-size settings panel to fill available space
function updateSettingsHeight() {
  const panel = document.querySelector('.settings-panel');
  const content = document.querySelector('.settings-content');
  if (!panel || !content) return;

  const stickyHeader = document.querySelector('.sticky-header');
  const headerBottom = stickyHeader ? stickyHeader.getBoundingClientRect().bottom : 200;
  const viewportHeight = window.innerHeight;
  const summaryHeight = panel.querySelector('summary').offsetHeight;
  const availableHeight = viewportHeight - headerBottom - summaryHeight - 30;
  content.style.maxHeight = Math.max(350, availableHeight) + 'px';
}

// Update height on page load and resize
updateSettingsHeight();
window.addEventListener('resize', updateSettingsHeight);

// Scroll to show settings when opened
settingsPanel.addEventListener('toggle', () => {
  if (settingsPanel.open) {
    // Delay height calculation to let browser finish layout
    setTimeout(() => {
      updateSettingsHeight();
      // Scroll the settings panel into view
      settingsPanel.scrollIntoView({ behavior: 'smooth', block: 'start' });
      // Recalculate after scroll settles
      setTimeout(updateSettingsHeight, 350);
    }, 50);
  }
});

