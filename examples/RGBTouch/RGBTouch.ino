// RGBTouch - AXS5106L + LovyanGFX demo for the Waveshare ESP32-C6-Touch-LCD-1.47
//
// Drag the red, green and blue sliders to mix a colour. The swatch at the top
// shows the result and its RGB565 / RGB888 values. Tap the swatch to reset.
//
// Pins (fixed by the board):
//   Touch I2C : SDA 18, SCL 19, RST 20, INT 21  (AXS5106L @ 0x63)
//   LCD SPI   : see LGFX_Waveshare_C6_Touch_147.hpp

#include <Arduino.h>
#include <LGFX_Waveshare_C6_Touch_147.hpp>
#include <AXS5106L.h>

static LGFX         lcd;
static LGFX_Sprite  canvas(&lcd);
static AXS5106L     touch;        // 172x320 native, addr 0x63

// ---------------------------------------------------------------------------
// Layout (portrait, 172 x 320)
// ---------------------------------------------------------------------------
static constexpr int SWATCH_H  = 116;
static constexpr int MARGIN    = 10;
static constexpr int SLIDER_Y0 = SWATCH_H + 10;
static constexpr int SLIDER_H  = 64;      // row pitch
static constexpr int TRACK_H   = 26;

struct Slider {
  const char *label;
  uint8_t     shiftR, shiftG, shiftB;     // which channel this slider drives
  uint8_t     value;
};

static Slider sliders[3] = {
  { "R", 1, 0, 0, 255 },
  { "G", 0, 1, 0, 128 },
  { "B", 0, 0, 1,  32 },
};

static int  activeSlider = -1;            // slider grabbed at touch-down
static bool wasTouched   = false;
static AXS5106L::Point lastPt;

static int trackX()     { return MARGIN; }
static int trackW()     { return lcd.width() - 2 * MARGIN; }
static int rowY(int i)  { return SLIDER_Y0 + i * SLIDER_H; }
static int trackY(int i){ return rowY(i) + 20; }

static uint32_t mixedColor()
{
  return lcd.color888(sliders[0].value, sliders[1].value, sliders[2].value);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
static void drawSwatch()
{
  uint8_t r = sliders[0].value, g = sliders[1].value, b = sliders[2].value;
  canvas.fillRoundRect(MARGIN, MARGIN, trackW(), SWATCH_H - MARGIN, 10, mixedColor());
  canvas.drawRoundRect(MARGIN, MARGIN, trackW(), SWATCH_H - MARGIN, 10, TFT_DARKGREY);

  // Pick black or white text depending on perceived brightness
  uint32_t luma = (r * 299 + g * 587 + b * 114) / 1000;
  canvas.setTextColor(luma > 140 ? TFT_BLACK : TFT_WHITE);
  canvas.setTextDatum(middle_center);

  int cx = lcd.width() / 2;
  int cy = MARGIN + (SWATCH_H - MARGIN) / 2;
  char buf[24];
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  canvas.drawString(buf, cx, cy - 10);

  canvas.setFont(&fonts::Font2);
  snprintf(buf, sizeof(buf), "RGB565 0x%04X", lcd.color565(r, g, b));
  canvas.drawString(buf, cx, cy + 18);
}

static void drawSlider(int i)
{
  const Slider &s = sliders[i];
  int x = trackX(), y = trackY(i), w = trackW();

  // Label + value
  canvas.setFont(&fonts::Font2);
  canvas.setTextDatum(top_left);
  uint32_t labelColor = lcd.color888(s.shiftR * 255, s.shiftG * 255, s.shiftB * 255);
  canvas.setTextColor(labelColor);
  canvas.drawString(s.label, x, rowY(i));
  canvas.setTextDatum(top_right);
  canvas.setTextColor(i == activeSlider ? TFT_WHITE : TFT_LIGHTGREY);
  canvas.drawNumber(s.value, x + w, rowY(i));

  // Gradient track: 0 -> full intensity of this channel
  for (int dx = 0; dx < w; ++dx) {
    uint8_t v = dx * 255 / (w - 1);
    canvas.drawFastVLine(x + dx, y, TRACK_H,
                         lcd.color888(v * s.shiftR, v * s.shiftG, v * s.shiftB));
  }
  canvas.drawRect(x, y, w, TRACK_H, TFT_DARKGREY);

  // Knob
  int kx = x + s.value * (w - 1) / 255;
  canvas.fillRoundRect(kx - 4, y - 4, 9, TRACK_H + 8, 3, TFT_WHITE);
  canvas.drawRoundRect(kx - 4, y - 4, 9, TRACK_H + 8, 3, TFT_BLACK);
}

static void drawFrame(bool touched)
{
  canvas.fillScreen(TFT_BLACK);
  drawSwatch();
  for (int i = 0; i < 3; ++i) drawSlider(i);

  if (touched) {                          // touch indicator
    canvas.drawCircle(lastPt.x, lastPt.y, 12, TFT_YELLOW);
    canvas.fillCircle(lastPt.x, lastPt.y, 3, TFT_YELLOW);
  }
  canvas.pushSprite(0, 0);
}

// ---------------------------------------------------------------------------
// Touch handling
// ---------------------------------------------------------------------------
static int sliderAt(int y)
{
  for (int i = 0; i < 3; ++i)
    if (y >= rowY(i) && y < rowY(i) + SLIDER_H) return i;
  return -1;
}

static void handleTouch(const AXS5106L::Point &p, bool touchDown)
{
  if (touchDown) {
    activeSlider = sliderAt(p.y);
    if (activeSlider < 0 && p.y < SWATCH_H) {   // tap swatch -> reset
      sliders[0].value = sliders[1].value = sliders[2].value = 128;
    }
  }
  if (activeSlider >= 0) {                      // drag keeps the grabbed slider
    int v = (p.x - trackX()) * 255 / (trackW() - 1);
    sliders[activeSlider].value = constrain(v, 0, 255);
  }
}

// ---------------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);

  lcd.init();
  lcd.setRotation(0);                     // portrait 172x320
  lcd.setBrightness(160);

  if (touch.begin(Wire, 18, 19, 20, 21)) {
    uint8_t id[3];
    if (touch.readId(id))
      Serial.printf("AXS5106L ID: %02X %02X %02X\n", id[0], id[1], id[2]);
  } else {
    Serial.println("AXS5106L not responding at 0x63");
  }
  touch.setRotation(lcd.getRotation());

  canvas.setColorDepth(16);
  canvas.createSprite(lcd.width(), lcd.height());

  drawFrame(false);
}

void loop()
{
  AXS5106L::Point p;
  bool touched = touch.read(p);

  if (touched) {
    lastPt = p;
    handleTouch(p, !wasTouched);
  } else if (wasTouched) {
    activeSlider = -1;
  }

  if (touched || wasTouched) drawFrame(touched);   // redraw only when needed
  wasTouched = touched;

  delay(10);
}
