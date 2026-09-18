/**
 * @file web_assets.h
 * @brief Polished, mobile-first Hi-Fi Web UI for the ESP32-C3 WiFi-HiFi receiver.
 */

#pragma once

#include <pgmspace.h>

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>WiFi-HiFi | Receiver Control</title>
<style>
  :root {
    --bg-primary: #0f1117;
    --bg-surface: #181b24;
    --bg-card: #202430;
    --border: #2e3444;
    --text-main: #f0f3fa;
    --text-muted: #8e98ac;
    --accent: #d4af37; /* Hi-Fi Gold */
    --accent-glow: rgba(212, 175, 55, 0.2);
    --success: #10b981;
    --danger: #ef4444;
    --radius: 12px;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background-color: var(--bg-primary);
    color: var(--text-main);
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    line-height: 1.5;
    padding: 16px;
    max-width: 800px;
    margin: 0 auto;
  }
  header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 16px 0 24px;
    border-bottom: 1px solid var(--border);
    margin-bottom: 24px;
  }
  .brand { display: flex; align-items: center; gap: 12px; }
  .brand-title { font-size: 20px; font-weight: 700; letter-spacing: 0.5px; color: var(--accent); }
  .brand-sub { font-size: 12px; color: var(--text-muted); }
  .badge {
    padding: 4px 10px;
    border-radius: 9999px;
    font-size: 11px;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.5px;
  }
  .badge-live { background: rgba(16, 185, 129, 0.15); color: var(--success); border: 1px solid rgba(16, 185, 129, 0.3); }
  .badge-idle { background: rgba(142, 152, 172, 0.15); color: var(--text-muted); border: 1px solid var(--border); }
  
  .card {
    background: var(--bg-surface);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 20px;
    margin-bottom: 20px;
  }
  .card-title {
    font-size: 13px;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 1px;
    color: var(--accent);
    margin-bottom: 16px;
    display: flex;
    justify-content: space-between;
    align-items: center;
  }
  .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 14px; }
  .stat-label { font-size: 12px; color: var(--text-muted); }
  .stat-val { font-size: 15px; font-weight: 600; color: var(--text-main); margin-top: 2px; }
  
  /* Buffer Bar */
  .bar-container {
    width: 100%;
    height: 10px;
    background: #12141c;
    border-radius: 5px;
    overflow: hidden;
    margin-top: 6px;
    border: 1px solid var(--border);
  }
  .bar-fill {
    height: 100%;
    background: linear-gradient(90deg, #d4af37, #f59e0b);
    width: 0%;
    transition: width 0.3s ease;
  }
  
  /* Controls */
  .slider-row { display: flex; align-items: center; gap: 16px; margin: 12px 0; }
  input[type="range"] {
    flex: 1;
    accent-color: var(--accent);
    cursor: pointer;
  }
  .btn {
    background: var(--bg-card);
    color: var(--text-main);
    border: 1px solid var(--border);
    padding: 10px 18px;
    border-radius: 8px;
    font-size: 13px;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.2s;
  }
  .btn:hover { background: #282e3e; border-color: var(--accent); }
  .btn-gold { background: var(--accent); color: #000; border: none; }
  .btn-gold:hover { background: #e5be45; }
  .btn-danger { background: rgba(239, 68, 68, 0.15); color: var(--danger); border-color: rgba(239, 68, 68, 0.3); }
  .btn-danger:hover { background: var(--danger); color: #fff; }

  /* Forms */
  .form-group { margin-bottom: 12px; }
  label { display: block; font-size: 12px; color: var(--text-muted); margin-bottom: 4px; }
  input[type="text"], input[type="password"] {
    width: 100%;
    padding: 10px 12px;
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: 6px;
    color: #fff;
    font-size: 14px;
  }
  input[type="text"]:focus, input[type="password"]:focus {
    outline: none;
    border-color: var(--accent);
  }

  .nav-tabs {
    display: flex;
    gap: 8px;
    margin-bottom: 20px;
    overflow-x: auto;
    border-bottom: 1px solid var(--border);
    padding-bottom: 8px;
  }
  .tab-btn {
    background: transparent;
    border: none;
    color: var(--text-muted);
    font-size: 13px;
    font-weight: 600;
    padding: 6px 14px;
    border-radius: 6px;
    cursor: pointer;
  }
  .tab-btn.active {
    background: var(--bg-card);
    color: var(--accent);
  }
  .section { display: none; }
  .section.active { display: block; }

  #ota-progress {
    display: none;
    margin-top: 12px;
  }
</style>
</head>
<body>

<header>
  <div class="brand">
    <div>
      <div class="brand-title">WiFi-HiFi</div>
      <div class="brand-sub">ESP32-C3 / UDA1334A Receiver</div>
    </div>
  </div>
  <div id="stream-badge" class="badge badge-idle">IDLE</div>
</header>

<div class="nav-tabs">
  <button class="tab-btn active" onclick="switchTab('now-playing')">Now Playing</button>
  <button class="tab-btn" onclick="switchTab('audio')">Audio</button>
  <button class="tab-btn" onclick="switchTab('network')">Network</button>
  <button class="tab-btn" onclick="switchTab('device')">Device</button>
  <button class="tab-btn" onclick="switchTab('system')">System</button>
</div>

<!-- 1. NOW PLAYING -->
<div id="sec-now-playing" class="section active">
  <div class="card">
    <div class="card-title">Streaming Status</div>
    <div class="grid-2">
      <div>
        <div class="stat-label">Receiver Status</div>
        <div class="stat-val" id="st-status">Standby</div>
      </div>
      <div>
        <div class="stat-label">Connected Sender</div>
        <div class="stat-val" id="st-sender">None</div>
      </div>
      <div>
        <div class="stat-label">Audio Format</div>
        <div class="stat-val" id="st-format">PCM 16-bit Stereo</div>
      </div>
      <div>
        <div class="stat-label">Sample Rate</div>
        <div class="stat-val" id="st-rate">44,100 Hz</div>
      </div>
      <div>
        <div class="stat-label">Latency Mode</div>
        <div class="stat-val" id="st-latency">BALANCED</div>
      </div>
      <div>
        <div class="stat-label">Buffer Fill</div>
        <div class="stat-val" id="st-buf-pct">0%</div>
      </div>
    </div>
    <div style="margin-top: 14px;">
      <div class="stat-label">Jitter Buffer Level (<span id="st-buf-bytes">0</span> / 32,768 bytes)</div>
      <div class="bar-container">
        <div id="buf-bar" class="bar-fill"></div>
      </div>
    </div>
  </div>
</div>

<!-- 2. AUDIO -->
<div id="sec-audio" class="section">
  <div class="card">
    <div class="card-title">Volume & Output</div>
    <div class="slider-row">
      <span class="stat-label" style="min-width: 60px;">Volume</span>
      <input type="range" id="vol-slider" min="0" max="100" value="85" oninput="onVolInput(this.value)" onchange="setVolume(this.value)">
      <span id="vol-display" style="min-width: 45px; font-weight: 700;">85%</span>
      <button class="btn" id="mute-btn" onclick="toggleMute()">Mute</button>
    </div>
    <div class="grid-2" style="margin-top: 16px;">
      <div>
        <div class="stat-label">I2S Interface</div>
        <div class="stat-val" style="color: var(--success);">Active (GPIO 4, 5, 6)</div>
      </div>
      <div>
        <div class="stat-label">Hardware DAC</div>
        <div class="stat-val">NXP UDA1334A Stereo</div>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="card-title">Buffer Latency Profile</div>
    <div style="display: flex; gap: 8px;">
      <button class="btn" id="btn-lat-low" onclick="setLatency('LOW_LATENCY')">Low Latency (35ms)</button>
      <button class="btn" id="btn-lat-bal" onclick="setLatency('BALANCED')">Balanced (100ms)</button>
      <button class="btn" id="btn-lat-sta" onclick="setLatency('STABLE')">Stable (160ms)</button>
    </div>
  </div>
</div>

<!-- 3. NETWORK -->
<div id="sec-network" class="section">
  <div class="card">
    <div class="card-title">Wi-Fi & Streaming Telemetry</div>
    <div class="grid-2">
      <div>
        <div class="stat-label">SSID</div>
        <div class="stat-val" id="net-ssid">Scanning...</div>
      </div>
      <div>
        <div class="stat-label">Signal Strength (RSSI)</div>
        <div class="stat-val" id="net-rssi">- dBm</div>
      </div>
      <div>
        <div class="stat-label">IP Address</div>
        <div class="stat-val" id="net-ip">-</div>
      </div>
      <div>
        <div class="stat-label">mDNS Hostname</div>
        <div class="stat-val" id="net-mdns">wifi-hifi.local</div>
      </div>
      <div>
        <div class="stat-label">Packets Received</div>
        <div class="stat-val" id="net-rx">0</div>
      </div>
      <div>
        <div class="stat-label">Packets Lost</div>
        <div class="stat-val" id="net-lost">0 (0.0%)</div>
      </div>
      <div>
        <div class="stat-label">Out of Order / Duplicates</div>
        <div class="stat-val" id="net-ooo">0 / 0</div>
      </div>
      <div>
        <div class="stat-label">Underruns / Overruns</div>
        <div class="stat-val" id="net-underrun">0 / 0</div>
      </div>
    </div>
  </div>
</div>

<!-- 4. DEVICE -->
<div id="sec-device" class="section">
  <div class="card">
    <div class="card-title">Hardware Information</div>
    <div class="grid-2">
      <div>
        <div class="stat-label">Target MCU</div>
        <div class="stat-val">ESP32-C3 RISC-V</div>
      </div>
      <div>
        <div class="stat-label">Firmware Version</div>
        <div class="stat-val" id="dev-fw">1.0.0</div>
      </div>
      <div>
        <div class="stat-label">Free Heap</div>
        <div class="stat-val" id="dev-heap">- KB</div>
      </div>
      <div>
        <div class="stat-label">System Uptime</div>
        <div class="stat-val" id="dev-uptime">-</div>
      </div>
      <div>
        <div class="stat-label">MAC Address</div>
        <div class="stat-val" id="dev-mac">-</div>
      </div>
    </div>
  </div>
</div>

<!-- 5. SYSTEM -->
<div id="sec-system" class="section">
  <div class="card">
    <div class="card-title">Wi-Fi Configuration</div>
    <div class="form-group">
      <label>Network Name (SSID)</label>
      <input type="text" id="cfg-ssid" placeholder="Enter Wi-Fi SSID">
    </div>
    <div class="form-group">
      <label>Password</label>
      <input type="password" id="cfg-pass" placeholder="Enter Wi-Fi Password">
    </div>
    <button class="btn btn-gold" onclick="saveWifi()">Save & Connect</button>
  </div>

  <div class="card">
    <div class="card-title">Over-the-Air Firmware Update (OTA)</div>
    <p style="font-size: 13px; color: var(--text-muted); margin-bottom: 12px;">
      Upload a compiled <code>firmware.bin</code> binary. The device will validate and boot the new partition.
    </p>
    <input type="file" id="ota-file" accept=".bin" style="font-size: 13px; margin-bottom: 12px;">
    <br>
    <button class="btn btn-gold" onclick="uploadOta()">Flash Firmware</button>
    <div id="ota-progress">
      <div class="stat-label" id="ota-status-txt">Uploading: 0%</div>
      <div class="bar-container"><div id="ota-bar" class="bar-fill"></div></div>
    </div>
  </div>

  <div class="card">
    <div class="card-title">System Actions</div>
    <div style="display: flex; gap: 10px;">
      <button class="btn" onclick="rebootSystem()">Restart Device</button>
      <button class="btn btn-danger" onclick="resetSystem()">Factory Reset Wi-Fi</button>
    </div>
  </div>
</div>

<script>
  let isMuted = false;

  function switchTab(id) {
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.section').forEach(s => s.classList.remove('active'));
    document.getElementById('sec-' + id).classList.add('active');
    event.target.classList.add('active');
  }

  function onVolInput(val) {
    document.getElementById('vol-display').innerText = val + '%';
  }

  function setVolume(val) {
    fetch('/api/audio/volume', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({volume: parseInt(val)})
    });
  }

  function toggleMute() {
    isMuted = !isMuted;
    fetch('/api/audio/mute', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({mute: isMuted})
    }).then(() => updateMuteUi());
  }

  function updateMuteUi() {
    const btn = document.getElementById('mute-btn');
    btn.innerText = isMuted ? 'Unmute' : 'Mute';
    btn.style.color = isMuted ? 'var(--danger)' : 'var(--text-main)';
  }

  function setLatency(mode) {
    fetch('/api/audio/latency', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({mode: mode})
    });
  }

  function saveWifi() {
    const ssid = document.getElementById('cfg-ssid').value;
    const pass = document.getElementById('cfg-pass').value;
    if (!ssid) { alert('SSID cannot be empty'); return; }
    fetch('/api/wifi/config', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({ssid: ssid, password: pass})
    }).then(() => alert('Saved! The device is connecting to ' + ssid));
  }

  function rebootSystem() {
    if (confirm('Restart WiFi-HiFi Receiver?')) {
      fetch('/api/system/reboot', {method: 'POST'}).then(() => alert('Rebooting...'));
    }
  }

  function resetSystem() {
    if (confirm('Clear saved Wi-Fi and return to AP configuration mode?')) {
      fetch('/api/system/reset', {method: 'POST'}).then(() => alert('Reset! SoftAP starting...'));
    }
  }

  function uploadOta() {
    const fileInput = document.getElementById('ota-file');
    if (!fileInput.files.length) { alert('Please select a firmware.bin file first'); return; }
    const file = fileInput.files[0];
    const formData = new FormData();
    formData.append('firmware', file);

    const progDiv = document.getElementById('ota-progress');
    const bar = document.getElementById('ota-bar');
    const txt = document.getElementById('ota-status-txt');
    progDiv.style.display = 'block';

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/ota', true);

    xhr.upload.onprogress = function(e) {
      if (e.lengthComputable) {
        const pct = Math.round((e.loaded / e.total) * 100);
        bar.style.width = pct + '%';
        txt.innerText = 'Uploading: ' + pct + '%';
      }
    };

    xhr.onload = function() {
      if (xhr.status === 200) {
        txt.innerText = 'Upload successful! Device rebooting into new firmware...';
        bar.style.background = 'var(--success)';
      } else {
        txt.innerText = 'OTA failed: ' + xhr.responseText;
        bar.style.background = 'var(--danger)';
      }
    };

    xhr.send(formData);
  }

  function pollStatus() {
    fetch('/api/status')
      .then(r => r.json())
      .then(data => {
        // Stream
        const badge = document.getElementById('stream-badge');
        if (data.streaming) {
          badge.className = 'badge badge-live';
          badge.innerText = 'STREAMING';
          document.getElementById('st-status').innerText = 'Playing';
          document.getElementById('st-status').style.color = 'var(--success)';
        } else {
          badge.className = 'badge badge-idle';
          badge.innerText = 'IDLE';
          document.getElementById('st-status').innerText = 'Standby';
          document.getElementById('st-status').style.color = 'var(--text-main)';
        }

        document.getElementById('st-sender').innerText = data.sender || 'None';
        document.getElementById('st-latency').innerText = data.latency_mode;
        document.getElementById('st-buf-pct').innerText = data.buffer_fill_pct.toFixed(1) + '%';
        document.getElementById('st-buf-bytes').innerText = data.buffer_bytes.toLocaleString();
        document.getElementById('buf-bar').style.width = data.buffer_fill_pct + '%';

        // Volume
        document.getElementById('vol-slider').value = data.volume;
        document.getElementById('vol-display').innerText = data.volume + '%';
        isMuted = data.muted;
        updateMuteUi();

        // Network
        document.getElementById('net-ssid').innerText = data.ssid || '-';
        document.getElementById('net-rssi').innerText = data.rssi + ' dBm';
        document.getElementById('net-ip').innerText = data.ip || '-';
        document.getElementById('net-rx').innerText = data.packets_received.toLocaleString();
        document.getElementById('net-lost').innerText = data.packets_lost.toLocaleString() + ' (' + data.packet_loss_pct.toFixed(2) + '%)';
        document.getElementById('net-ooo').innerText = data.packets_out_of_order + ' / ' + data.packets_duplicate;
        document.getElementById('net-underrun').innerText = data.buffer_underruns + ' / ' + data.buffer_overruns;

        // Device
        document.getElementById('dev-heap').innerText = Math.round(data.free_heap / 1024) + ' KB';
        document.getElementById('dev-uptime').innerText = Math.round(data.uptime_s) + ' seconds';
        document.getElementById('dev-mac').innerText = data.mac || '-';
      })
      .catch(() => {});
  }

  setInterval(pollStatus, 1000);
  pollStatus();
</script>
</body>
</html>
)rawliteral";
