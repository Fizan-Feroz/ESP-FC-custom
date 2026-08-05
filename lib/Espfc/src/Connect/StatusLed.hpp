#pragma once

#include <cstddef>
#include <cstdint>

namespace Espfc::Connect {

enum LedType
{
  LED_SIMPLE,
  LED_STRIP,
};

enum LedStatus
{
  LED_OFF,
  LED_OK,
  LED_ERROR,
  LED_ON,
  LED_FADE,
  LED_SWEEP,
};

class StatusLed
{
public:
  StatusLed();
  void begin(int8_t pin, uint8_t type, uint8_t invert, uint8_t count = 0, uint8_t groups = 1,
             const uint8_t (*colors)[3] = nullptr, const uint8_t* colorIdle = nullptr);
  void update();
  void setStatus(LedStatus newStatus, bool force = false);

private:
  void _render(uint8_t val);
  void _write(uint8_t val);
  int32_t _orbitDist(size_t i) const;
  uint8_t _bandBrightness(size_t i, int32_t center) const;
  int8_t _pin;
  uint8_t _type;
  uint8_t _invert;
  uint8_t _count;
  uint8_t _groups;
  uint8_t _colors[4][3];
  uint8_t _colorIdle[3];
  uint8_t _alpha;
  uint32_t _fadeStart;
  uint32_t _lastFade;
  uint32_t _sweepStart;
  uint32_t _lastSweep;
  uint32_t _flashUntil;
  uint32_t _disarmStart;
  uint8_t _disarmFromAlpha;
  uint8_t _lastVal;
  uint8_t _lastAlpha;
  LedStatus _status;
  uint32_t _next;
  bool _state;
  size_t _step;
  int* _pattern;
};

} // namespace Espfc::Connect
