# AXS5106L

PlatformIO / Arduino library for the **Waveshare ESP32-C6-Touch-LCD-1.47**:

- **`AXS5106L.h`** – standalone driver for the AXS5106L capacitive touch controller (I2C `0x63`). Uses only `Wire`; no LovyanGFX dependency. Supports INT-driven or polled reads, rotation matching LovyanGFX `setRotation()`, mirror/swap, and a raw-byte debug dump.
- **`LGFX_Waveshare_C6_Touch_147.hpp`** – LovyanGFX device (`class LGFX`) for the board's JD9853 172×320 IPS panel, with Waveshare's vendor init sequence, 80 MHz SPI and PWM backlight.

## Board pin map

| Function | Pins |
|---|---|
| SPI (shared with TF card) | SCLK 1, MOSI 2, MISO 3 |
| LCD | CS 14, DC 15, RST 22, BL 23 |
| Touch I2C | SDA 18, SCL 19, RST 20, INT 21 |

## Install

`platformio.ini`:

```ini
lib_deps =
    https://github.com/JasonBolbach/AXS5106L.git
    lovyan03/LovyanGFX@^1.2.36
```

The board works with the pioarduino platform and `board = esp32-c6-devkitc-1`. See [examples/RGBTouch/platformio.ini](examples/RGBTouch/platformio.ini) for a complete env.

## Usage

```cpp
#include <LGFX_Waveshare_C6_Touch_147.hpp>
#include <AXS5106L.h>

LGFX lcd;
AXS5106L touch;              // 172x320, addr 0x63

void setup() {
  lcd.init();
  lcd.setRotation(0);
  touch.begin(Wire, 18, 19, 20, 21);   // SDA, SCL, RST, INT
  touch.setRotation(lcd.getRotation());
}

void loop() {
  AXS5106L::Point p;
  if (touch.read(p)) lcd.fillCircle(p.x, p.y, 3, TFT_WHITE);
}
```

Useful extras: `touch.readId(id)` to confirm communication, `touch.setPolling(true)` to ignore INT, `touch.setDebug(true)` to print raw report bytes, and `read(Point *pts, n)` for up to two touch points.

## Examples

- **RGBTouch** – drag red/green/blue sliders to mix a colour; the swatch shows the hex and RGB565 values. Tap the swatch to reset. Open `examples/RGBTouch` as a PlatformIO project to build it directly.

## License

MIT
