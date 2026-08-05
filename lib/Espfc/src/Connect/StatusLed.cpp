#include "StatusLed.hpp"
#include "Target/Target.h"
#include <Arduino.h>

#ifdef ESPFC_LED_WS2812
#include "driver/rmt.h"

// RMT-based WS2812 driver. A single ESP32 RMT channel can hold only 64 items
// (1 LED = 48 items), so each LED is written as its own chunk. The line idles
// low between chunks and the gap stays far below the ~50us reset threshold,
// so the strip never latches mid-frame.
// Timings follow the WS2812B 800kHz spec: 0 = 350ns high / 850ns low,
// 1 = 850ns high / 350ns low, frame reset >= 50us low.

static constexpr size_t LED_MAX = 32;
static constexpr size_t PIXEL_BITS = 24;
static constexpr size_t CHUNK_SIZE = PIXEL_BITS * 2 + 1; // 48 edges + reset

// APB clock 80MHz / 4 = 20MHz -> 50ns per tick
static constexpr uint32_t RMT_CLK_DIV = 4;
static constexpr uint32_t T0H = 7;     // 350ns
static constexpr uint32_t T0L = 17;    // 850ns
static constexpr uint32_t T1H = 17;    // 850ns
static constexpr uint32_t T1L = 7;     // 350ns
static constexpr uint32_t RESET = 1200; // 60us

struct ws2812_pixel_t
{
  uint8_t g;
  uint8_t r;
  uint8_t b;
};

static rmt_item32_t chunk[CHUNK_SIZE];
static ws2812_pixel_t ws_pixels[LED_MAX] = {};
static rmt_channel_t ws2812_channel = RMT_CHANNEL_0;
static bool ws2812_ready = false;

static rmt_item32_t ws2812_make_item(uint32_t level0, uint32_t duration0, uint32_t level1, uint32_t duration1)
{
  rmt_item32_t item;
  item.level0 = level0;
  item.duration0 = duration0;
  item.level1 = level1;
  item.duration1 = duration1;
  return item;
}

static void ws2812_init(int8_t pin)
{
  ws2812_channel = (rmt_channel_t)(RMT_CHANNEL_MAX - 1);
  rmt_config_t conf = RMT_DEFAULT_CONFIG_TX((gpio_num_t)pin, ws2812_channel);
  conf.clk_div = RMT_CLK_DIV;
  conf.tx_config.loop_en = false;
  conf.tx_config.carrier_en = false;
  conf.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  conf.tx_config.idle_output_en = true;
  if (rmt_config(&conf) != ESP_OK) return;
  if (rmt_driver_install(ws2812_channel, 0, 0) != ESP_OK) return;
  ws2812_ready = true;
}

static void ws2812_write_pixel(const ws2812_pixel_t& data, bool last)
{
  const uint8_t bytes[3] = { data.g, data.r, data.b };
  size_t idx = 0;
  for (size_t c = 0; c < 3; c++)
  {
    for (int32_t b = 7; b >= 0; b--)
    {
      bool one = (bytes[c] >> b) & 0x01;
      chunk[idx++] = ws2812_make_item(1, one ? T1H : T0H, 0, one ? T1L : T0L);
    }
  }
  if (last)
  {
    chunk[idx++] = ws2812_make_item(0, RESET, 0, 0);
  }
  rmt_write_items(ws2812_channel, chunk, idx, true);
}

static void ws2812_update(uint8_t count)
{
  if (!ws2812_ready) return;
  for (size_t i = 0; i < count; i++)
  {
    ws2812_write_pixel(ws_pixels[i], i + 1 == count);
  }
}

#endif

namespace Espfc::Connect {

static int LED_OFF_PATTERN[] = {0};
static int LED_OK_PATTERN[] = {100, 900, 0};
static int LED_ERROR_PATTERN[] = {100, 100, 100, 100, 100, 1500, 0};
static int LED_ON_PATTERN[] = {100, 0};

StatusLed::StatusLed()
    : _pin(-1), _type(0), _invert(0), _count(0), _groups(1), _status(LED_OFF), _next(0), _state(LOW),
      _step(0), _pattern(LED_OFF_PATTERN)
{
  static const uint8_t defaults[4][3] = { { 255, 255, 255 }, { 255, 40, 40 }, { 40, 255, 40 }, { 40, 40, 255 } };
  std::copy_n(&defaults[0][0], sizeof(defaults), &_colors[0][0]);
  std::fill_n(_colorIdle, 3, 0);
  std::fill_n(_modeColor, 3, 0);
  _modeColorOn = false;
}

void StatusLed::begin(int8_t pin, uint8_t type, uint8_t invert, uint8_t count, uint8_t groups,
                      const uint8_t (*colors)[3], const uint8_t* colorIdle)
{
  if (pin == -1) return;

  _pin = pin;
  _type = type;
  _invert = invert;
  _count = count > LED_MAX ? LED_MAX : count;
  _groups = groups == 0 ? 1 : (groups > 4 ? 4 : groups);

  if (colors)
  {
    for (size_t g = 0; g < 4; g++)
    {
      std::copy_n(colors[g], 3, _colors[g]);
    }
  }
  if (colorIdle) std::copy_n(colorIdle, 3, _colorIdle);

#ifdef ESPFC_LED_WS2812
  if (_type == LED_STRIP)
  {
    if (_count > 0) ws2812_init(_pin);
  }
  else if (_type == LED_SIMPLE) pinMode(_pin, OUTPUT);
#else
  pinMode(_pin, OUTPUT);
#endif
  setStatus(LED_ON, true);
}

void StatusLed::setStatus(LedStatus newStatus, bool force)
{
  if (_pin == -1) return;
  if (!force && newStatus == _status) return;

  _status = newStatus;
  _state = LOW;
  _step = 0;
  _next = millis();

  switch (_status)
  {
    case LED_OK:
      _pattern = LED_OK_PATTERN;
      break;
    case LED_ERROR:
      _pattern = LED_ERROR_PATTERN;
      break;
    case LED_ON:
      _pattern = LED_ON_PATTERN;
      _state = HIGH;
      break;
    case LED_OFF:
    default:
      _pattern = LED_OFF_PATTERN;
      break;
  }
  _write(_state);
}

void StatusLed::setModeColor(uint8_t r, uint8_t g, uint8_t b)
{
  if (_pin == -1) return;
  if (_modeColorOn && _modeColor[0] == r && _modeColor[1] == g && _modeColor[2] == b) return;
  _modeColor[0] = r;
  _modeColor[1] = g;
  _modeColor[2] = b;
  _modeColorOn = true;
  _write(_state);
}

void StatusLed::clearModeColor()
{
  if (_pin == -1 || !_modeColorOn) return;
  _modeColorOn = false;
  _write(_state);
}

void StatusLed::update()
{
  if (_pin == -1 || !_pattern) return;

  uint32_t now = millis();

  if (now < _next) return;

  if (!_pattern[_step])
  {
    _step = 0;
    _next = now + 20;
    return;
  }

  _state = !(_step & 1);
  _write(_state);

  _next = now + _pattern[_step];
  _step++;
}

void StatusLed::_write(uint8_t val)
{
#ifdef ESPFC_LED_WS2812
  if (_type == LED_STRIP)
  {
    if (_count == 0) return;
    for (size_t i = 0; i < _count; i++)
    {
      size_t g = (_groups * i) / _count;
      const uint8_t* c = val ? (_modeColorOn ? _modeColor : _colors[g]) : _colorIdle;
      ws_pixels[i].r = c[0];
      ws_pixels[i].g = c[1];
      ws_pixels[i].b = c[2];
    }
    ws2812_update(_count);
  }
  else if (_type == LED_SIMPLE) digitalWrite(_pin, val ^ _invert);
  else return;
#else
  digitalWrite(_pin, val ^ _invert);
#endif
}

} // namespace Espfc::Connect
