#pragma once

#include "Model.h"

namespace Espfc::Control {

class Actuator
{
  public:
    Actuator(Model& model);

    int begin();
    int update();

  #ifndef UNIT_TEST
  private:
  #endif

    void updateScaler();
    void updateArmingDisabled();
    void updateModeMask();
    bool canActivateMode(FlightMode mode);
    void updateArmed();
    void updateAirMode();
    void updateBuzzer();
    void updateDynLpf();
    void updateRescueConfig();
    void updateLed();
    void updateLedStrip();
    void updateStatusLed();

    static constexpr uint8_t MODE_DEBOUNCE_FRAMES = 25; // ~50ms at 500Hz loop
    uint8_t _modeDebounce[MODE_COUNT];

    Model& _model;
};

}
