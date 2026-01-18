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
#include "web_pages.h"

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
  // 从PROGMEM中读取HTML内容
  char buffer[1500]; // 确保缓冲区足够大
  sprintf_P(buffer, HTML_ROOT, STYLE_CSS);
  
  String html = String(buffer);
  html.replace("%STATUS%", sysStatus);
  html.replace("%DOOR_STATUS%", servoActive ? "已开门" : "已锁定");
  html.replace("%DISABLED%", servoActive ? "disabled" : "");
  server.send(200, "text/html", html);
}

void handleACControl() {
  // 从PROGMEM中读取HTML内容
  char buffer[4000]; // 确保缓冲区足够大
  sprintf_P(buffer, HTML_AC_CONTROL, STYLE_CSS);
  
  String html = String(buffer);
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
  // 从PROGMEM中读取HTML内容
  char buffer[2000]; // 确保缓冲区足够大
  sprintf_P(buffer, HTML_CONFIG, STYLE_CSS);
  
  String html = String(buffer);
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