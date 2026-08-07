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
static int LED_AIRMODE_PATTERN[] = {150, 150, 0};               // symmetric dual-color strobe
static int LED_THROTTLE_PATTERN[] = {200, 200, 0};              // steady green flash for arming-blocked throttle
static int LED_ANGLE_BLOCK_PATTERN[] = {150, 350, 0};           // blue flash: ANGLE-mode small-angle arming block
static const uint8_t LED_THROTTLE_COLOR[3] = { 0, 255, 0 };
static const uint8_t LED_ANGLE_BLOCK_COLOR[3] = { 0, 0, 255 };
static const uint8_t LED_AIRMODE_FRONT_COLOR[3] = { 255, 0, 0 }; // red
static const uint8_t LED_AIRMODE_BACK_COLOR[3] = { 0, 0, 255 };  // blue

// Arm flash: full-bright solid flash for this long after arming.
static constexpr uint32_t ARM_FLASH_MS = 250;

// Disarm fade-out: strip dims to off over this time before the idle animation.
static constexpr uint32_t DISARM_FADE_MS = 400;

// Front swipe (ACRO armed) and disarmed idle: a wave travels along BOTH sides of
// the frame. All four legs' data starts at the frame center (nose for the front
// legs, tail seam for the back legs) and runs out to the tip, so a shared
// position p in [0, gs-1] lights every leg at once: front legs at pos p sweep
// center->tip (data order), back legs at pos gs-1-p sweep tip->center (reverse
// data order). ACRO uses a back-first wave: back legs sweep center->tip during
// the first SWEEP_UP_MS, then front legs sweep center->tip, as one continuous
// tail-to-nose pass; idle ping-pongs so its motion is continuous.
static constexpr uint32_t SWEEP_UP_MS = 1200;
static constexpr uint32_t SWEEP_STEP_MS = 20;
static constexpr uint32_t BAND_RADIUS = 2;
static constexpr uint32_t EDGE_FADE = 256; // 1 LED (fixed-point) fade in/out at each end of the swipe

// Idle (disarmed) color cycle: whole strip washes through hues every ~12s,
// the traveling wave uses a hue offset ahead of the wash, full bright.
static constexpr uint32_t IDLE_HUE_STEP_MS = 48;
static constexpr uint8_t IDLE_WASH = 110;
static constexpr uint8_t IDLE_BAND_OFFSET = 60;

static void hsvToRgb(uint8_t h, uint8_t s, uint8_t v, uint8_t& r, uint8_t& g, uint8_t& b)
{
  uint8_t region = h / 43;
  uint8_t rem = (uint8_t)((h % 43) * 6);
  uint8_t p = (uint8_t)(v * (255 - s) / 255);
  uint8_t q = (uint8_t)(v * (255 - ((s * rem) / 255)) / 255);
  uint8_t t = (uint8_t)(v * (255 - ((s * (255 - rem)) / 255)) / 255);
  switch (region)
  {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
  }
}

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
    case LED_AIRMODE:
      _pattern = LED_AIRMODE_PATTERN;
      _flashUntil = _next + ARM_FLASH_MS;
      break;
    case LED_SWEEP:
      _pattern = LED_ON_PATTERN;
      _state = HIGH;
      _sweepStart = _next;
      _lastSweep = _next;
      _flashUntil = _next + ARM_FLASH_MS;
      break;
    case LED_THROTTLE:
      _pattern = LED_THROTTLE_PATTERN;
      break;
    case LED_ANGLE_BLOCK:
      _pattern = LED_ANGLE_BLOCK_PATTERN;
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

  // armed ANGLE / AIRMODE: hold the arm flash solid, then run the strobe pattern
  if ((_status == LED_ANGLE || _status == LED_AIRMODE) && now < _flashUntil)
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
  // All four legs' data starts at the frame center (nose for FL/FR, tail seam
  // for BR/BL) and runs outward to the tip. A shared position p in [0, gs-1]
  // lights every leg at once: front legs at pos p sweep center->tip (data
  // order), back legs at pos gs-1-p sweep tip->center (reverse data order), so
  // all four legs sweep "forward" together.
  int32_t gs = (int32_t)_count / 4;
  int32_t pos = (int32_t)(i % (size_t)gs);
  return (i < (size_t)(2 * gs)) ? pos : gs - 1 - pos;
}

int32_t StatusLed::_sweepPos(bool backForth) const
{
  // one-way swipe or ping-pong over [0, gs-1], returned as fixed-point (1/256
  // per LED) so the band glides smoothly between LEDs; the swipe resets to the
  // start, the ping-pong turns around continuously
  int32_t max = ((int32_t)(_count / 4) - 1) * 256;
  uint32_t cycle = backForth ? 2 * SWEEP_UP_MS : SWEEP_UP_MS;
  uint32_t ph = (millis() - _sweepStart) % cycle;
  uint32_t up = (backForth && ph >= SWEEP_UP_MS) ? cycle - ph : ph;
  return (int32_t)((uint32_t)max * up / SWEEP_UP_MS);
}

int32_t StatusLed::_sweepBackToFront() const
{
  // ACRO: one global wave that sweeps the back legs first, then the front
  // legs. Fixed-point (1/256 per LED) position in [0, 2*max] over a full
  // 2*SWEEP_UP_MS cycle, so each leg-group gets a full SWEEP_UP_MS to travel
  // center->tip: back legs use pos directly, front legs use pos - max.
  int32_t max = ((int32_t)(_count / 4) - 1) * 256;
  uint32_t cycle = 2 * SWEEP_UP_MS;
  uint32_t ph = (millis() - _sweepStart) % cycle;
  return (int32_t)((uint32_t)(2 * max) * ph / cycle);
}

uint8_t StatusLed::_bandBrightness(size_t i, int32_t pos) const
{
  // smooth falloff over BAND_RADIUS LEDs; pos and the LED positions are
  // fixed-point (1/256), so the band's intensity glides as it moves
  int32_t dist = _frontPos(i) * 256 - pos;
  if (dist < 0) dist = -dist;
  int32_t radius = (int32_t)BAND_RADIUS * 256;
  if (dist >= radius) return 0;
  float t = (float)dist / (float)radius;
  float b = 1.0f - t * t;
  b *= b; // (1 - t^2)^2: 1 at center, 0 at the edge, smooth at both
  return (uint8_t)(b * 255.0f + 0.5f);
}

void StatusLed::_render(uint8_t val)
{
  _lastVal = (_status == LED_ANGLE || _status == LED_AIRMODE) ? 1 : val;
  _lastAlpha = _alpha;
#ifdef ESPFC_LED_WS2812
  if (_type == LED_STRIP)
  {
    if (_count == 0) return;

    uint32_t now = millis();

    // disarmed idle: dim wash cycling through hues + bright hue-offset wave
    if (!_fadingOut && _status == LED_IDLE)
    {
      int32_t pos = _sweepPos(true);
      uint8_t hue = (uint8_t)(now / IDLE_HUE_STEP_MS);
      uint8_t wr, wg, wb, cr, cg, cb;
      hsvToRgb(hue, 255, IDLE_WASH, wr, wg, wb);
      hsvToRgb(hue + IDLE_BAND_OFFSET, 255, 255, cr, cg, cb);
      for (size_t i = 0; i < _count; i++)
      {
        uint8_t band = _bandBrightness(i, pos);
        uint32_t rm = ((uint32_t)wr * (255 - band) + (uint32_t)cr * band) / 255;
        uint32_t gm = ((uint32_t)wg * (255 - band) + (uint32_t)cg * band) / 255;
        uint32_t bm = ((uint32_t)wb * (255 - band) + (uint32_t)cb * band) / 255;
        ws_pixels[i].r = (uint16_t)rm * _alpha / 255;
        ws_pixels[i].g = (uint16_t)gm * _alpha / 255;
        ws_pixels[i].b = (uint16_t)bm * _alpha / 255;
      }
      ws2812_update(_count);
      return;
    }

    bool sweep = false;
    int32_t pos = 0;
    if (!_fadingOut && _status == LED_SWEEP && now >= _flashUntil)
    {
      sweep = true;
      pos = _sweepBackToFront();
    }

    for (size_t i = 0; i < _count; i++)
    {
      size_t g = (_groups * i) / _count;
      bool on = val;
      if (_status == LED_ANGLE)
      {
        // back legs stay solid, front legs flash together per val
        on = (g >= 2) || (val || now < _flashUntil);
      }
      else if (_status == LED_AIRMODE)
      {
        // airmode: whole strip strobes together; front legs cyan, back legs magenta
        on = (val || now < _flashUntil);
      }
      const uint8_t* c = on ? _colors[g] : _colorIdle;
      if (_status == LED_THROTTLE) c = on ? LED_THROTTLE_COLOR : _colorIdle;
      if (_status == LED_ANGLE_BLOCK) c = on ? LED_ANGLE_BLOCK_COLOR : _colorIdle;
      if (_status == LED_AIRMODE) c = on ? ((g >= 2) ? LED_AIRMODE_BACK_COLOR : LED_AIRMODE_FRONT_COLOR) : _colorIdle;
      uint32_t mult = _alpha;
      if (sweep)
      {
        // back legs lead (band at pos), front legs follow (band at pos - max);
        // both sweep center->tip in their own half of the 2*SWEEP_UP_MS cycle
        int32_t maxPos = ((int32_t)(_count / 4) - 1) * 256;
        int32_t band = (g >= 2) ? pos : pos - maxPos;
        uint32_t b = _bandBrightness(i, band);
        uint32_t fade = 255;
        if (pos < (int32_t)EDGE_FADE)
          fade = (uint32_t)pos * 255 / EDGE_FADE;
        else if (pos > 2 * maxPos - (int32_t)EDGE_FADE)
          fade = (uint32_t)(2 * maxPos - pos) * 255 / EDGE_FADE;
        b = b * fade / 255;
        mult = (uint32_t)_alpha * b / 255;
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
