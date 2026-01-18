// web_pages.h - 存储网页相关的HTML内容，使用PROGMEM节省RAM

#ifndef WEB_PAGES_H
#define WEB_PAGES_H

#include <Arduino.h>

// CSS样式
const char STYLE_CSS[] PROGMEM = R"raw(
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

// 首页HTML
const char HTML_ROOT[] PROGMEM = R"raw(
<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>智能家居控制</title>
%s
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

// 空调控制页HTML
const char HTML_AC_CONTROL[] PROGMEM = R"raw(
<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>空调控制</title>
%s
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

// 配置页HTML
const char HTML_CONFIG[] PROGMEM = R"raw(
<!DOCTYPE html>
<html><head><title>系统配置</title>%s</head>
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

#endif // WEB_PAGES_H
