# PWM Receiver Wiring (FlySky FS-R6B)

Parallel PWM receiver input on an ESP32 flight controller. Each receiver channel is a separate signal wire on its own GPIO.

## Pin connections

| FS-R6B pin   | Function | ESP32 GPIO | Channel |
|--------------|----------|-----------|---------|
| CH1          | Aileron  | 32        | Roll    |
| CH2          | Elevator | 33        | Pitch   |
| CH3          | Throttle | 34        | Throttle |
| CH4          | Rudder   | 35        | Yaw     |
| CH5          | Aux 1    | 26        | Aux1    |
| CH6          | Aux 2    | 14        | Aux2    |
| VCC          | Power    | 5V        |         |
| GND          | Ground   | GND       |         |

Notes:
- FS-R6B PWM outputs are 3.3 V logic, so they can connect directly to the ESP32 (no level shifter / voltage divider).
- Power the receiver from the FC 5 V BEC, not the 3.3 V rail (the FS-R6B needs at least 4 V).
- A common ground between receiver, ESP32 and any ESC/battery is required.

## Firmware configuration

Enable the PWM input device and assign the pins. This is done in the CLI tab (there is no GUI equivalent for these esp-fc specific params).

```
set feature_rx_pwm 1
set pin_input_pwm_0 32
set pin_input_pwm_1 33
set pin_input_pwm_2 34
set pin_input_pwm_3 35
set pin_input_pwm_4 26
set pin_input_pwm_5 14
set serial_1 0 0 0
set pin_buzzer -1
save
```

Then reboot the board. The input device is selected at boot, so a power cycle is required after changing pins or features.

## Betaflight Configurator

1. Open the **Receiver** tab.
2. Set **Receiver Mode** to **Parallel PWM** (this maps to `feature_rx_pwm`).
3. The channel bars should respond to the sticks (~1000-2000).
4. Use **Save & Reboot**.

## Channel mapping

| pin_input_pwm | Receiver CH | Control |
|---------------|-------------|---------|
| pin_input_pwm_0 | CH1 | Roll |
| pin_input_pwm_1 | CH2 | Pitch |
| pin_input_pwm_2 | CH3 | Throttle |
| pin_input_pwm_3 | CH4 | Yaw |
| pin_input_pwm_4 | CH5 | Aux1 |
| pin_input_pwm_5 | CH6 | Aux2 |

If a stick lands on the wrong axis, use the **Channel Map** box in the Configurator or swap the signal wires.

## I2C sensors (MPU6050 + BMP180)

Both sensors are supported and auto-detected on the I2C bus (MPU6050 gyro/accel at 0x68, BMP180 barometer via the BMP085 driver at 0x77).

```
MPU6050        ESP32
VCC      ->    3.3V
GND      ->    GND
SDA      ->    GPIO 21
SCL      ->    GPIO 22
AD0      ->    GND (or float = address 0x68)

BMP180         ESP32
VCC      ->    3.3V
GND      ->    GND
SDA      ->    GPIO 21   (shared bus)
SCL      ->    GPIO 22   (shared bus)
```

- No configuration needed: `gyro_bus`, `gyro_dev`, `baro_bus`, `baro_dev` default to AUTO and probe the bus at boot.
- Verify in the Configurator **Setup tab** - the model preview should tilt with the board. Calibrate the accelerometer with the board level.
- The I2C bus (GPIO 21/22) does not conflict with the PWM input pins or the buzzer pin.

## Notes and pitfalls

- GPIO 26 is the default buzzer pin on the ESP32 target. `set pin_buzzer -1` frees it for PWM channel 5. Otherwise the buzzer driver drives it as an output and the channel will not read.
- GPIO 32/33 are the default Serial1 (MSP) pins. `set serial_1 0 0 0` disables that port so the pins can be used as PWM input. Serial0 (USB) keeps working for the CLI and configurator.
- GPIO 34/35 are input-only pins (no internal pullup) - fine for PWM input because the receiver drives them actively.
- Avoid strapping pins at boot: GPIO 0, 2, 12, 15.
- The receiver must be powered and bound to its transmitter, otherwise `update()` reports signal lost after 100 ms.
