// YAP0F3AC.cpp
// Final, complete, and un-abbreviated implementation file for the YAP0F3 AC control library.
// Includes ALL analyzed features.

#include "YAP0F3AC.h"

YAP0F3AC::YAP0F3AC(IRsend* irsend) : _irsend(irsend) {}

void YAP0F3AC::begin() {
  _state[0] = 0x09;
  _state[1] = 0x0A;
  _state[2] = 0x20;
  _state[3] = 0x50;
  _state[4] = 0x00;
  _state[5] = 0x40;
  _state[6] = 0x00;
  _state[7] = 0x10;
  
  _sleep_mode = 0;
  _fan_speed = kYAP0F3FanAuto;
  _timer_hours = 0.0; // Initialize timer

  setPower(false);
}

void YAP0F3AC::send() {
  _updateState();
  _updateChecksum();
  _irsend->sendGree(_state, 8);
}

void YAP0F3AC::setState(const uint8_t remote_state[]) {
  memcpy(_state, remote_state, 8);
  _parseState();
}

// =========================================================================
//                      SETTER IMPLEMENTATIONS
// =========================================================================

void YAP0F3AC::on() { setPower(true); }
void YAP0F3AC::off() { setPower(false); }

void YAP0F3AC::setPower(bool on) {
  if (on) _state[0] |= 0x08;
  else {
    _state[0] &= ~0x08;
    // Reset stateful modes when turning off
    _sleep_mode = 0;
    _fan_speed = kYAP0F3FanAuto;
    _timer_hours = 0.0;
  }
}

void YAP0F3AC::setTemp(uint8_t temp) {
  temp = constrain(temp, 16, 30);
  _state[1] = (_state[1] & 0xF0) | (temp - 16);
}

void YAP0F3AC::setFan(uint8_t speed) {
  _fan_speed = constrain(speed, kYAP0F3FanAuto, kYAP0F3FanSpeed5);
}

void YAP0F3AC::setMode(uint8_t mode) {
    mode = constrain(mode, kYAP0F3ModeAuto, kYAP0F3ModeHeat);
    _state[0] = (_state[0] & 0xF8) | mode;
}

void YAP0F3AC::setSwingV(bool on) {
    uint8_t swing_h_part = _state[4] & 0xF0;
    if (on) _state[4] = swing_h_part | 0x01;
    else _state[4] = swing_h_part | 0x00;
}

void YAP0F3AC::setSwingH(bool on) {
    uint8_t swing_v_part = _state[4] & 0x0F;
    if (on) _state[4] = swing_v_part | 0x10;
    else _state[4] = swing_v_part | 0x00;
}

void YAP0F3AC::setLight(bool on) {
    if (on) _state[2] |= 0x20;
    else _state[2] &= ~0x20;
}

void YAP0F3AC::setEcono(bool on) {
    if (on) _state[7] |= 0x04;
    else _state[7] &= ~0x04;
}

void YAP0F3AC::setTurbo(bool on) {
    if (on) _state[2] |= 0x10;
    else _state[2] &= ~0x10;
}

void YAP0F3AC::setDry(bool on) {
    if (on) _state[2] |= 0x80;
    else _state[2] &= ~0x80;
}

void YAP0F3AC::setSleep(uint8_t mode) {
    _sleep_mode = constrain(mode, 0, 4);
}

// Re-added Timer function
void YAP0F3AC::setTimer(float hours) {
    _timer_hours = constrain(hours, 0.0, 24.0);
}


// =========================================================================
//                      GETTER IMPLEMENTATIONS
// =========================================================================

bool YAP0F3AC::getPower() { return (_state[0] & 0x08) != 0; }
uint8_t YAP0F3AC::getTemp() { return (_state[1] & 0x0F) + 16; }
uint8_t YAP0F3AC::getFan() { return _fan_speed; }
uint8_t YAP0F3AC::getMode() { return _state[0] & 0x07; }
bool YAP0F3AC::getSwingV() { return (_state[4] & 0x0F) != 0; }
bool YAP0F3AC::getSwingH() { return ((_state[4] >> 4) & 0x0F) != 0; }
bool YAP0F3AC::getLight() { return (_state[2] & 0x20) != 0; }
bool YAP0F3AC::getEcono() { return (_state[7] & 0x04) != 0; }
bool YAP0F3AC::getTurbo() { return (_state[2] & 0x10) != 0; }
bool YAP0F3AC::getDry() { return (_state[2] & 0x80) != 0; }
uint8_t YAP0F3AC::getSleep() { return _sleep_mode; }
float YAP0F3AC::getTimer() { return _timer_hours; } // Re-added Timer function

// =========================================================================
//                      PRIVATE HELPER FUNCTIONS
// =========================================================================

void YAP0F3AC::_parseState() {
  if (getPower()) {
    // --- Parse Sleep Mode ---
    bool incoming_sleep_bit = (_state[0] & 0x80) != 0;
    if (incoming_sleep_bit) {
      if (_sleep_mode == 0) _sleep_mode = 1; else if (_sleep_mode < 4) _sleep_mode++;
    } else {
      if (_sleep_mode == 1) _sleep_mode = 2; else if (_sleep_mode > 1) _sleep_mode = 0;
    }

    // --- Parse Fan Speed ---
    uint8_t fan_code = (_state[0] >> 4) & 0x07;
    uint8_t base_fan_code = fan_code;
    if (getSwingH()) base_fan_code &= ~0b0100;

    if (base_fan_code == 0) _fan_speed = 0;
    else if (base_fan_code == 1) _fan_speed = 1;
    else if (base_fan_code == 2) _fan_speed = 2;
    else if (base_fan_code == 3) {
      if (_fan_speed < 3) _fan_speed = 3; else if (_fan_speed < 5) _fan_speed++;
    }
  } else {
    _sleep_mode = 0;
    _fan_speed = kYAP0F3FanAuto;
  }
  
  // --- Parse Timer ---
  uint8_t timer_half_hour_code = _state[1] >> 4;
  uint8_t timer_hours_code = _state[2] & 0x0F;
  if (timer_half_hour_code == 0) {
    _timer_hours = 0.0;
  } else {
    float total_hours = timer_hours_code;
    if (timer_half_hour_code == 9) total_hours += 0.5;
    _timer_hours = total_hours;
  }
}

void YAP0F3AC::_updateState() {
  // --- Update Fan Speed ---
  uint8_t fan_base_code = 0;
  if (_fan_speed == kYAP0F3FanAuto) fan_base_code = 0;
  else if (_fan_speed == kYAP0F3FanSpeed1) fan_base_code = 1;
  else if (_fan_speed == kYAP0F3FanSpeed2) fan_base_code = 2;
  else if (_fan_speed >= kYAP0F3FanSpeed3) fan_base_code = 3;
  
  uint8_t final_fan_code = fan_base_code;
  if (getSwingH()) final_fan_code |= 0b0100;
  
  _state[0] = (_state[0] & 0x8F) | (final_fan_code << 4);

  // --- Update Sleep ---
  if (_sleep_mode > 0) _state[0] |= 0x80;
  else _state[0] &= ~0x80;
  
  // --- Update Timer ---
  if (_timer_hours <= 0.0) {
    _state[1] &= 0x0F; // Clear high nibble for timer off
    _state[2] &= 0xF0; // Clear low nibble for timer off
  } else {
    int whole_hours = (int)_timer_hours;
    bool half_hour = (_timer_hours - whole_hours) > 0.0;
    
    if (half_hour) _state[1] = (_state[1] & 0x0F) | 0x90;
    else _state[1] = (_state[1] & 0x0F) | 0x80;
    
    _state[2] = (_state[2] & 0xF0) | (whole_hours & 0x0F);
  }
}

void YAP0F3AC::_updateChecksum() {
  uint8_t checksum = 0;
  for (int i = 0; i < 7; i++) {
    checksum += (_state[i] & 0x0F) + ((_state[i] >> 4) & 0x0F);
  }
  _state[7] = (_state[7] & 0x0F) | ((checksum & 0x0F) << 4);
}