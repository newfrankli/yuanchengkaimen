#define kCaptureBufferSize 256

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <Servo.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include "YAP0F3AC.h" 

#define SERVO_PIN 12     // D6
#define IR_PIN 14        // D5 (发射)
#define RECV_PIN 13      // D7 (接收)

#define AP_SSID "SmartHome_AP"
#define AP_PASS "admin1234"

// 前向声明
void handleRoot();
void handleACControl();
void handleConfig();
void handleControl();
void handleACStatus();
void saveConfig();
void forgetNetwork();
void checkServo();
String getACMode();
String getFanSpeed();
String getSleepStatus();
String getSwingVerticalStatus();
String getSwingHorizontalStatus();
String getLightStatus();
void printACStatusToSerial();

ESP8266WebServer server(80);
DNSServer dnsServer;
Servo doorServo;

IRsend irsend(IR_PIN);
YAP0F3AC ac(&irsend);
IRrecv irrecv(RECV_PIN);
decode_results results;

// <-- NEW: 用于软件防抖，防止重复处理同一按键的多次信号
unsigned long lastIrSignalTime = 0;
const int irDebounceTime = 500; // 500毫秒的冷却时间

struct WiFiConfig {
  char ssid[32];
  char password[64];
  char acModel[8] = "YAP0F3"; 
} savedConfig;

bool servoActive = false;
unsigned long servoStart = 0;
String sysStatus = "Initializing";

const String styleCSS = R"raw(
<style>
  body { font-family:Arial, sans-serif; margin:20px; background:#f8f9fa; }
  .card {
    background:white;
    padding:20px;
    border-radius:8px;
    box-shadow:0 2px 4px rgba(0,0,0,0.1);
    margin-bottom:20px;
  }
  button {
    padding:10px 20px;
    border:none;
    border-radius:5px;
    cursor:pointer;
    margin:5px;
    transition:0.3s;
  }
  .btn-primary { background:#007bff; color:white; }
  .btn-danger { background:#dc3545; color:white; }
  .btn-secondary { background:#6c757d; color:white; }
  button:disabled { background:#cccccc; cursor:not-allowed; }
  input, select { 
    width:100%; padding:8px; margin:8px 0; border:1px solid #ddd; 
    border-radius:4px; box-sizing:border-box; 
  }
  input[type=range] { padding: 0; }
  .status { 
    color:#28a745; font-weight:bold; padding:10px; background:#e8f5e9; 
    border-radius:4px; margin-bottom:15px; 
  }
  .nav-bar {
    display:flex;
    gap:10px;
    margin-bottom:20px;
  }
  .message {
    position:fixed;
    top:20px;
    right:20px;
    padding:15px;
    background:#d4edda;
    border:1px solid #c3e6cb;
    border-radius:4px;
    color:#155724;
    animation: fadeOut 2s 2s forwards;
    z-index:1000;
  }
  .temp-slider {
    display: flex;
    align-items: center;
    gap: 15px;
    margin-top: 10px;
  }
  @keyframes fadeOut {
    from { opacity:1; }
    to { opacity:0; display:none; }
  }
</style>
)raw";

void setup() {
  Serial.begin(9600);
  
  doorServo.attach(SERVO_PIN, 500, 2500);
  doorServo.write(0);
  
  irsend.begin();
  ac.begin();
  
  irrecv.enableIRIn();
  Serial.println("\nIR Receiver is ready to decode!");

  EEPROM.begin(512);
  EEPROM.get(0, savedConfig);
  
  if (savedConfig.ssid[0] == 0xFF) {
    Serial.println("EEPROM uninitialized. Setting defaults.");
    memset(&savedConfig, 0, sizeof(savedConfig));
    strcpy(savedConfig.acModel, "YAP0F3");
  }
  
  if (strlen(savedConfig.ssid) > 0) {
    connectToWiFi();
  } else {
    startAPMode();
  }
  
  server.on("/", HTTP_GET, handleRoot);
  server.on("/ac-control", HTTP_GET, handleACControl);
  server.on("/config", HTTP_GET, handleConfig);
  server.on("/control", HTTP_GET, handleControl);
  server.on("/save", HTTP_POST, saveConfig);
  server.on("/forget", HTTP_GET, forgetNetwork);
  server.on("/ac-status", HTTP_GET, handleACStatus);
  server.onNotFound(handleRoot);
  
  server.begin();
}

void loop() {
  if (irrecv.decode(&results)) {
    Serial.print(F("IR Signal Received: "));
    Serial.println(resultToHumanReadableBasic(&results));

    // <-- MODIFIED: 增加 TECO 协议支持，并加入软件防抖逻辑
    if (results.decode_type == GREE || results.decode_type == KELVINATOR || results.decode_type == TECO) {
      if (millis() - lastIrSignalTime > irDebounceTime) {
        ac.setState(results.state); 
        printACStatusToSerial();
        lastIrSignalTime = millis(); // 更新最后一次成功处理的时间
      } else {
        Serial.println("Signal ignored (debounce).");
      }
    }
    irrecv.resume(); 
  }

  if (WiFi.getMode() == WIFI_AP) {
    dnsServer.processNextRequest();
  }
  server.handleClient();
  checkServo();
}

/*************** 网络功能 (无变化) ***************/
void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(savedConfig.ssid, savedConfig.password);
  
  Serial.print("Connecting...");
  for (int i=0; i<20; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      sysStatus = "STA模式 | IP: " + WiFi.localIP().toString();
      Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
      return;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnection failed!");
  startAPMode();
}
void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  sysStatus = "AP模式 | IP: " + WiFi.softAPIP().toString();
  dnsServer.start(53, "*", WiFi.softAPIP());
  Serial.println("AP Mode Started. IP: " + WiFi.softAPIP().toString());
  Serial.println("Captive Portal is active.");
}
void checkServo() {
  if (servoActive && (millis() - servoStart > 2000)) {
    doorServo.write(0);
    servoActive = false;
    Serial.println("Door locked");
  }
}

/*************** 网页界面 (无变化) ***************/
void handleRoot() {
  String html = R"raw(
<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>智能家居控制</title>
)raw" + styleCSS + R"raw(
<script>
function showMessage(msg) {
  const div = document.createElement('div');
  div.className = 'message';
  div.innerHTML = msg;
  document.body.appendChild(div);
}
function sendCommand(cmd) {
  fetch('/control?cmd=' + cmd)
    .then(response => response.text())
    .then(data => {
      showMessage(data);
      if (data.includes('开门')) {
        setTimeout(() => location.reload(), 2100);
      }
    })
    .catch(err => showMessage('错误：' + err));
}
</script>
</head>
<body>
  <div class="nav-bar">
    <a href="/"><button class="btn-primary">门锁</button></a>
    <a href="/ac-control"><button class="btn-primary">空调</button></a>
    <a href="/config"><button class="btn-primary">配置</button></a>
  </div>
  <div class="card">
    <h2>门锁控制</h2>
    <div class="status">系统状态：%STATUS%</div>
    <button onclick="sendCommand('door_open')" %DISABLED%>开门</button>
    <p>当前状态：%DOOR_STATUS%</p>
  </div>
</body></html>
)raw";
  html.replace("%STATUS%", sysStatus);
  html.replace("%DOOR_STATUS%", servoActive ? "已开门" : "已锁定");
  html.replace("%DISABLED%", servoActive ? "disabled" : "");
  server.send(200, "text/html", html);
}

void handleACControl() {
  String html = R"raw(
<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>空调控制</title>
)raw" + styleCSS + R"raw(
</head>
<body>
  <div class="nav-bar">
    <a href="/"><button class="btn-primary">门锁</button></a>
    <a href="/ac-control"><button class="btn-primary">空调</button></a>
    <a href="/config"><button class="btn-primary">配置</button></a>
  </div>
  <div class="card">
    <h2>空调控制（型号：YAP0F3）</h2>
    <div class="status">系统状态：%STATUS%</div>
    
    <h3>基础控制</h3>
    <button class="btn-primary" onclick="sendCommand('ac_on')">开机</button>
    <button class="btn-primary" onclick="sendCommand('ac_off')">关机</button>
    <p>当前状态：<span id="ac-power-status">%AC_POWER%</span></p>

    <h3>温度调节</h3>
    <button class="btn-secondary" onclick="sendCommand('temp_up')">温度+</button>
    <button class="btn-secondary" onclick="sendCommand('temp_down')">温度-</button>
    <div class="temp-slider">
      <span>16℃</span>
      <input type="range" min="16" max="30" value="%AC_TEMP%" id="temp-slider" onchange="setTempBySlider(this.value)" style="width:100%;">
      <span>30℃</span>
    </div>
    <p>当前温度：<span id="ac-temp">%AC_TEMP%</span>℃</p>

    <h3>运行模式</h3>
    <button onclick="sendCommand('mode','cool')">制冷</button>
    <button onclick="sendCommand('mode','heat')">制热</button>
    <button onclick="sendCommand('mode','dry')">除湿</button>
    <button onclick="sendCommand('mode','fan')">送风</button>
    <button onclick="sendCommand('mode','auto')">自动</button>
    <p>当前模式：<span id="ac-mode">%AC_MODE%</span></p>

    <h3>风速控制</h3>
    <button onclick="sendCommand('fan_cycle')">切换风速</button>
    <p>当前风速：<span id="fan-speed">%FAN_SPEED%</span></p>

    <h3>上下扫风</h3>
    <button onclick="sendCommand('swing_v_on')">开启</button>
    <button onclick="sendCommand('swing_v_off')">关闭</button>
    <p>状态：<span id="swing-v-status">%SWING_V_STATUS%</span></p>
    
    <h3>左右扫风</h3>
    <button onclick="sendCommand('swing_h_on')">开启</button>
    <button onclick="sendCommand('swing_h_off')">关闭</button>
    <p>状态：<span id="swing-h-status">%SWING_H_STATUS%</span></p>

    <h3>灯光控制</h3>
    <button onclick="sendCommand('light_on')">开启灯光</button>
    <button onclick="sendCommand('light_off')">关闭灯光</button>
    <p>灯光状态：<span id="light-status">%LIGHT_STATUS%</span></p>

    <h3>睡眠设置</h3>
    <button onclick="sendCommand('sleep_cycle')">切换睡眠模式</button>
    <p>睡眠模式：<span id="sleep-status">%SLEEP_STATUS%</span></p>

    <h3>定时功能</h3>
    <div>
      定时关机：<input type="number" step="0.5" id="timer_hours" min="0.5" max="24" style="width:80px"> 小时
      <button class="btn-secondary" onclick="setTimer()">设置</button>
      <button class="btn-danger" onclick="sendCommand('timer_cancel')">取消定时</button>
    </div>
    <p>当前定时：<span id="timer-status">%TIMER_STATUS%</span></p>
  </div>

<script>
function showMessage(msg) {
  const oldMsg = document.querySelector('.message');
  if(oldMsg) oldMsg.remove();
  const div = document.createElement('div');
  div.className = 'message';
  div.innerHTML = msg;
  document.body.appendChild(div);
}
function updateACStatus() {
  fetch('/ac-status')
    .then(response => response.json())
    .then(data => {
      document.getElementById('ac-power-status').innerText = data.power;
      document.getElementById('ac-temp').innerText = data.temp;
      document.getElementById('ac-mode').innerText = data.mode;
      document.getElementById('fan-speed').innerText = data.fan;
      document.getElementById('sleep-status').innerText = data.sleep;
      document.getElementById('timer-status').innerText = data.timer;
      document.getElementById('swing-v-status').innerText = data.swing_v;
      document.getElementById('swing-h-status').innerText = data.swing_h;
      document.getElementById('light-status').innerText = data.light;
      document.getElementById('temp-slider').value = data.temp;
    })
    .catch(err => console.error('状态更新失败:', err));
}
function sendCommand(cmd, value = null) {
  let url = '/control?cmd=' + cmd;
  if (value !== null) url += '&value=' + value;
  fetch(url)
    .then(response => response.text())
    .then(data => {
      showMessage(data);
      setTimeout(updateACStatus, 500); 
    })
    .catch(err => showMessage('错误：' + err));
}
function setTempBySlider(value) {
  document.getElementById('ac-temp').innerText = value;
  sendCommand('temp_set', value);
}
function setTimer() {
  const hours = document.getElementById('timer_hours').value;
  if (hours >= 0.5 && hours <= 24) { sendCommand('timer', hours); } 
  else { showMessage('请输入0.5-24之间的小时数'); }
}
document.addEventListener('DOMContentLoaded', () => { setInterval(updateACStatus, 3000); });
</script>

</body></html>
)raw";
  html.replace("%STATUS%", sysStatus);
  html.replace("%AC_POWER%", ac.getPower() ? "运行中" : "已关闭");
  html.replace("%AC_TEMP%", String(ac.getTemp()));
  html.replace("%AC_MODE%", getACMode());
  html.replace("%FAN_SPEED%", getFanSpeed());
  html.replace("%SLEEP_STATUS%", getSleepStatus());
  html.replace("%TIMER_STATUS%", ac.getTimer() > 0 ? String(ac.getTimer()) + "小时后关机" : "未设置");
  html.replace("%SWING_V_STATUS%", getSwingVerticalStatus());
  html.replace("%SWING_H_STATUS%", getSwingHorizontalStatus());
  html.replace("%LIGHT_STATUS%", getLightStatus());
  server.send(200, "text/html", html);
}

void handleConfig() {
  String html = R"raw(
<!DOCTYPE html>
<html><head><title>系统配置</title>)raw" + styleCSS + R"raw(</head>
<body>
  <div class="nav-bar">
    <a href="/"><button class="btn-primary">门锁</button></a>
    <a href="/ac-control"><button class="btn-primary">空调</button></a>
    <a href="/config"><button class="btn-primary">配置</button></a>
  </div>
  <div class="card">
    <h2>系统配置</h2>
    <form method="post" action="/save">
      <h3>WiFi设置</h3>
      <input type="text" name="ssid" placeholder="WiFi名称 (SSID)" value="%CURRENT_SSID%" required>
      <input type="password" name="password" placeholder="WiFi密码">
      <p style="font-size: smaller; color: #888;">注意：空调型号已固定为YAP0F3，无需配置。</p>
      <button type="submit" class="btn-primary">保存配置并重启</button>
    </form>
    <h3>其他操作</h3>
    <a href="/forget"><button class="btn-danger">忘记网络并重启</button></a>
  </div>
</body></html>
)raw";
  html.replace("%CURRENT_SSID%", String(savedConfig.ssid));
  server.send(200, "text/html", html);
}

/*************** 控制逻辑与状态API (无变化) ***************/
void handleACStatus() {
  String json = "{";
  json += "\"power\":\"" + String(ac.getPower() ? "运行中" : "已关闭") + "\",";
  json += "\"temp\":" + String(ac.getTemp()) + ",";
  json += "\"mode\":\"" + getACMode() + "\",";
  json += "\"fan\":\"" + getFanSpeed() + "\",";
  json += "\"swing_v\":\"" + getSwingVerticalStatus() + "\",";
  json += "\"swing_h\":\"" + getSwingHorizontalStatus() + "\",";
  json += "\"light\":\"" + getLightStatus() + "\",";
  json += "\"sleep\":\"" + getSleepStatus() + "\",";
  json += "\"timer\":\"" + (ac.getTimer() > 0 ? String(ac.getTimer()) + "小时后关机" : "未设置") + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleControl() {
  if (!server.hasArg("cmd")) { server.send(400, "text/plain", "缺少指令参数"); return; }
  String cmd = server.arg("cmd");
  String value = server.hasArg("value") ? server.arg("value") : "";
  String response = "操作成功";
  bool sendSignal = false;

  if (cmd == "door_open") {
    if (!servoActive) {
      doorServo.write(180); servoActive = true; servoStart = millis();
      response = "正在开门...（2秒后自动关闭）";
    } else { response = "门锁正在操作中，请稍候"; }
  }
  else if (cmd == "ac_on") { ac.on(); sendSignal = true; } 
  else if (cmd == "ac_off") { ac.off(); sendSignal = true; } 
  else if (cmd == "temp_up") { ac.setTemp(min(30, ac.getTemp() + 1)); response += " → 温度: " + String(ac.getTemp()) + "℃"; sendSignal = true; } 
  else if (cmd == "temp_down") { ac.setTemp(max(16, ac.getTemp() - 1)); response += " → 温度: " + String(ac.getTemp()) + "℃"; sendSignal = true; } 
  else if (cmd == "temp_set") {
    int temp = value.toInt();
    if (temp >= 16 && temp <= 30) { ac.setTemp(temp); response = "温度已设为 " + String(temp) + "℃"; sendSignal = true; } 
    else { response = "温度值无效"; }
  } else if (cmd == "mode") {
    if (value == "cool") ac.setMode(kYAP0F3ModeCool);
    else if (value == "heat") ac.setMode(kYAP0F3ModeHeat);
    else if (value == "dry") ac.setMode(kYAP0F3ModeDry);
    else if (value == "fan") ac.setMode(kYAP0F3ModeFan);
    else if (value == "auto") ac.setMode(kYAP0F3ModeAuto);
    response += " → 模式: " + getACMode(); sendSignal = true;
  } 
  else if (cmd == "fan_cycle") {
    uint8_t currentFan = ac.getFan();
    uint8_t nextFan = (currentFan + 1) % 6;
    ac.setFan(nextFan);
    response += " → " + getFanSpeed();
    sendSignal = true;
  }
  else if (cmd == "sleep_cycle") {
    uint8_t currentSleep = ac.getSleep();
    uint8_t nextSleep = (currentSleep + 1) % 5;
    ac.setSleep(nextSleep);
    if (nextSleep > 0) { ac.setPower(true); }
    response += " → " + getSleepStatus();
    sendSignal = true;
  }
  else if (cmd == "timer") {
    float hours = value.toFloat();
    if (hours >= 0.5 && hours <= 24) { ac.setPower(true); ac.setTimer(hours); response += " → 已设置" + String(hours) + "小时后关闭"; sendSignal = true; } 
    else { response = "定时参数错误（0.5-24小时）"; }
  } else if (cmd == "timer_cancel") { ac.setTimer(0); response = "操作成功：已取消定时"; sendSignal = true; } 
  else if (cmd == "swing_v_on") { ac.setSwingV(true); response = "操作成功：开启上下扫风"; sendSignal = true; } 
  else if (cmd == "swing_v_off") { ac.setSwingV(false); response = "操作成功：关闭上下扫风"; sendSignal = true; } 
  else if (cmd == "swing_h_on") { ac.setSwingH(true); response = "操作成功：开启左右扫风"; sendSignal = true; } 
  else if (cmd == "swing_h_off") { ac.setSwingH(false); response = "操作成功：关闭左右扫风"; sendSignal = true; } 
  else if (cmd == "light_on") { ac.setLight(true); response = "操作成功：开启灯光"; sendSignal = true; } 
  else if (cmd == "light_off") { ac.setLight(false); response = "操作成功：关闭灯光"; sendSignal = true; }
  
  if (sendSignal) {
    irrecv.disableIRIn();
    ac.send();
    irrecv.enableIRIn();
    delay(50);
  }
  
  server.send(200, "text/plain", response);
}

/*************** 辅助函数 (无变化) ***************/
void printACStatusToSerial() {
  Serial.println(F("===== AC State Updated via IR Remote ====="));
  Serial.print(F("  - Power: ")); Serial.println(ac.getPower() ? "ON" : "OFF");
  Serial.print(F("  - Temp:  ")); Serial.print(ac.getTemp()); Serial.println(F(" C"));
  Serial.print(F("  - Mode:  ")); Serial.println(getACMode());
  Serial.print(F("  - Fan:   ")); Serial.println(getFanSpeed());
  Serial.print(F("  - SwingV:")); Serial.println(getSwingVerticalStatus());
  Serial.print(F("  - SwingH:")); Serial.println(getSwingHorizontalStatus());
  Serial.print(F("  - Light: ")); Serial.println(getLightStatus());
  Serial.print(F("  - Sleep: ")); Serial.println(getSleepStatus());
  Serial.print(F("  - Timer: "));
  if (ac.getTimer() > 0) {
    Serial.print(ac.getTimer()); Serial.println(F(" hours"));
  } else {
    Serial.println(F("OFF"));
  }
  Serial.println(F("========================================"));
}

String getACMode() {
  switch (ac.getMode()) {
    case kYAP0F3ModeCool: return "制冷";
    case kYAP0F3ModeHeat: return "制热";
    case kYAP0F3ModeDry: return "除湿";
    case kYAP0F3ModeFan: return "送风";
    case kYAP0F3ModeAuto: return "自动";
    default: return "未知";
  }
}
String getFanSpeed() {
  switch (ac.getFan()) {
    case kYAP0F3FanAuto: return "自动";
    case kYAP0F3FanSpeed1: return "1档 (低速)";
    case kYAP0F3FanSpeed2: return "2档";
    case kYAP0F3FanSpeed3: return "3档 (中速)";
    case kYAP0F3FanSpeed4: return "4档";
    case kYAP0F3FanSpeed5: return "5档 (高速)";
    default: return "未知";
  }
}
String getSleepStatus() {
  uint8_t sleepMode = ac.getSleep();
  if (sleepMode == 0) {
    return "已关闭";
  } else {
    return "模式 " + String(sleepMode);
  }
}
String getSwingVerticalStatus() { return ac.getSwingV() ? "已开启" : "已关闭"; }
String getSwingHorizontalStatus() { return ac.getSwingH() ? "已开启" : "已关闭"; }
String getLightStatus() { return ac.getLight() ? "已开启" : "已关闭"; }

/*************** 配置保存 (无变化) ***************/
void saveConfig() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  if (ssid.isEmpty()) { server.send(400, "text/plain", "SSID不能为空"); return; }
  
  memset(savedConfig.ssid, 0, sizeof(savedConfig.ssid));
  memset(savedConfig.password, 0, sizeof(savedConfig.password));
  strncpy(savedConfig.ssid, ssid.c_str(), sizeof(savedConfig.ssid) - 1);
  strncpy(savedConfig.password, password.c_str(), sizeof(savedConfig.password) - 1);
  
  EEPROM.put(0, savedConfig);
  EEPROM.commit();
  
  server.send(200, "text/html", "<div class='card'><h2>配置已保存，设备正在重启...</h2></div><meta http-equiv='refresh' content='2;url=/'>");
  delay(1000);
  ESP.restart();
}
void forgetNetwork() {
  memset(savedConfig.ssid, 0, sizeof(savedConfig.ssid));
  memset(savedConfig.password, 0, sizeof(savedConfig.password));
  EEPROM.put(0, savedConfig);
  EEPROM.commit();
  server.send(200, "text/html", "<div class='card'><h2>已清除网络配置，设备正在重启进入AP模式...</h2></div><meta http-equiv='refresh' content='2;url=/'>");
  delay(1000);
  ESP.restart();
}