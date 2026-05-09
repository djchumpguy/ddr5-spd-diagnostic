#include "WebUi.h"
#include "AppConfig.h"
#include "AppState.h"
#include "BoardControl.h"
#include "Cli.h"
#include "Log.h"
#include "RoleDetect.h"
#include "SpdHub.h"
#include "SpdOps.h"

#include <WiFi.h>
#include <WebServer.h>

static WebServer server(WEB_PORT);

// ===================== WEB HELPERS =====================
static String jsonBool(bool v) { return v ? "true" : "false"; }

static String jsonEscape(const char* s) {
  String out;
  if (!s) return out;
  while (*s) {
    char c = *s++;
    if (c == '"' || c == '\\') out += '\\';
    if ((uint8_t)c >= 0x20) out += c;
  }
  return out;
}

static void webSendJson(int code, const String& body) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(code, "application/json", body);
}

static void webSendText(int code, const String& body) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(code, "text/plain", body);
}

static void webSendBinFromBuf(const uint8_t* buf, size_t len, const char* filename) {
  server.sendHeader("Content-Type", "application/octet-stream");
  server.sendHeader("Content-Disposition", String("attachment; filename=\"") + filename + "\"");
  server.setContentLength(len);
  server.send(200, "application/octet-stream", "");
  WiFiClient c = server.client();
  c.write(buf, len);
}

// ===================== WEB: API =====================
static void handleWebStatus() {
  bool mrOk = spdRefreshMR11(DEFAULT_SPD_ADDR);

  String j = "{";
  j += "\"ok\":true,";
  j += "\"pwr_good\":" + jsonBool(pwrGoodReady()) + ",";
  j += "\"pwr_en_released\":" + jsonBool(readPwrEnLevel()) + ",";
  j += "\"dimm_power_on\":" + jsonBool(isDimmPowerOn()) + ",";
  j += "\"hsa_low\":" + jsonBool(isHsaLow()) + ",";
  j += "\"scan_ok\":" + jsonBool(gApp.scanOK) + ",";
  j += "\"read_ok\":" + jsonBool(gApp.readOK) + ",";
  j += "\"dump_ok\":" + jsonBool(gApp.dumpOK) + ",";
  j += "\"good_valid\":" + jsonBool(gApp.goodSpdValid) + ",";
  j += "\"good_crc\":\"0x" + String(gApp.goodCrc, HEX) + "\",";
  j += "\"pmic_ref_valid\":" + jsonBool(gApp.pmicRefValid) + ",";
  j += "\"pmic_ref_crc\":\"0x" + String(gApp.pmicRefCrc, HEX) + "\",";
  j += "\"last_dump_valid\":" + jsonBool(gApp.lastDumpValid) + ",";
  j += "\"mr11_ok\":" + jsonBool(mrOk) + ",";
  j += "\"mr11\":\"0x" + String(mrOk ? gApp.mr11 : 0, HEX) + "\",";
  j += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\",";
  j += "\"scan_count\":" + String((unsigned)gApp.lastScanCount) + ",";
  j += "\"scan_addrs\":[";
  for (size_t i = 0; i < gApp.lastScanCount; i++) {
    if (i) j += ",";
    j += String(gApp.lastScanAddrs[i]);
  }
  j += "],";
  j += "\"roles\":[";
  for (size_t i = 0; i < gApp.roleRecordCount; i++) {
    DeviceRoleRecord& rec = gApp.roleRecords[i];
    if (i) j += ",";
    j += "{";
    j += "\"addr\":" + String(rec.address) + ",";
    j += "\"role\":\"" + String(roleName(rec.role)) + "\",";
    j += "\"label\":\"" + String(roleUiName(rec.role)) + "\",";
    j += "\"confidence\":\"" + String(confidenceName(rec.confidence)) + "\",";
    j += "\"reason\":\"" + jsonEscape(rec.reason) + "\"";
    j += "}";
  }
  j += "]";
  j += "}";

  webSendJson(200, j);
}

static void handleWebLog() {
  webSendText(200, getLogString());
}

static void handleWebRun() {
  if (!server.hasArg("line")) {
    webSendText(400, "ERR: missing 'line'");
    return;
  }

  String line = server.arg("line");
  execCommandLine(line);
  webSendText(200, "OK");
}

static void handleWebClearLog() {
  clearLog();
  webSendText(200, "OK: log cleared");
}

// ===================== WEB: DOWNLOADS =====================
static void handleDownloadCurrent() {
  if (!pwrGoodReady()) {
    webSendText(400, "PWR_GOOD LOW: check wiring/readiness before trusting SPD/PMIC reads");
    return;
  }

  if (!gApp.lastDumpValid) {
    bool ok = dumpToBuffer(DEFAULT_SPD_ADDR);
    if (!ok) {
      webSendText(500, "dump failed");
      return;
    }
    gApp.dumpOK = true;
    gApp.readOK = true;
  }

  webSendBinFromBuf(gApp.lastDump, SPD_NVM_SIZE, "current_spd.bin");
}

static void handleDownloadGood() {
  if (!gApp.goodSpdValid) {
    webSendText(404, "no known-good SPD reference stored");
    return;
  }

  webSendBinFromBuf(gApp.goodSpd, SPD_NVM_SIZE, "good_spd.bin");
}

// ===================== WEB UI (stored in flash) =====================
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1" />
<title>ESP32 SPD Tool</title>
<style>
  :root{
    --bg:#f5f7fb;
    --card:#ffffff;
    --line:#d8dee8;
    --text:#17212b;
    --muted:#5b6672;
    --term:#0b0f14;
    --termText:#d2e7ff;
    --green:#1f9d55;
    --red:#d64545;
    --blue:#2d6cdf;
    --amber:#b7791f;
  }

  *{box-sizing:border-box}
  body{
    font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial;
    margin:12px;
    background:var(--bg);
    color:var(--text);
  }

  h2{margin:0 0 10px 0}
  h3{margin:0 0 10px 0}

  .section{margin-top:12px}
  .card{
    border:1px solid var(--line);
    border-radius:14px;
    padding:12px;
    background:var(--card);
    box-shadow:0 1px 3px rgba(0,0,0,.04);
  }

  .grid{
    display:grid;
    grid-template-columns:1fr;
    gap:12px;
  }

  @media (min-width: 1000px){
    .grid.two{grid-template-columns:1fr 1fr;}
    .grid.three{grid-template-columns:1fr 1fr 1fr;}
  }

  .row{
    display:flex;
    flex-wrap:wrap;
    gap:8px;
    align-items:center;
  }

  .toolbar{
    display:flex;
    flex-wrap:wrap;
    gap:8px;
    align-items:center;
    justify-content:space-between;
    margin-bottom:10px;
  }

  .muted{color:var(--muted);font-size:13px}

  button{
    padding:10px 12px;
    border-radius:10px;
    border:1px solid #b8c2cf;
    background:#f8fafc;
    cursor:pointer;
    color:var(--text);
    font-weight:600;
  }

  button:hover{background:#eef3f8}
  button:active{transform:translateY(1px)}
  a button{cursor:pointer}

  .btnBlue{background:#edf4ff;border-color:#bfd4ff}
  .btnAmber{background:#fff6e7;border-color:#f0d28f}
  .btnRed{background:#fff1f1;border-color:#f1b2b2}
  .btnGreen{background:#ecfff4;border-color:#b7e7c8}

  input[type="text"]{
    padding:10px;
    border-radius:10px;
    border:1px solid #b8c2cf;
    min-width:240px;
    flex:1;
    background:#fff;
  }

  pre{
    background:var(--term);
    color:var(--termText);
    padding:12px;
    border-radius:12px;
    overflow:auto;
    font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, "Liberation Mono", monospace;
    line-height:1.25;
    font-size:13px;
    margin:0;
    white-space:pre-wrap;
    word-break:break-word;
  }

  #term{height:62vh}

  .pills{
    display:flex;
    flex-wrap:wrap;
    gap:8px;
    margin-top:8px;
  }

  .pill{
    border-radius:999px;
    padding:6px 10px;
    font-size:12px;
    font-weight:700;
    border:1px solid transparent;
  }

  .pill.ok{
    background:#ebfff3;
    color:#11693d;
    border-color:#b9e8cb;
  }

  .pill.bad{
    background:#fff0f0;
    color:#9d2626;
    border-color:#efb6b6;
  }

  .pill.neutral{
    background:#eef3f8;
    color:#415261;
    border-color:#d2dce7;
  }

  .toggleWrap{
    display:flex;
    flex-direction:column;
    gap:12px;
  }

  .toggleRow{
    display:flex;
    align-items:center;
    justify-content:space-between;
    gap:12px;
    padding:10px 12px;
    border:1px solid var(--line);
    border-radius:12px;
    background:#fbfdff;
  }

  .toggleLeft{
    display:flex;
    flex-direction:column;
    gap:4px;
  }

  .toggleTitle{
    font-weight:700;
  }

  .toggleMeta{
    font-size:12px;
    color:var(--muted);
  }

  .toggleRight{
    display:flex;
    align-items:center;
    gap:10px;
  }

  .stateBadge{
    min-width:44px;
    text-align:center;
    border-radius:999px;
    padding:5px 10px;
    font-size:12px;
    font-weight:800;
    color:#fff;
  }

  .stateOn{background:var(--green)}
  .stateOff{background:var(--red)}

  .switch{
    position:relative;
    display:inline-block;
    width:62px;
    height:34px;
    flex:0 0 auto;
  }

  .switch input{
    opacity:0;
    width:0;
    height:0;
  }

  .slider{
    position:absolute;
    cursor:pointer;
    inset:0;
    background:var(--red);
    transition:.2s;
    border-radius:999px;
    box-shadow: inset 0 0 0 1px rgba(0,0,0,.08);
  }

  .slider:before{
    content:"";
    position:absolute;
    height:26px;
    width:26px;
    left:4px;
    top:4px;
    background:white;
    transition:.2s;
    border-radius:50%;
    box-shadow:0 1px 3px rgba(0,0,0,.25);
  }

  input:checked + .slider{
    background:var(--green);
  }

  input:checked + .slider:before{
    transform:translateX(28px);
  }

  .deviceList{
    display:grid;
    grid-template-columns:1fr;
    gap:10px;
    margin-top:10px;
  }

  @media (min-width: 1100px){
    .deviceList{
      grid-template-columns:1fr 1fr;
    }
  }

  .deviceCard{
    border:1px solid var(--line);
    border-radius:14px;
    padding:12px;
    background:#fcfdff;
  }

  .deviceHeader{
    display:flex;
    align-items:center;
    justify-content:space-between;
    gap:12px;
    margin-bottom:10px;
  }

  .deviceAddr{
    font-weight:800;
    font-size:18px;
  }

  .deviceActions{
    display:flex;
    flex-wrap:wrap;
    gap:8px;
  }

  .subtle{
    font-size:12px;
    color:var(--muted);
    margin-top:6px;
  }
</style>
</head>
<body>
<h2>ESP32 SPD Tool</h2>

<div class="card">
  <div class="toolbar">
    <div class="muted">Live state</div>
    <div class="row">
      <button onclick="refreshStatus()">Refresh</button>
      <button onclick="refreshLog()">Refresh Log</button>
      <button onclick="clearLog()">Clear Log</button>
    </div>
  </div>
  <div class="pills">
    <span id="pillPwrGood" class="pill neutral">PWR_GOOD: --</span>
    <span id="pillPwrEn" class="pill neutral">PWR_EN / VR: --</span>
    <span id="pillDimm" class="pill neutral">VIN_BULK: --</span>
    <span id="pillHsa" class="pill neutral">HSA: --</span>
    <span id="pillGood" class="pill neutral">Known-good SPD: --</span>
    <span id="pillPmicRef" class="pill neutral">PMIC reference: --</span>
    <span id="pillScan" class="pill neutral">SCAN: --</span>
  </div>
</div>

<div class="grid three section">
  <div class="card">
    <h3>Tools</h3>
    <div class="row">
      <button class="btnBlue" onclick="sendCmdValue('scan')">Scan</button>
      <button class="btnBlue" onclick="sendCmdValue('autodetect')">Auto-detect Roles</button>
      <button class="btnBlue" onclick="sendCmdValue('status')">Status</button>
      <button class="btnBlue" onclick="sendCmdValue('map')">Map Current Mode</button>
    </div>
    <div class="subtle">Read-only mapper. Uses current HSA/power state only; does not switch HSA or cold-cycle VIN_BULK.</div>
  </div>

  <div class="card">
    <h3>VIN_BULK Power</h3>
    <div class="toggleWrap">
      <div class="toggleRow">
        <div class="toggleLeft">
          <div class="toggleTitle">VIN_BULK Power</div>
          <div class="toggleMeta">GPIO32 optional 5 V VIN_BULK switch</div>
        </div>
        <div class="toggleRight">
          <span id="dimmState" class="stateBadge stateOff">OFF</span>
          <label class="switch">
            <input type="checkbox" id="dimmToggle" onchange="toggleDimmPower()">
            <span class="slider"></span>
          </label>
        </div>
      </div>

      <div class="toggleRow">
        <div class="toggleLeft">
          <div class="toggleTitle">PMIC VR / DRAM Rail Enable</div>
          <div class="toggleMeta">GPIO33 PWR_EN; optional for SPD/PMIC sideband reads</div>
        </div>
        <div class="toggleRight">
          <span id="pwrEnState" class="stateBadge stateOff">OFF</span>
          <label class="switch">
            <input type="checkbox" id="pwrEnToggle" onchange="togglePwrEn()">
            <span class="slider"></span>
          </label>
        </div>
      </div>

      <div class="row">
        <button class="btnAmber" onclick="sendCmdValue('dimmpwr cycle')">Cold-cycle VIN_BULK</button>
      </div>
    </div>
  </div>

  <div class="card">
    <h3>HSA / Address Strap</h3>
    <div class="row">
      <button onclick="sendCmdValue('hsa release')">Release HSA GPIO</button>
      <button onclick="sendCmdValue('hsa ground')">Drive HSA GND/OFFLINE</button>
    </div>
    <div class="subtle">GPIO27 control is experimental. Released GPIO lets the external HSA strap decide the address. Auto-detect determines the active SPD/HUB address. Cold-cycle VIN_BULK after HSA changes.</div>
  </div>
</div>

<div class="card section">
  <div class="toolbar">
    <div class="muted">Terminal Log</div>
    <div class="row">
      <button onclick="sendCmdValue('help')">Help</button>
      <button onclick="sendCmdValue('status')">Status</button>
      <button class="btnBlue" onclick="sendCmdValue('map')">Map Current Mode</button>
    </div>
  </div>

  <div class="row" style="margin:10px 0">
    <input id="cmd" type="text" placeholder="Type command e.g. read 0x50 0x0000 16" />
    <button onclick="sendCmd()">Run</button>
  </div>

  <pre id="term"></pre>
</div>

<div class="card section">
  <h3>Discovered Devices</h3>
  <div class="muted">Run Scan and this becomes the universal action area for each detected address.</div>
  <div id="devices" class="deviceList"></div>
</div>

<script>
const term = document.getElementById('term');
const cmdI = document.getElementById('cmd');
const devicesDiv = document.getElementById('devices');

const dimmToggle = document.getElementById('dimmToggle');
const pwrEnToggle = document.getElementById('pwrEnToggle');
const dimmState = document.getElementById('dimmState');
const pwrEnState = document.getElementById('pwrEnState');

let uiState = {
  pwr_good:false,
  pwr_en_released:false,
  dimm_power_on:false,
  hsa_low:false,
  good_valid:false,
  pmic_ref_valid:false,
  scan_count:0,
  scan_addrs:[],
  roles:[]
};

function hex2(v){
  return '0x' + Number(v).toString(16).toUpperCase().padStart(2,'0');
}

function setBadge(el, onText, offText, on){
  el.textContent = on ? onText : offText;
  el.className = 'stateBadge ' + (on ? 'stateOn' : 'stateOff');
}

function setPill(id, text, mode='neutral'){
  const el = document.getElementById(id);
  el.textContent = text;
  el.className = 'pill ' + mode;
}

function updatePills(j){
  setPill('pillPwrGood', 'PWR_GOOD: ' + (j.pwr_good ? 'READY' : 'LOW'), j.pwr_good ? 'ok' : 'bad');
  setPill('pillPwrEn', 'PWR_EN / VR: ' + (j.pwr_en_released ? 'ON' : 'OFF'), j.pwr_en_released ? 'ok' : 'bad');
  setPill('pillDimm', 'VIN_BULK: ' + (j.dimm_power_on ? 'ON' : 'OFF'), j.dimm_power_on ? 'ok' : 'bad');
  setPill('pillHsa', 'HSA GPIO: ' + (j.hsa_low ? 'DRIVING GND/OFFLINE' : 'RELEASED'), 'neutral');
  setPill('pillGood', 'Known-good SPD: ' + (j.good_valid ? 'PRESENT' : 'MISSING'), j.good_valid ? 'ok' : 'neutral');
  setPill('pillPmicRef', 'PMIC reference: ' + (j.pmic_ref_valid ? 'PRESENT' : 'MISSING'), j.pmic_ref_valid ? 'ok' : 'neutral');
  setPill('pillScan', 'SCAN: ' + (j.scan_count || 0) + ' device(s)', (j.scan_count || 0) > 0 ? 'ok' : 'neutral');
}

function updateToggleVisuals(j){
  dimmToggle.checked = !!j.dimm_power_on;
  pwrEnToggle.checked = !!j.pwr_en_released;
  setBadge(dimmState, 'ON', 'OFF', !!j.dimm_power_on);
  setBadge(pwrEnState, 'ON', 'OFF', !!j.pwr_en_released);
}

function buildDeviceButtons(addrs){
  devicesDiv.innerHTML = '';

  if(!addrs || addrs.length === 0){
    devicesDiv.innerHTML = '<div class="muted">(no scan results yet)</div>';
    return;
  }

  const roleByAddr = {};
  (uiState.roles || []).forEach((r)=>{ roleByAddr[Number(r.addr)] = r; });

  addrs.forEach((a)=>{
    const h = hex2(a);
    const r = roleByAddr[Number(a)] || inferRole(a);
    const wrap = document.createElement('div');
    wrap.className = 'deviceCard';
    const actions = buildRoleActions(h, r.role);

    wrap.innerHTML = `
      <div class="deviceHeader">
        <div>
          <div class="deviceAddr">${h} - ${r.label}</div>
          <div class="muted">${r.confidence ? 'confidence=' + r.confidence + '  ' : ''}${r.reason || ''}</div>
        </div>
      </div>

      <div class="deviceActions">
        ${actions}
      </div>
    `;

    devicesDiv.appendChild(wrap);
  });
}

function inferRole(a){
  const n = Number(a);
  if(n === 0x7E) return {role:'RESERVED', label:'Reserved/Unknown', confidence:'medium', reason:'reserved/broadcast style address'};
  if(n >= 0x50 && n <= 0x57) return {role:'SPD_HUB', label:'SPD/HUB', confidence:'low', reason:'address-range hint; run Auto-detect Roles'};
  if(n >= 0x48 && n <= 0x4F) return {role:'PMIC', label:'PMIC', confidence:'low', reason:'address-range hint; run Auto-detect Roles'};
  if((n >= 0x10 && n <= 0x17) || (n >= 0x30 && n <= 0x37) || n === 0x0C) {
    return {role:'TEMP_SENSOR_CANDIDATE', label:'Temp candidate', confidence:'low', reason:'address-range hint; run Auto-detect Roles'};
  }
  return {role:'UNKNOWN', label:'Unknown', confidence:'low', reason:'run Auto-detect Roles'};
}

function buildRoleActions(h, role){
  const reg = `<button onclick="sendCmdValue('reg ${h} 0x00 16')">Reg Read</button>`;
  if(role === 'SPD_HUB'){
    return `
      ${reg}
      <button onclick="sendCmdValue('read ${h} 0x0000 16')">SPD Read</button>
      <button onclick="sendCmdValue('dump ${h}')">Dump 1024</button>
      <button onclick="sendCmdValue('compare ${h}')">Compare vs known-good</button>
      <button class="btnGreen" onclick="confirmRun('capturegood ${h}','Save current SPD as known-good reference? This overwrites the stored SPD reference in ESP32 flash.')">Save known-good reference</button>
      <button onclick="sendCmdValue('verifygood ${h}')">Verify known-good</button>
    `;
  }
  if(role === 'PMIC'){
    return `
      ${reg}
      <button onclick="sendCmdValue('pmicid ${h}')">PMIC ID</button>
      <button onclick="sendCmdValue('pmicdumpat ${h} 0x00 0x80')">PMIC Dump</button>
      <button class="btnGreen" onclick="confirmRun('capturepmic ${h} 0x00 0x80','Save current PMIC registers as PMIC reference? This overwrites the stored PMIC reference in ESP32 flash.')">Save PMIC reference</button>
      <button onclick="sendCmdValue('comparepmic ${h}')">Compare PMIC reference</button>
    `;
  }
  if(role === 'RESERVED' || role === 'I3C_RESERVED_OR_BROADCAST'){
    return '';
  }
  return reg;
}

async function refreshStatus(){
  try{
    const r = await fetch('/api/status',{method:'POST'});
    const t = await r.text();

    try{
      const j = JSON.parse(t);
      uiState = j;
      updatePills(j);
      updateToggleVisuals(j);
      buildDeviceButtons(j.scan_addrs || []);
    }catch(e){
      console.error('status parse error', e, t);
    }
  }catch(e){
    console.error(e);
  }
}

async function refreshLog(){
  try{
    const r = await fetch('/api/log');
    const t = await r.text();
    term.textContent = t;
    term.scrollTop = term.scrollHeight;
  }catch(e){
    term.textContent = "ERR: " + e;
  }
}

async function clearLog(){
  await fetch('/api/clearlog',{method:'POST'});
  await refreshLog();
}

async function run(line){
  try{
    const body = "line=" + encodeURIComponent(line);
    await fetch('/api/run',{
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body
    });
  }catch(e){
    term.textContent += "\\nERR: " + e + "\\n";
  }

  await refreshStatus();
  await refreshLog();
}

function sendCmd(){
  const v = cmdI.value.trim();
  if(!v) return;
  run(v);
}

function sendCmdValue(v){
  cmdI.value = v;
  run(v);
}

function confirmRun(cmd, msg){
  if(!confirm(msg)) return;
  sendCmdValue(cmd);
}

function toggleDimmPower(){
  const wantOn = dimmToggle.checked;
  sendCmdValue(wantOn ? 'dimmpwr on' : 'dimmpwr off');
}

function togglePwrEn(){
  const wantOn = pwrEnToggle.checked;
  sendCmdValue(wantOn ? 'en on' : 'en off');
}

cmdI.addEventListener('keydown', (e)=>{
  if(e.key === 'Enter') sendCmd();
});

refreshStatus();
refreshLog();
setInterval(refreshStatus, 2500);
</script>
</body>
</html>)HTML";

static void handleWebRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

static void webSetup() {
  server.on("/", HTTP_GET, handleWebRoot);

  server.on("/api/status", HTTP_POST, handleWebStatus);
  server.on("/api/run",    HTTP_POST, handleWebRun);
  server.on("/api/log",    HTTP_GET,  handleWebLog);
  server.on("/api/clearlog", HTTP_POST, handleWebClearLog);

  server.on("/download/current.bin", HTTP_GET, handleDownloadCurrent);
  server.on("/download/good.bin",    HTTP_GET, handleDownloadGood);

  server.begin();
}

void webInit() {
  if (!WIFI_ENABLE) return;

  WiFi.mode(WIFI_AP);
  if (strlen(AP_PASS) >= 8) WiFi.softAP(AP_SSID, AP_PASS);
  else WiFi.softAP(AP_SSID);

  delay(50);
  webSetup();
}

void webPoll() {
  if (WIFI_ENABLE) server.handleClient();
}
