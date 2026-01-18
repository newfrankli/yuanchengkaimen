// YAP0F3AC.h
// Final, complete, and un-abbreviated header file for the YAP0F3 AC control library.
// Includes ALL analyzed features: Power, Mode, Temp, Fan (stateful), Swings, Light,
// Econo, Turbo, Dry, Sleep (stateful), and Timer.

#ifndef YAP0F3AC_H_
#define YAP0F3AC_H_

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>

// --- Constants for Fan Speed ---
const uint8_t kYAP0F3FanAuto = 0;
const uint8_t kYAP0F3FanSpeed1 = 1;
const uint8_t kYAP0F3FanSpeed2 = 2;
const uint8_t kYAP0F3FanSpeed3 = 3;
const uint8_t kYAP0F3FanSpeed4 = 4;
const uint8_t kYAP0F3FanSpeed5 = 5;

// --- Constants for Mode ---
const uint8_t kYAP0F3ModeAuto = 0;
const uint8_t kYAP0F3ModeCool = 1;
const uint8_t kYAP0F3ModeDry = 2;
const uint8_t kYAP0F3ModeFan = 3;
const uint8_t kYAP0F3ModeHeat = 4;

class YAP0F3AC {
 public:
  explicit YAP0F3AC(IRsend* irsend);

  void begin();
  void send();
  void setState(const uint8_t remote_state[]);

  // --- Main Control Functions (Setters) ---
  void on();
  void off();
  void setPower(bool on);
  void setTemp(uint8_t temp);
  void setFan(uint8_t speed);
  void setMode(uint8_t mode);
  void setSwingV(bool on);
  void setSwingH(bool on);
  void setLight(bool on);
  void setEcono(bool on);
  void setTurbo(bool on);
  void setSleep(uint8_t mode);
  void setDry(bool on);
  void setTimer(float hours); // Re-added Timer function

  // --- State Query Functions (Getters) ---
  bool getPower();
  uint8_t getTemp();
  uint8_t getFan();
  uint8_t getMode();
  bool getSwingV();
  bool getSwingH();
  bool getLight();
  bool getEcono();
  bool getTurbo();
  bool getDry();
  uint8_t getSleep();
  float getTimer(); // Re-added Timer function

 private:
  IRsend* _irsend;
  uint8_t _state[8];
  
  // Internal state variables for stateful features
  int   _sleep_mode;
  int   _fan_speed;
  float _timer_hours; // Re-added Timer variable
  
  void _parseState();
  void _updateState();
  void _updateChecksum();
};

#endif  // YAP0F3AC_H_