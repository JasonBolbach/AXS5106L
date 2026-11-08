#pragma once
// LovyanGFX configuration for the Waveshare ESP32-C6-Touch-LCD-1.47
//
// Pin map (from Waveshare's factory firmware BSP):
//   SPI  : SCLK 1, MOSI 2, MISO 3          (shared with the TF card)
//   LCD  : CS 14, DC 15, RST 22, BL 23     (JD9853, 172x320, x offset 34)
//   I2C  : SDA 18, SCL 19                  (AXS5106L touch @0x63, QMI8658 IMU)
//   Touch: INT 21, RST 20

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

namespace lgfx {
inline namespace v1 {

// ---------------------------------------------------------------------------
// JD9853 with Waveshare's vendor init sequence for this 172x320 panel.
// (LovyanGFX's built-in Panel_JD9853 carries the init for M5's 240x240 panel,
//  whose power/gamma settings differ.)
// ---------------------------------------------------------------------------
struct Panel_JD9853_WS147 : public Panel_JD9853
{
protected:
  const uint8_t* getInitCommands(uint8_t listno) const override
  {
    static constexpr uint8_t list0[] = {
      0x11, 0 + CMD_INIT_DELAY, 120,               // Sleep out
      0xDF, 2, 0x98, 0x53,                         // Unlock vendor commands
      0xDF, 2, 0x98, 0x53,
      0xB2, 1, 0x23,
      0xB7, 4, 0x00, 0x47, 0x00, 0x6F,
      0xBB, 6, 0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0,
      0xC0, 2, 0x44, 0xA4,
      0xC1, 1, 0x16,
      0xC3, 8, 0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,
      0xC4, 12, 0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,
      0xC8, 32,                                    // Gamma
        0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
        0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
        0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
        0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
      0xD0, 5, 0x04, 0x06, 0x6B, 0x0F, 0x00,
      0xD7, 2, 0x00, 0x30,
      0xE6, 1, 0x14,
      0xDE, 1, 0x01,                               // Page 1
      0xB7, 5, 0x03, 0x13, 0xEF, 0x35, 0x35,
      0xC1, 3, 0x14, 0x15, 0xC0,
      0xC2, 2, 0x06, 0x3A,
      0xC4, 2, 0x72, 0x12,
      0xBE, 1, 0x00,
      0xDE, 1, 0x02,                               // Page 2
      0xE5, 3, 0x00, 0x02, 0x00,
      0xE5, 3, 0x01, 0x02, 0x00,
      0xDE, 1, 0x00,                               // Page 0
      0x35, 1, 0x00,                               // TE on
      0x3A, 1, 0x05,                               // RGB565
      0x2A, 4, 0x00, 0x22, 0x00, 0xCD,             // X 34..205
      0x2B, 4, 0x00, 0x00, 0x01, 0x3F,             // Y 0..319
      0xDE, 1, 0x02,
      0xE5, 3, 0x00, 0x02, 0x00,
      0xDE, 1, 0x00,
      0x29, 0 + CMD_INIT_DELAY, 10,                // Display on
      0xFF, 0xFF,
    };
    return listno == 0 ? list0 : nullptr;
  }
};

// Touch (AXS5106L) is handled by the standalone driver in AXS5106L.h.

} // namespace v1
} // namespace lgfx

// ---------------------------------------------------------------------------
// Device definition
// ---------------------------------------------------------------------------
class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_JD9853_WS147 _panel;
  lgfx::Bus_SPI            _bus;
  lgfx::Light_PWM          _light;

public:
  LGFX(void)
  {
    { // SPI bus
      auto cfg = _bus.config();
      cfg.spi_host    = SPI2_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 80000000;   // Waveshare's demo runs the panel at 80 MHz
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = 1;
      cfg.pin_mosi    = 2;
      cfg.pin_miso    = 3;
      cfg.pin_dc      = 15;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    { // Panel
      auto cfg = _panel.config();
      cfg.pin_cs           = 14;
      cfg.pin_rst          = 22;
      cfg.pin_busy         = -1;
      cfg.memory_width     = 240;
      cfg.memory_height    = 320;
      cfg.panel_width      = 172;
      cfg.panel_height     = 320;
      cfg.offset_x         = 34;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 16;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = true;
      cfg.invert           = true;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true;   // TF card sits on the same SPI bus
      _panel.config(cfg);
    }

    { // Backlight
      auto cfg = _light.config();
      cfg.pin_bl      = 23;
      cfg.invert      = false;
      cfg.freq        = 5000;
      cfg.pwm_channel = 0;
      _light.config(cfg);
      _panel.setLight(&_light);
    }

    setPanel(&_panel);
  }
};
