#pragma once
// ---------------------------------------------------------------------------
// AXS5106L capacitive touch controller - standalone Arduino driver
// (no LovyanGFX dependency; uses the Arduino Wire library)
//
// What is known about the chip (from Waveshare's reference BSP code):
//   I2C address : 0x63
//   Reset       : active low; needs ~200 ms low and ~300 ms after release
//                 before the firmware answers on I2C
//   INT         : active low, pulses when a new report is ready
//   Reg 0x01    : touch report, read 14 bytes
//                   [0]      gesture (unused here)
//                   [1]      number of touch points (low nibble)
//                   [2..7]   point 0, [8..13] point 1:
//                     b0 : event (bits 7:6) | X[11:8] (bits 3:0)
//                     b1 : X[7:0]
//                     b2 : id    (bits 7:4) | Y[11:8] (bits 3:0)
//                     b3 : Y[7:0]
//                     b4, b5 : weight/area (unused)
//   Reg 0x08    : chip / firmware ID (3 bytes) - handy to confirm comms
//
// Usage:
//   AXS5106L touch;
//   touch.begin(Wire, 18, 19, 20, 21);   // SDA, SCL, RST, INT
//   touch.setRotation(lcd.getRotation());
//   AXS5106L::Point p;
//   if (touch.read(p)) { ... p.x, p.y ... }
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <Wire.h>

class AXS5106L
{
public:
  static constexpr uint8_t  DEFAULT_ADDR  = 0x63;
  static constexpr uint8_t  REG_TOUCH     = 0x01;
  static constexpr uint8_t  REG_ID        = 0x08;
  static constexpr uint8_t  MAX_POINTS    = 2;

  struct Point {
    int16_t x = 0;
    int16_t y = 0;
    uint8_t id = 0;
    uint8_t event = 0;   // 0 = down, 1 = up, 2 = contact (FocalTech-style)
  };

  // Native panel size in portrait (rotation 0)
  AXS5106L(uint16_t width = 172, uint16_t height = 320, uint8_t addr = DEFAULT_ADDR)
    : _w(width), _h(height), _addr(addr) {}

  // Returns true if the chip ACKs on the bus after reset.
  bool begin(TwoWire &wire, int sda, int scl, int rst = -1, int intPin = -1,
             uint32_t freq = 400000)
  {
    _wire = &wire;
    _rst  = rst;
    _int  = intPin;

    _wire->begin(sda, scl, freq);
    _wire->setTimeOut(20);

    reset();

    if (_int >= 0) {
      pinMode(_int, INPUT_PULLUP);
      attachInterruptArg(digitalPinToInterrupt(_int), isr, this, FALLING);
    }

    _ok = probe();
    return _ok;
  }

  void reset()
  {
    if (_rst < 0) return;
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, LOW);
    delay(200);
    digitalWrite(_rst, HIGH);
    delay(300);
  }

  // True if the device ACKs its address.
  bool probe()
  {
    _wire->beginTransmission(_addr);
    return _wire->endTransmission() == 0;
  }

  // Reads the 3-byte ID register. Returns false on I2C failure.
  bool readId(uint8_t id[3]) { return readReg(REG_ID, id, 3); }

  // Rotation matches LovyanGFX setRotation(): 0/2 portrait, 1/3 landscape.
  void setRotation(uint8_t r) { _rot = r & 3; }

  // The controller's X axis runs opposite to this panel in portrait.
  void setMirrorX(bool m) { _mirrorX = m; }
  void setMirrorY(bool m) { _mirrorY = m; }
  void setSwapXY(bool s)  { _swapXY = s; }

  // Ignore INT and just poll I2C every call (useful for debugging).
  void setPolling(bool p) { _polling = p; }

  // Print raw register bytes to Serial on every read.
  void setDebug(bool d) { _debug = d; }

  bool ok() const { return _ok; }

  // Reads all touch points into pts[]. Returns number of points (0..2).
  // Coordinates are mapped to screen space for the current rotation.
  uint8_t read(Point *pts, uint8_t maxPts = MAX_POINTS)
  {
    if (!_wire || maxPts == 0) return 0;

    // With INT: only talk to the chip when it has signalled new data (or is
    // still holding INT low). Otherwise keep the last state briefly, then
    // treat silence as release.
    if (!_polling && _int >= 0) {
      bool pending = _intFlag || digitalRead(_int) == LOW;
      if (!pending) {
        if (_lastCount && (millis() - _lastReadMs) > RELEASE_TIMEOUT_MS) _lastCount = 0;
        uint8_t n = min(_lastCount, maxPts);
        for (uint8_t i = 0; i < n; ++i) pts[i] = _last[i];
        return n;
      }
      _intFlag = false;
    }

    uint8_t d[14] = {0};
    if (!readReg(REG_TOUCH, d, sizeof(d))) {
      _lastCount = 0;
      return 0;
    }
    _lastReadMs = millis();

    if (_debug) {
      Serial.print("AXS5106L:");
      for (uint8_t b : d) Serial.printf(" %02X", b);
      Serial.println();
    }

    uint8_t n = d[1] & 0x0F;
    if (n > MAX_POINTS) n = 0;          // garbage frame
    uint8_t out = 0;

    for (uint8_t i = 0; i < n; ++i) {
      const uint8_t *p = &d[2 + i * 6];
      uint8_t event = p[0] >> 6;
      if (event == 1) continue;         // lift-off report, not a touch
      int16_t rx = ((p[0] & 0x0F) << 8) | p[1];
      int16_t ry = ((p[2] & 0x0F) << 8) | p[3];
      Point pt;
      pt.id    = p[2] >> 4;
      pt.event = event;
      map(rx, ry, pt.x, pt.y);
      _last[out++] = pt;
    }

    _lastCount = out;
    uint8_t ret = min(out, maxPts);
    for (uint8_t i = 0; i < ret; ++i) pts[i] = _last[i];
    return ret;
  }

  // Convenience: single point.
  bool read(Point &p) { return read(&p, 1) > 0; }

  bool readReg(uint8_t reg, uint8_t *buf, size_t len)
  {
    // Waveshare's code sends a STOP between the register write and the read
    // (no repeated start), so do the same.
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    if (_wire->endTransmission(true) != 0) return false;
    if (_wire->requestFrom(_addr, (uint8_t)len) != len) return false;
    for (size_t i = 0; i < len; ++i) buf[i] = _wire->read();
    return true;
  }

private:
  static constexpr uint32_t RELEASE_TIMEOUT_MS = 60;

  static void IRAM_ATTR isr(void *arg) { static_cast<AXS5106L *>(arg)->_intFlag = true; }

  // raw (controller) -> portrait -> rotated screen coordinates
  void map(int16_t rx, int16_t ry, int16_t &sx, int16_t &sy) const
  {
    if (_swapXY)  { int16_t t = rx; rx = ry; ry = t; }
    if (_mirrorX) rx = (_w - 1) - rx;
    if (_mirrorY) ry = (_h - 1) - ry;
    rx = constrain(rx, 0, _w - 1);
    ry = constrain(ry, 0, _h - 1);

    switch (_rot) {
      default:
      case 0: sx = rx;              sy = ry;              break;
      case 1: sx = ry;              sy = (_w - 1) - rx;   break;
      case 2: sx = (_w - 1) - rx;   sy = (_h - 1) - ry;   break;
      case 3: sx = (_h - 1) - ry;   sy = rx;              break;
    }
  }

  TwoWire *_wire = nullptr;
  uint16_t _w, _h;
  uint8_t  _addr;
  int      _rst = -1, _int = -1;
  uint8_t  _rot = 0;
  bool     _mirrorX = true, _mirrorY = false, _swapXY = false;
  bool     _polling = false, _debug = false, _ok = false;

  volatile bool _intFlag = false;
  Point    _last[MAX_POINTS];
  uint8_t  _lastCount = 0;
  uint32_t _lastReadMs = 0;
};
