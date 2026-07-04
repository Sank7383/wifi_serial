#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <Arduino.h>

// Single-page dashboard: Monitor / Wi-Fi / ESP-NOW / Admin tabs.
// Vanilla JS, no external CDN dependencies (device may only offer its own AP).
const char INDEX_HTML[] PROGMEM = R"HTMLPAGE(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP WiFi-Serial Bridge</title>
<style>
  :root{
    --bg:#f4f6f9; --card:#ffffff; --border:#e2e6ec; --text:#1c2430; --muted:#6b7688;
    --primary:#2f6fed; --primary-dark:#1f52c7; --ok:#1aa06d; --err:#d64545; --warn:#e0912b;
    --mono-bg:#0d1117; --mono-text:#c9d1d9;
  }
  *{box-sizing:border-box}
  body{margin:0;font-family:-apple-system,Segoe UI,Roboto,Helvetica,Arial,sans-serif;background:var(--bg);color:var(--text)}
  header{background:var(--card);border-bottom:1px solid var(--border);padding:14px 20px;display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:10px}
  header h1{font-size:17px;margin:0;font-weight:600}
  .badge{display:inline-flex;align-items:center;gap:6px;font-size:12px;padding:4px 10px;border-radius:999px;background:#eef2f8;color:var(--muted)}
  .dot{width:8px;height:8px;border-radius:50%;background:var(--err)}
  .dot.on{background:var(--ok)}
  nav{display:flex;gap:4px;padding:10px 20px 0;background:var(--card);border-bottom:1px solid var(--border);overflow-x:auto}
  nav button{border:none;background:transparent;padding:10px 16px;font-size:13px;font-weight:600;color:var(--muted);cursor:pointer;border-bottom:2px solid transparent;white-space:nowrap}
  nav button.active{color:var(--primary);border-color:var(--primary)}
  main{max-width:960px;margin:0 auto;padding:20px}
  .tab{display:none}
  .tab.active{display:block}
  .card{background:var(--card);border:1px solid var(--border);border-radius:10px;padding:18px;margin-bottom:16px}
  .card h2{font-size:14px;margin:0 0 12px;color:var(--muted);text-transform:uppercase;letter-spacing:.04em}
  .row{display:flex;gap:12px;flex-wrap:wrap;margin-bottom:10px}
  .field{flex:1;min-width:180px}
  label{display:block;font-size:12px;color:var(--muted);margin-bottom:4px}
  input[type=text],input[type=password],input[type=number],select{
    width:100%;padding:8px 10px;border:1px solid var(--border);border-radius:6px;font-size:13px;background:#fff;color:var(--text)
  }
  .check{display:flex;align-items:center;gap:8px;font-size:13px;margin-bottom:8px}
  button.btn{background:var(--primary);color:#fff;border:none;padding:9px 16px;border-radius:6px;font-size:13px;font-weight:600;cursor:pointer}
  button.btn:hover{background:var(--primary-dark)}
  button.btn.secondary{background:#eef2f8;color:var(--text)}
  button.btn.danger{background:var(--err)}
  #monitor{background:var(--mono-bg);color:var(--mono-text);font-family:Consolas,Menlo,monospace;font-size:12.5px;height:340px;overflow-y:auto;border-radius:8px;padding:10px;white-space:pre-wrap;word-break:break-all}
  .toolbar{display:flex;gap:8px;margin:10px 0;flex-wrap:wrap;align-items:center}
  .send-row{display:flex;gap:8px}
  .send-row input{flex:1}
  .muted{color:var(--muted);font-size:12px}
  .toast{position:fixed;bottom:16px;right:16px;background:#1c2430;color:#fff;padding:10px 16px;border-radius:8px;font-size:13px;opacity:0;transition:opacity .2s;pointer-events:none}
  .toast.show{opacity:1}
  .grid2{display:grid;grid-template-columns:1fr 1fr;gap:16px}
  @media (max-width:640px){.grid2{grid-template-columns:1fr}}
  code.mac{font-family:Consolas,Menlo,monospace}
</style>
</head>
<body>

<header>
  <h1 id="deviceTitle">ESP WiFi-Serial Bridge</h1>
  <span class="badge"><span class="dot" id="wsDot"></span><span id="wsLabel">connecting…</span></span>
</header>

<nav>
  <button data-tab="monitor" class="active">Monitor</button>
  <button data-tab="wifi">Wi-Fi</button>
  <button data-tab="espnow">ESP-NOW</button>
  <button data-tab="admin">Admin</button>
  <button data-tab="programmer">Programmer</button>
</nav>

<main>

  <section id="tab-monitor" class="tab active">
    <div class="card">
      <h2>Serial Monitor</h2>
      <div id="monitor"></div>
      <div class="toolbar">
        <select id="lineEnding">
          <option value="none">No line ending</option>
          <option value="lf" selected>LF (\n)</option>
          <option value="crlf">CRLF (\r\n)</option>
          <option value="cr">CR (\r)</option>
        </select>
        <select id="monitorBaud" title="Serial baud rate">
          <option>300</option><option>1200</option><option>2400</option><option>4800</option>
          <option>9600</option><option>19200</option><option>38400</option><option>57600</option>
          <option>74880</option><option selected>115200</option><option>230400</option>
          <option>460800</option><option>921600</option><option>1000000</option>
          <option>1500000</option><option>2000000</option><option>3000000</option>
        </select>
        <label class="check"><input type="checkbox" id="hexView"> Hex view</label>
        <label class="check"><input type="checkbox" id="autoscroll" checked> Autoscroll</label>
        <label class="check"><input type="checkbox" id="timestamps"> Timestamps</label>
        <button class="btn secondary" id="clearBtn">Clear</button>
      </div>
      <div class="send-row">
        <input type="text" id="sendInput" placeholder="Type data to send over serial / ESP-NOW…">
        <button class="btn" id="sendBtn">Send</button>
      </div>
    </div>
  </section>

  <section id="tab-wifi" class="tab">
    <div class="card">
      <h2>Network Mode</h2>
      <div class="row">
        <label class="check"><input type="radio" name="wifiOpMode" value="0"> Station (connect to a router)</label>
        <label class="check"><input type="radio" name="wifiOpMode" value="1"> Access Point (device creates its own network)</label>
        <label class="check"><input type="radio" name="wifiOpMode" value="2"> Both (AP + Station simultaneously)</label>
      </div>
      <p class="muted">"Both" keeps the device's own AP reachable while it also joins your router — handy if a STA connection might drop and you still need a fallback path in.</p>
    </div>
    <div class="card">
      <h2>Station (STA) Settings</h2>
      <div class="row">
        <div class="field">
          <label>SSID</label>
          <input type="text" id="staSsid" maxlength="32">
        </div>
        <div class="field">
          <label>Password</label>
          <input type="password" id="staPass" maxlength="64">
        </div>
      </div>
      <button class="btn secondary" id="scanBtn">Scan networks</button>
      <div id="scanResults" class="muted" style="margin-top:8px"></div>
      <label class="check" style="margin-top:12px"><input type="checkbox" id="useStaticIp"> Use static IP</label>
      <div class="row" id="staticIpRow" style="display:none">
        <div class="field"><label>IP</label><input type="text" id="ip"></div>
        <div class="field"><label>Gateway</label><input type="text" id="gateway"></div>
        <div class="field"><label>Subnet</label><input type="text" id="subnet"></div>
        <div class="field"><label>DNS</label><input type="text" id="dns"></div>
      </div>
    </div>
    <div class="card">
      <h2>Access Point (AP) Settings</h2>
      <div class="row">
        <div class="field"><label>AP SSID</label><input type="text" id="apSsid" maxlength="32"></div>
        <div class="field"><label>AP Password (min 8 chars)</label><input type="password" id="apPass" maxlength="64"></div>
      </div>
    </div>
    <button class="btn" id="saveWifiBtn">Save network settings</button>
    <p class="muted">Changing network mode or STA credentials reboots the device.</p>
  </section>

  <section id="tab-espnow" class="tab">
    <div class="card">
      <h2>ESP-NOW Bridging</h2>
      <label class="check"><input type="checkbox" id="espnowEnabled"> Enable ESP-NOW</label>
      <div class="row" style="margin-top:10px">
        <label class="check"><input type="radio" name="espnowMode" value="0"> Broadcast (all nearby ESP-NOW devices)</label>
        <label class="check"><input type="radio" name="espnowMode" value="1"> Unicast (one specific device)</label>
      </div>
      <div class="row">
        <div class="field">
          <label>Peer MAC address (unicast target)</label>
          <input type="text" id="espnowPeerMac" placeholder="AA:BB:CC:DD:EE:FF" maxlength="17">
        </div>
        <div class="field">
          <label>Wi-Fi channel (used only in AP mode)</label>
          <input type="number" id="espnowChannel" min="1" max="13">
        </div>
      </div>
    </div>
    <div class="card">
      <h2>Bridge Direction</h2>
      <div class="grid2">
        <div>
          <label class="check"><input type="checkbox" id="serialToWs"> Serial → WebSocket (monitor)</label>
          <label class="check"><input type="checkbox" id="serialToEspnow"> Outgoing data → ESP-NOW</label>
        </div>
        <div>
          <label class="check"><input type="checkbox" id="espnowToSerial"> ESP-NOW → Serial</label>
          <label class="check"><input type="checkbox" id="espnowToWs"> ESP-NOW → WebSocket (monitor)</label>
        </div>
      </div>
      <p class="muted">"Outgoing data" covers both UART RX and anything typed into the Monitor tab — both are sent out over ESP-NOW when this is enabled.</p>
    </div>
    <button class="btn" id="saveEspnowBtn">Save ESP-NOW settings</button>
  </section>

  <section id="tab-admin" class="tab">
    <div class="card">
      <h2>Device</h2>
      <div class="row">
        <div class="field"><label>Device name</label><input type="text" id="deviceName" maxlength="31"></div>
        <div class="field">
          <label>Serial baud rate</label>
          <select id="baudRate">
            <option>300</option><option>1200</option><option>2400</option><option>4800</option>
            <option>9600</option><option>19200</option><option>38400</option><option>57600</option>
            <option>74880</option><option selected>115200</option><option>230400</option>
            <option>460800</option><option>921600</option><option>1000000</option>
            <option>1500000</option><option>2000000</option><option>3000000</option>
          </select>
        </div>
      </div>
      <button class="btn" id="saveDeviceBtn">Save device settings</button>
    </div>
    <div class="card">
      <h2>Admin Credentials</h2>
      <p class="muted">Protects this web dashboard (HTTP Basic Auth).</p>
      <div class="row">
        <div class="field"><label>Username</label><input type="text" id="authUser" maxlength="23"></div>
        <div class="field"><label>New password (leave blank to keep current)</label><input type="password" id="authPass" maxlength="32"></div>
      </div>
      <label class="check"><input type="checkbox" id="authDisabled"> Disable password protection (open access — anyone on the network can view and control this device)</label>
      <p class="muted" style="color:var(--err)">Only enable this on a network you fully trust. With it on, the dashboard, serial monitor and all settings are reachable by anyone who can reach the device's IP — no login required.</p>
      <button class="btn" id="saveAuthBtn">Update credentials</button>
    </div>
    <div class="card">
      <h2>Status</h2>
      <div id="statusBox" class="muted">Loading…</div>
    </div>
    <div class="card">
      <h2>Danger Zone</h2>
      <button class="btn danger" id="rebootBtn">Reboot device</button>
    </div>
  </section>

  <section id="tab-programmer" class="tab">
    <div class="card">
      <h2>ESP Programmer</h2>
      <p class="muted">Turns this unit into a dedicated WiFi-to-UART bridge for flashing a <b>third</b> ESP with esptool.py, instead of the normal serial monitor. Wire this unit's UART to the target's UART (TX&rarr;RX, RX&rarr;TX, shared GND), and two spare GPIOs to the target's GPIO0/BOOT and EN/RST pins.</p>
      <label class="check"><input type="checkbox" id="programmerEnabled"> Enable Programmer mode</label>
      <p class="muted" style="color:var(--warn)">While enabled, the Serial Monitor and ESP-NOW bridging on this device are suspended — its UART is dedicated to the target ESP.</p>
    </div>
    <div class="card">
      <h2>Pin Wiring</h2>
      <div class="row">
        <div class="field">
          <label>GPIO0 / BOOT control pin</label>
          <select id="pgmGpio0Pin"></select>
        </div>
        <div class="field">
          <label>EN / RST control pin</label>
          <select id="pgmResetPin"></select>
        </div>
      </div>
      <label class="check"><input type="checkbox" id="pgmGpio0ActiveLow" checked> GPIO0 pin is active-low (drives LOW to select bootloader)</label>
      <label class="check"><input type="checkbox" id="pgmResetActiveLow" checked> Reset pin is active-low (drives LOW to reset)</label>
    </div>
    <div class="card">
      <h2>Network</h2>
      <div class="row">
        <div class="field"><label>TCP port for esptool</label><input type="number" id="pgmTcpPort" min="1" max="65535"></div>
        <div class="field">
          <label>Flashing baud rate (fixed for the session)</label>
          <select id="pgmBaudRate">
            <option>9600</option><option>19200</option><option>38400</option><option>57600</option>
            <option selected>115200</option><option>230400</option><option>460800</option><option>921600</option>
          </select>
        </div>
      </div>
      <button class="btn" id="savePgmBtn">Save Programmer settings</button>
    </div>
    <div class="card">
      <h2>Flash Command</h2>
      <p class="muted">Run this on the PC where esptool is installed, instead of pointing it at a local COM port. The baud is fixed for the whole session — don't rely on esptool's automatic speed upgrade.</p>
      <input type="text" id="pgmCommand" readonly style="font-family:Consolas,Menlo,monospace;margin-bottom:10px">
      <button class="btn secondary" id="copyPgmCmdBtn">Copy command</button>
    </div>
    <div class="card">
      <h2>Wiring Test</h2>
      <div id="pgmStatus" class="muted">Programmer mode is off.</div>
      <div class="toolbar">
        <button class="btn secondary" id="pgmTestBootBtn">Enter bootloader (test)</button>
        <button class="btn secondary" id="pgmTestRunBtn">Return to run mode</button>
      </div>
    </div>
  </section>

</main>

<div class="toast" id="toast"></div>

<script>
const $ = (id) => document.getElementById(id);
let cfg = {};

// ---------- tabs ----------
document.querySelectorAll('nav button').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('nav button').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    btn.classList.add('active');
    $('tab-' + btn.dataset.tab).classList.add('active');
  });
});

function toast(msg) {
  const t = $('toast');
  t.textContent = msg;
  t.classList.add('show');
  setTimeout(() => t.classList.remove('show'), 2200);
}

// ---------- base64 helpers (browser-side) ----------
function bytesToB64(bytes) {
  let bin = '';
  for (const b of bytes) bin += String.fromCharCode(b);
  return btoa(bin);
}
function b64ToBytes(b64) {
  const bin = atob(b64);
  const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i);
  return out;
}

// ---------- config load/save ----------
async function loadConfig() {
  const res = await fetch('/api/config');
  cfg = await res.json();
  $('deviceTitle').textContent = cfg.deviceName || 'ESP WiFi-Serial Bridge';
  document.querySelector(`input[name=wifiOpMode][value="${cfg.wifiOpMode}"]`).checked = true;
  $('staSsid').value = cfg.staSsid || '';
  $('staPass').value = '';
  $('apSsid').value = cfg.apSsid || '';
  $('apPass').value = '';
  $('useStaticIp').checked = !!cfg.useStaticIp;
  $('ip').value = cfg.ip || ''; $('gateway').value = cfg.gateway || '';
  $('subnet').value = cfg.subnet || ''; $('dns').value = cfg.dns || '';
  $('staticIpRow').style.display = cfg.useStaticIp ? 'flex' : 'none';

  $('espnowEnabled').checked = !!cfg.espnowEnabled;
  document.querySelector(`input[name=espnowMode][value="${cfg.espnowMode}"]`).checked = true;
  $('espnowPeerMac').value = cfg.espnowPeerMac || '';
  $('espnowChannel').value = cfg.espnowChannel || 1;
  $('serialToWs').checked = !!cfg.serialToWs;
  $('serialToEspnow').checked = !!cfg.serialToEspnow;
  $('espnowToSerial').checked = !!cfg.espnowToSerial;
  $('espnowToWs').checked = !!cfg.espnowToWs;

  $('deviceName').value = cfg.deviceName || '';
  $('baudRate').value = cfg.baudRate || 115200;
  $('monitorBaud').value = cfg.baudRate || 115200;
  $('authUser').value = cfg.authUser || 'admin';
  $('authDisabled').checked = !!cfg.authDisabled;

  $('programmerEnabled').checked = !!cfg.programmerEnabled;
  $('pgmGpio0Pin').value = cfg.pgmGpio0Pin ?? 14;
  $('pgmResetPin').value = cfg.pgmResetPin ?? 12;
  $('pgmGpio0ActiveLow').checked = cfg.pgmGpio0ActiveLow !== false;
  $('pgmResetActiveLow').checked = cfg.pgmResetActiveLow !== false;
  $('pgmTcpPort').value = cfg.pgmTcpPort || 3333;
  $('pgmBaudRate').value = cfg.pgmBaudRate || 115200;
  updatePgmCommand();
}

async function saveConfig(patch) {
  const merged = Object.assign({}, cfg, patch);
  const res = await fetch('/api/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(merged)
  });
  if (!res.ok) { toast('Save failed: ' + (await res.text())); return false; }
  const body = await res.json();
  cfg = merged;
  toast(body.rebootRequired ? 'Saved — rebooting…' : 'Saved');
  return true;
}

$('useStaticIp').addEventListener('change', e => {
  $('staticIpRow').style.display = e.target.checked ? 'flex' : 'none';
});

$('saveWifiBtn').addEventListener('click', async () => {
  const patch = {
    wifiOpMode: Number(document.querySelector('input[name=wifiOpMode]:checked').value),
    staSsid: $('staSsid').value.trim(),
    apSsid: $('apSsid').value.trim(),
    useStaticIp: $('useStaticIp').checked,
    ip: $('ip').value.trim(), gateway: $('gateway').value.trim(),
    subnet: $('subnet').value.trim(), dns: $('dns').value.trim(),
  };
  if ($('staPass').value) patch.staPass = $('staPass').value;
  if ($('apPass').value) patch.apPass = $('apPass').value;
  await saveConfig(patch);
});

$('saveEspnowBtn').addEventListener('click', async () => {
  const mac = $('espnowPeerMac').value.trim().toUpperCase();
  if (mac && !/^([0-9A-F]{2}:){5}[0-9A-F]{2}$/.test(mac)) {
    toast('Invalid MAC address format'); return;
  }
  await saveConfig({
    espnowEnabled: $('espnowEnabled').checked,
    espnowMode: Number(document.querySelector('input[name=espnowMode]:checked').value),
    espnowPeerMac: mac,
    espnowChannel: Number($('espnowChannel').value) || 1,
    serialToWs: $('serialToWs').checked,
    serialToEspnow: $('serialToEspnow').checked,
    espnowToSerial: $('espnowToSerial').checked,
    espnowToWs: $('espnowToWs').checked,
  });
});

$('saveDeviceBtn').addEventListener('click', async () => {
  await saveConfig({
    deviceName: $('deviceName').value.trim() || 'ESP-WiFiSerial',
    baudRate: Number($('baudRate').value),
  });
});

$('saveAuthBtn').addEventListener('click', async () => {
  const disabling = $('authDisabled').checked;
  if (disabling && !confirm('This makes the dashboard and serial monitor open to anyone on the network, with no login. Continue?')) {
    $('authDisabled').checked = false;
    return;
  }
  const patch = {
    authUser: $('authUser').value.trim() || 'admin',
    authDisabled: disabling,
  };
  if ($('authPass').value) patch.authPass = $('authPass').value;
  const ok = await saveConfig(patch);
  if (ok) toast(disabling ? 'Open access enabled — no login required' : 'Credentials updated — you may be asked to sign in again');
});

$('rebootBtn').addEventListener('click', async () => {
  if (!confirm('Reboot the device now?')) return;
  await fetch('/api/reboot', { method: 'POST' });
  toast('Rebooting…');
});

// ---------- ESP Programmer ----------
const PGM_PINS = [
  [0, 'GPIO0 (D3)'], [2, 'GPIO2 (D4)'], [4, 'GPIO4 (D2)'], [5, 'GPIO5 (D1)'],
  [12, 'GPIO12 (D6)'], [13, 'GPIO13 (D7)'], [14, 'GPIO14 (D5)'], [15, 'GPIO15 (D8)'], [16, 'GPIO16 (D0)'],
];
for (const id of ['pgmGpio0Pin', 'pgmResetPin']) {
  $(id).innerHTML = PGM_PINS.map(([v, label]) => `<option value="${v}">${label}</option>`).join('');
}

function updatePgmCommand() {
  const port = $('pgmTcpPort').value || 3333;
  const baud = $('pgmBaudRate').value || 115200;
  $('pgmCommand').value =
    `esptool.py --port socket://${location.hostname}:${port} --baud ${baud} --before no_reset --after no_reset write_flash 0x0 firmware.bin`;
}
$('pgmTcpPort').addEventListener('input', updatePgmCommand);
$('pgmBaudRate').addEventListener('change', updatePgmCommand);

$('savePgmBtn').addEventListener('click', async () => {
  const enabling = $('programmerEnabled').checked;
  const gpio0Pin = Number($('pgmGpio0Pin').value);
  const resetPin = Number($('pgmResetPin').value);
  if (gpio0Pin === resetPin) { toast('GPIO0 and Reset pins must be different'); return; }
  if (enabling && !confirm('This suspends the Serial Monitor and ESP-NOW bridging on this device and dedicates its UART to flashing a target ESP. Continue?')) {
    return;
  }
  await saveConfig({
    programmerEnabled: enabling,
    pgmGpio0Pin: gpio0Pin,
    pgmResetPin: resetPin,
    pgmGpio0ActiveLow: $('pgmGpio0ActiveLow').checked,
    pgmResetActiveLow: $('pgmResetActiveLow').checked,
    pgmTcpPort: Number($('pgmTcpPort').value) || 3333,
    pgmBaudRate: Number($('pgmBaudRate').value) || 115200,
  });
  updatePgmCommand();
});

$('copyPgmCmdBtn').addEventListener('click', () => {
  const el = $('pgmCommand');
  el.removeAttribute('readonly');
  el.select();
  try {
    document.execCommand('copy');
    toast('Command copied');
  } catch (e) {
    toast('Copy failed — select the text manually');
  }
  el.setAttribute('readonly', 'readonly');
});

async function pgmTest(mode) {
  const res = await fetch('/api/pgm/test', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ mode })
  });
  if (!res.ok) { toast('Failed: ' + (await res.text())); return; }
  toast(mode === 'bootloader' ? 'Target pulsed into bootloader mode' : 'Target pulsed into run mode');
}
$('pgmTestBootBtn').addEventListener('click', () => pgmTest('bootloader'));
$('pgmTestRunBtn').addEventListener('click', () => pgmTest('run'));

$('scanBtn').addEventListener('click', async () => {
  $('scanResults').textContent = 'Scanning…';
  const res = await fetch('/api/scan');
  const nets = await res.json();
  $('scanResults').innerHTML = nets.map(n =>
    `<div style="cursor:pointer;padding:2px 0" data-ssid="${n.ssid.replace(/"/g,'&quot;')}">${n.ssid} (${n.rssi} dBm)${n.secure ? ' 🔒' : ''}</div>`
  ).join('') || 'No networks found';
  $('scanResults').querySelectorAll('[data-ssid]').forEach(el => {
    el.addEventListener('click', () => { $('staSsid').value = el.dataset.ssid; });
  });
});

async function refreshStatus() {
  try {
    const res = await fetch('/api/status');
    const s = await res.json();
    const ipLine = s.apIp ? `STA IP: ${s.ip} &nbsp;|&nbsp; AP IP: ${s.apIp}` : `IP: ${s.ip}`;
    $('statusBox').innerHTML = `
      Firmware: ${s.fwVersion} &nbsp;|&nbsp; Mode: ${s.mode} &nbsp;|&nbsp; ${ipLine}<br>
      Wi-Fi MAC: <code class="mac">${s.mac}</code> &nbsp;|&nbsp; RSSI: ${s.rssi} dBm<br>
      Free heap: ${s.freeHeap} bytes &nbsp;|&nbsp; Uptime: ${Math.floor(s.uptimeMs/1000)}s`;
    $('pgmStatus').textContent = !s.pgmEnabled
      ? 'Programmer mode is off.'
      : (s.pgmClientConnected ? 'Programmer mode: esptool connected — flashing in progress' : 'Programmer mode: waiting for esptool to connect…');
  } catch (e) { /* ignore transient errors */ }
}
setInterval(refreshStatus, 5000);

// ---------- monitor + websocket ----------
const LINE_ENDINGS = { none: '', lf: '\n', crlf: '\r\n', cr: '\r' };
const monitorEl = $('monitor');

// Tracks whether the next appended character starts a fresh line, so
// timestamps (when enabled) are inserted once per line rather than once
// per received chunk.
let atLineStart = true;
function timestampPrefix() {
  const d = new Date();
  const pad = (n, w = 2) => String(n).padStart(w, '0');
  return `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}.${pad(d.getMilliseconds(), 3)} -> `;
}
function splitKeepingNewlines(text) {
  const parts = [];
  let start = 0;
  for (let i = 0; i < text.length; i++) {
    if (text[i] === '\n') { parts.push(text.slice(start, i + 1)); start = i + 1; }
  }
  if (start < text.length) parts.push(text.slice(start));
  return parts;
}
function appendMonitor(text, cls) {
  if (!text) return;
  const atBottom = monitorEl.scrollTop + monitorEl.clientHeight >= monitorEl.scrollHeight - 4;
  let out = text;
  if ($('timestamps').checked) {
    out = '';
    for (const line of splitKeepingNewlines(text)) {
      if (atLineStart) out += timestampPrefix();
      out += line;
      atLineStart = line.endsWith('\n');
    }
  } else {
    atLineStart = text.endsWith('\n');
  }
  const span = document.createElement('span');
  if (cls) span.style.color = cls;
  span.textContent = out;
  monitorEl.appendChild(span);
  if ($('autoscroll').checked && atBottom) monitorEl.scrollTop = monitorEl.scrollHeight;
}
function bytesToHex(bytes) {
  return Array.from(bytes).map(b => b.toString(16).padStart(2, '0')).join(' ') + ' ';
}
$('clearBtn').addEventListener('click', () => { monitorEl.textContent = ''; atLineStart = true; });

$('monitorBaud').addEventListener('change', async () => {
  const baudRate = Number($('monitorBaud').value);
  const prev = cfg.baudRate;
  try {
    const res = await fetch('/api/baud', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ baudRate })
    });
    if (!res.ok) { toast('Baud rate change failed: ' + (await res.text())); $('monitorBaud').value = prev; return; }
    cfg.baudRate = baudRate;
    $('baudRate').value = baudRate;
    toast('Baud rate set to ' + baudRate);
  } catch (e) {
    toast('Baud rate change failed');
    $('monitorBaud').value = prev;
  }
});

let ws;
async function connectWs() {
  const tokRes = await fetch('/api/wstoken');
  if (!tokRes.ok) { $('wsLabel').textContent = 'auth failed'; return; }
  const { token } = await tokRes.json();
  ws = new WebSocket(`ws://${location.hostname}:81/?token=${token}`);
  ws.onopen = () => { $('wsDot').classList.add('on'); $('wsLabel').textContent = 'connected'; };
  ws.onclose = () => {
    $('wsDot').classList.remove('on'); $('wsLabel').textContent = 'reconnecting…';
    setTimeout(connectWs, 2000);
  };
  ws.onerror = () => ws.close();
  ws.onmessage = (evt) => {
    let msg;
    try { msg = JSON.parse(evt.data); } catch { return; }
    if (msg.type === 'serial') {
      const bytes = b64ToBytes(msg.data);
      appendMonitor($('hexView').checked ? bytesToHex(bytes) : new TextDecoder().decode(bytes));
    } else if (msg.type === 'espnow') {
      const bytes = b64ToBytes(msg.data);
      const text = $('hexView').checked ? bytesToHex(bytes) : new TextDecoder().decode(bytes);
      appendMonitor(`[ESPNOW ${msg.mac}] ${text}\n`, '#58a6ff');
    } else if (msg.type === 'log') {
      appendMonitor(`[${msg.level}] ${msg.msg}\n`, msg.level === 'error' ? '#f85149' : '#8b949e');
    }
  };
}
connectWs();

// Must stay <= WS_RX_MAX_PAYLOAD in websocket_handler.h — the device decodes
// each WS message into a fixed-size buffer of that capacity.
const WS_SEND_CHUNK_BYTES = 512;

function sendData() {
  const val = $('sendInput').value;
  if (!val && val !== '') return;
  const ending = LINE_ENDINGS[$('lineEnding').value] || '';
  const text = val + ending;
  if (!ws || ws.readyState !== WebSocket.OPEN) { toast('Not connected'); return; }
  const bytes = new TextEncoder().encode(text);
  // Long input is split across multiple WS messages so it can never
  // silently exceed the device's per-message decode buffer.
  for (let i = 0; i < bytes.length; i += WS_SEND_CHUNK_BYTES) {
    ws.send(JSON.stringify({ type: 'serial', data: bytesToB64(bytes.subarray(i, i + WS_SEND_CHUNK_BYTES)) }));
  }
  appendMonitor(text);
  $('sendInput').value = '';
}
$('sendBtn').addEventListener('click', sendData);
$('sendInput').addEventListener('keydown', e => { if (e.key === 'Enter') sendData(); });

loadConfig();
refreshStatus();
</script>
</body>
</html>
)HTMLPAGE";

#endif // WEBPAGE_H
