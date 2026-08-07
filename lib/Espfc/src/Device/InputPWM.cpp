#include "Device/InputPWM.h"
#include <Arduino.h>
#include "Utils/MemoryHelper.h"
#include "Hal/Gpio.h"

namespace Espfc {

namespace Device {

InputPWM::InputPWM(): _count(0), _last_frame_us(0)
{
  for(size_t i = 0; i < CHANNELS; i++)
  {
    _pins[i] = -1;
    _state[i].rise_us = 0;
    _state[i].width_us = PULSE_NEUTRAL;
    _state[i].updated = false;
  }
}

int InputPWM::begin(const int8_t * pins)
{
  end();

  uint8_t count = 0;
  for(size_t i = 0; i < CHANNELS; i++)
  {
    if(pins[i] < 0) continue;

    _pins[i] = pins[i];
    _state[i].rise_us = 0;
    _state[i].width_us = PULSE_NEUTRAL;
    _state[i].updated = false;

    Espfc::Hal::Gpio::pinMode(_pins[i], INPUT);
#if defined(UNIT_TEST)
    // no mock available
#elif defined(ARCH_RP2040)
    attachInterruptParam(_pins[i], _isr_table[i], (PinStatus)CHANGE, this);
#else
    attachInterruptArg(digitalPinToInterrupt(_pins[i]), _isr_table[i], this, CHANGE);
#endif
    count++;
  }

  _count = count;
  _last_frame_us = micros();
  return _count;
}

void InputPWM::end()
{
  for(size_t i = 0; i < CHANNELS; i++)
  {
    if(_pins[i] >= 0)
    {
      detachInterrupt(_pins[i]);
      _pins[i] = -1;
    }
    _state[i].rise_us = 0;
    _state[i].width_us = PULSE_NEUTRAL;
    _state[i].updated = false;
  }
  _count = 0;
}

InputStatus FAST_CODE_ATTR InputPWM::update()
{
  const uint32_t now = micros();

  bool complete = true;
  for(size_t i = 0; i < CHANNELS; i++)
  {
    if(_pins[i] < 0) continue;
    if(!_state[i].updated) { complete = false; break; }
  }

  if(complete)
  {
    for(size_t i = 0; i < CHANNELS; i++)
    {
      if(_pins[i] >= 0) _state[i].updated = false;
    }
    _last_frame_us = now;
    return INPUT_RECEIVED;
  }

  if(now - _last_frame_us > BROKEN_LINK_US)
  {
    return INPUT_LOST;
  }

  return INPUT_IDLE;
}

uint16_t FAST_CODE_ATTR InputPWM::get(uint8_t i) const
{
  return (i < CHANNELS) ? _state[i].width_us : PULSE_NEUTRAL;
}

void FAST_CODE_ATTR InputPWM::get(uint16_t * data, size_t len) const
{
  const size_t n = len > CHANNELS ? CHANNELS : len;
  for(size_t i = 0; i < n; i++)
  {
    data[i] = _state[i].width_us;
  }
}

size_t InputPWM::getChannelCount() const { return _count; }

bool InputPWM::needAverage() const { return false; }

void IRAM_ATTR InputPWM::handleEdge(uint8_t channel)
{
  const int8_t pin = _pins[channel];
  if(pin < 0) return;

  const uint32_t now = micros();
  if(Espfc::Hal::Gpio::digitalRead(pin))
  {
    _state[channel].rise_us = now;
  }
  else
  {
    const uint32_t width = now - _state[channel].rise_us;
    if(width >= PULSE_MIN && width <= PULSE_MAX)
    {
      _state[channel].width_us = width;
      _state[channel].updated = true;
    }
  }
}

void IRAM_ATTR InputPWM::handle_isr_0(void* args)
{
  if(args) reinterpret_cast<InputPWM*>(args)->handleEdge(0);
}

void IRAM_ATTR InputPWM::handle_isr_1(void* args)
{
  if(args) reinterpret_cast<InputPWM*>(args)->handleEdge(1);
}

void IRAM_ATTR InputPWM::handle_isr_2(void* args)
{
  if(args) reinterpret_cast<InputPWM*>(args)->handleEdge(2);
}

void IRAM_ATTR InputPWM::handle_isr_3(void* args)
{
  if(args) reinterpret_cast<InputPWM*>(args)->handleEdge(3);
}

void IRAM_ATTR InputPWM::handle_isr_4(void* args)
{
  if(args) reinterpret_cast<InputPWM*>(args)->handleEdge(4);
}

void IRAM_ATTR InputPWM::handle_isr_5(void* args)
{
  if(args) reinterpret_cast<InputPWM*>(args)->handleEdge(5);
}

const InputPWM::IsrFn InputPWM::_isr_table[InputPWM::CHANNELS] = {
  &InputPWM::handle_isr_0,
  &InputPWM::handle_isr_1,
  &InputPWM::handle_isr_2,
  &InputPWM::handle_isr_3,
  &InputPWM::handle_isr_4,
  &InputPWM::handle_isr_5,
};

}

}
