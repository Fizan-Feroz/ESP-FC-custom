#pragma once

#include <cstdint>
#include <cstddef>
#include "InputDevice.h"

namespace Espfc {

namespace Device {

class InputPWM: public InputDevice
{
  public:
    static constexpr size_t CHANNELS = 6;
    static constexpr int16_t PULSE_MIN       = 800;
    static constexpr int16_t PULSE_MAX       = 2200;
    static constexpr int16_t PULSE_NEUTRAL   = 1500;
    static constexpr uint32_t BROKEN_LINK_US = 100000UL; // 100ms

    InputPWM();
    int begin(const int8_t * pins);
    void end();

    InputStatus update() override;
    uint16_t get(uint8_t i) const override;
    void get(uint16_t * data, size_t len) const override;
    size_t getChannelCount() const override;
    bool needAverage() const override;

  private:
    struct ChannelState
    {
      volatile uint32_t rise_us;
      volatile uint16_t width_us;
      volatile bool updated;
    };

    void handleEdge(uint8_t channel);
    static void handle_isr_0(void* args);
    static void handle_isr_1(void* args);
    static void handle_isr_2(void* args);
    static void handle_isr_3(void* args);
    static void handle_isr_4(void* args);
    static void handle_isr_5(void* args);
    using IsrFn = void (*)(void*);
    static const IsrFn _isr_table[CHANNELS];

    ChannelState _state[CHANNELS];
    int8_t _pins[CHANNELS];       // channel -> pin, -1 = disabled
    uint8_t _count;
    uint32_t _last_frame_us;
};

}

}

