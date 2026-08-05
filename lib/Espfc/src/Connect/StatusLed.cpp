#include "StatusLed.hpp"
#include "Target/Target.h"
#include <Arduino.h>

#ifdef ESPFC_LED_WS2812
#include "driver/rmt.h"

// RMT-based WS2812 driver. With the RMT driver installed, rmt_write_items does
// not copy the item data: it points at our array and the driver ISR streams it
// into the channel memory block (64 items) as it drains, so an arbitrary-size
// frame is supported in a single non-blocking call. We build the whole frame
// (all pixels + reset) up front and guard against modifying the array while a
// transmission is still in flight.
// Timings follow the WS2812B 800kHz spec: 0 = 350ns high / 850ns low,
// 1 = 850ns high / 350ns low, frame reset >= 50us low.

static constexpr size_t LED_MAX = 32;
static constexpr size_t PIXEL_BITS = 24;
static constexpr size_t FRAME_MAX = LED_MAX * PIXEL_BITS * 2 + 1; // 48 edges/LED + reset

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

static rmt_item32_t frame[FRAME_MAX];
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

static void ws2812_update(uint8_t count)
{
  if (!ws2812_ready) return;
  // Never rebuild the frame while the previous non-blocking transmission is
  // still streaming from it. Writes are spaced >= 20ms apart and a full frame
  // takes ~1.5ms, so this only ever trips on a bug.
  if (rmt_wait_tx_done(ws2812_channel, 0) != ESP_OK) return;

  size_t idx = 0;
  for (size_t i = 0; i < count; i++)
  {
    const uint8_t bytes[3] = { ws_pixels[i].g, ws_pixels[i].r, ws_pixels[i].b };
    for (size_t c = 0; c < 3; c++)
    {
      for (int32_t b = 7; b >= 0; b--)
      {
        bool one = (bytes[c] >> b) & 0x01;
        frame[idx++] = ws2812_make_item(1, one ? T1H : T0H, 0, one ? T1L : T0L);
      }
    }
  }
  frame[idx++] = ws2812_make_item(0, RESET, 0, 0);
  rmt_write_items(ws2812_channel, frame, idx, false);
}

#endif

namespace Espfc::Connect {

static int LED_OFF_PATTERN[] = {0};
static int LED_OK_PATTERN[] = {100, 900, 0};
static int LED_ERROR_PATTERN[] = {100, 100, 100, 100, 100, 1500, 0};
static int LED_ON_PATTERN[] = {100, 0};
static int LED_FRONT_FLASH_PATTERN[] = {150, 150, 150, 500, 0}; // asymmetric double-flash strobe

// Arm flash: full-bright solid flash for this long after arming.
static constexpr uint32_t ARM_FLASH_MS = 250;

// Disarm fade-out: strip dims to off over this time before the idle animation.
static constexpr uint32_t DISARM_FADE_MS = 400;

// Front swipe (ACRO armed): a wave starts at the back of the frame and sweeps
// forward along BOTH sides at once, holding bright at the nose for a moment so
// the front direction is obvious. Frontness f runs 0 (back seam) to 2*gs-1
// (front seam between the two front legs). Both sides share the same f, so a
// band at position pos lights the back legs + front legs together toward front.
// Disarmed (LED_IDLE) reuses the geometry as a smooth back-forth ping-pong with
// a dim color baseline so the group colors stay faintly visible.
static constexpr uint32_t SWEEP_UP_MS = 1200;
static constexpr uint32_t SWEEP_HOLD_MS = 400;
static constexpr uint32_t SWEEP_STEP_MS = 20;
static constexpr uint32_t BAND_RADIUS = 2;
static constexpr uint32_t IDLE_BASELINE = 40;

StatusLed::StatusLed()
    : _pin(-1), _type(0), _invert(0), _count(0), _groups(1), _alpha(255),
      _sweepStart(0), _lastSweep(0), _flashUntil(0), _disarmStart(0), _disarmFromAlpha(0),
      _lastVal(0), _lastAlpha(0), _fadingOut(false), _status(LED_OFF), _next(0), _state(LOW),
      _step(0), _pattern(LED_OFF_PATTERN)
{
  static const uint8_t defaults[4][3] = { { 255, 255, 255 }, { 255, 40, 40 }, { 40, 255, 40 }, { 40, 40, 255 } };
  std::copy_n(&defaults[0][0], sizeof(defaults), &_colors[0][0]);
  std::fill_n(_colorIdle, 3, 0);
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

  uint8_t prevAlpha = _alpha;
  uint8_t prevVal = _lastVal;

  _status = newStatus;
  _state = LOW;
  _step = 0;
  _next = millis();
  _alpha = 255;

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
    case LED_ANGLE:
      _pattern = LED_FRONT_FLASH_PATTERN;
      _flashUntil = _next + ARM_FLASH_MS;
      break;
    case LED_SWEEP:
      _pattern = LED_ON_PATTERN;
      _state = HIGH;
      _sweepStart = _next;
      _lastSweep = _next;
      _flashUntil = _next + ARM_FLASH_MS;
      break;
    case LED_IDLE:
      _pattern = LED_ON_PATTERN;
      _state = HIGH;
      _sweepStart = _next;
      _lastSweep = _next;
      // fading out of an armed state? dim the last frame before the idle animation.
      if (prevVal)
      {
        _disarmStart = _next;
        _disarmFromAlpha = prevAlpha;
        return;
      }
      break;
    case LED_OFF:
    default:
      _pattern = LED_OFF_PATTERN;
      break;
  }
  _write(_state);
}

void StatusLed::update()
{
  if (_pin == -1 || !_pattern) return;

  uint32_t now = millis();

  // disarm fade-out: dim the last armed frame to off before the idle animation
  if (_status == LED_IDLE && _disarmStart)
  {
    uint32_t dt = now - _disarmStart;
    if (dt >= DISARM_FADE_MS)
    {
      _disarmStart = 0;
      _alpha = 255;
      _write(_state);
      return;
    }
    _alpha = _disarmFromAlpha - (dt * _disarmFromAlpha) / DISARM_FADE_MS;
    _fadingOut = true;
    _render(HIGH);
    _fadingOut = false;
    return;
  }

  // armed ACRO: front swipe
  if (_status == LED_SWEEP)
  {
    if (now - _lastSweep < SWEEP_STEP_MS) return;
    _lastSweep = now;
    _render(_state);
    return;
  }

  // disarmed: fancy idle animation (back-forth ping-pong wave)
  if (_status == LED_IDLE)
  {
    if (now - _lastSweep < SWEEP_STEP_MS) return;
    _lastSweep = now;
    _render(_state);
    return;
  }

  // armed ANGLE: hold the arm flash solid, then run the front flash pattern
  if (_status == LED_ANGLE && now < _flashUntil)
  {
    _render(HIGH);
    return;
  }

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
  _render(val);
}

int32_t StatusLed::_frontPos(size_t i) const
{
  // Position along the back->front path of each side, in steps. f=0 is the back
  // seam (where the two back legs meet), f=2*gs-1 is the front seam (between the
  // two front legs). Left side runs BL then FL, right side runs BR then FR, both
  // ending at the front seam. gs = LEDs per leg = count / 4.
  int32_t gs = (int32_t)_count / 4;
  if (i < (size_t)gs)       return (int32_t)i + gs;       // FL, front end at i=gs-1
  if (i < (size_t)(2 * gs)) return 3 * gs - 1 - (int32_t)i; // FR, front end at i=gs
  if (i < (size_t)(3 * gs)) return 3 * gs - 1 - (int32_t)i; // BR, front end at i=2gs
  return (int32_t)i - 3 * gs;                              // BL, front end at i=4gs-1
}

int32_t StatusLed::_sweepPos() const
{
  uint32_t now = millis();
  int32_t frontMax = 2 * (int32_t)(_count / 4) - 1;
  if (_status == LED_SWEEP)
  {
    // one-way ramp to the nose, hold, then restart
    uint32_t cycle = SWEEP_UP_MS + SWEEP_HOLD_MS;
    uint32_t ph = (now - _sweepStart) % cycle;
    return (ph < SWEEP_UP_MS) ? (int32_t)(ph * frontMax / SWEEP_UP_MS) : frontMax;
  }
  // LED_IDLE: smooth back-forth ping-pong
  uint32_t cycle = 2 * SWEEP_UP_MS;
  uint32_t ph = (now - _sweepStart) % cycle;
  return (ph < SWEEP_UP_MS) ? (int32_t)(ph * frontMax / SWEEP_UP_MS)
                            : (int32_t)((2 * SWEEP_UP_MS - ph) * frontMax / SWEEP_UP_MS);
}

uint8_t StatusLed::_bandBrightness(size_t i, int32_t pos) const
{
  int32_t dist = _frontPos(i) - pos;
  if (dist < 0) dist = -dist;
  if (dist > (int32_t)BAND_RADIUS) return 0;
  return (uint8_t)((BAND_RADIUS - dist) * 255 / BAND_RADIUS);
}

void StatusLed::_render(uint8_t val)
{
  _lastVal = (_status == LED_ANGLE) ? 1 : val;
  _lastAlpha = _alpha;
#ifdef ESPFC_LED_WS2812
  if (_type == LED_STRIP)
  {
    if (_count == 0) return;

    bool sweep = false;
    int32_t pos = 0;
    uint32_t baseline = 0;
    if (!_fadingOut && _status == LED_SWEEP && millis() >= _flashUntil)
    {
      sweep = true;
      pos = _sweepPos();
    }
    else if (!_fadingOut && _status == LED_IDLE)
    {
      sweep = true;
      pos = _sweepPos();
      baseline = IDLE_BASELINE;
    }

    for (size_t i = 0; i < _count; i++)
    {
      size_t g = (_groups * i) / _count;
      bool on = val;
      if (_status == LED_ANGLE)
      {
        // back legs stay solid, front legs flash together per val
        on = (g >= 2) || (val || millis() < _flashUntil);
      }
      const uint8_t* c = on ? _colors[g] : _colorIdle;
      uint32_t mult = _alpha;
      if (sweep)
      {
        uint32_t band = baseline + (_bandBrightness(i, pos) * (255 - baseline)) / 255;
        mult = (uint32_t)_alpha * band / 255;
      }
      ws_pixels[i].r = (uint16_t)c[0] * mult / 255;
      ws_pixels[i].g = (uint16_t)c[1] * mult / 255;
      ws_pixels[i].b = (uint16_t)c[2] * mult / 255;
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
