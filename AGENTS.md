# ESP-FC build notes

## Hardware (current build)
- Board: LOLIN32 (ESP32 WROOM, 240 MHz)
- Only serial device: **COM3** (CP210x) @ 115200 8N1
- Gyro/accel: MPU6050 on I2C0 (SDA 21, SCL 22). Uses hardware I2C — the software bit-bang path (EspWire) is disabled because WiFi/UART/timer interrupts corrupt it (see `Target/TargetESP32.h` + `TargetEsp32Common.h`).
- Baro: BMP180 on the same I2C bus.
- WiFi: AP `ESP-FC` / 192.168.4.1, TCP-to-MSP bridge on port 1111.

## Build / flash
- `pio run -e esp32 -t upload` flashes COM3. Close the Configurator first (it holds the port).
- CLI over COM3: `& "C:\Users\fizan\AppData\Local\Temp\opencode\send-cli.ps1" -Cmds @("get ...", "set ...")`. It waits ~10 s for boot/CLI; output goes to `C:\Users\fizan\AppData\Local\Temp\opencode\cli-log.txt`.

## Current board CLI config (working)
```
set feature_rx_pwm 1
set pin_input_pwm_0 32     set pin_input_pwm_1 33     set pin_input_pwm_2 34
set pin_input_pwm_3 35     set pin_input_pwm_4 26     set pin_input_pwm_5 14
set serial_1 0 0 0         # frees GPIO 32/33 (was MSP) for PWM
set serial_2 0 115200 0    # frees GPIO 16 (was RX_SERIAL) for LED strip
set pin_buzzer -1          # frees GPIO 26 for PWM ch5
set pin_spi_cs_1 -1        # frees GPIO 13 (was SPI baro CS) for motor 4
set pin_output_3 13        # motor 4 (out0..3 = 27, 25, 4, 13)
set pin_led_strip 16       # WS2812 strip (RMT, full-frame non-blocking write)
set led_count 28
set led_groups 4
set led_mode STATUS        # POSITION | STATUS | AUX (commercial-drone behavior)
set led_color_0 255,255,255  set led_color_1 255,0,0
set led_color_2 40,255,40    set led_color_3 40,40,255
set led_idle 0,0,0
set feature_soft_serial 1  # WiFi always on at boot
```
Group → LED mapping (count 28, groups 4): LEDs 0-6 front-left, 7-13 front-right, 14-20 back-right, 21-27 back-left.
LED behavior (STATUS mode, Betaflight-style): **disarmed** = slow blink (100ms on/900ms off) of group colors; **armed + ANGLE** = steady group colors (orientation kept); **armed + ACRO** = group colors slowly fade in/out (2s period, 40ms step, dims fully to off); **failsafe or arming-blocked** = fast triple-blink. POSITION mode = steady group colors, AUX mode = strip on/off by an aux channel.

## Wiring (actual build)
- FS-R6B PWM receiver CH1-6 → GPIO 32, 33, 34, 35, 26, 14 (Roll, Pitch, Throttle, Yaw, Aux1, Aux2). See `docs/pwm-receiver.md`.
- Motors: GPIO 27, 25, 4, 13.
- LED strip data → GPIO 16 (28 pixels, 4 legs of 7 in series).

## Gotchas
- Configurator **Save & Reboot** or factory reset wipes the CLI-only params above (`pin_input_pwm_*`, `serial_1`, etc.) — the Receiver tab then falls back to 12 dead channels. Re-apply via CLI and reboot (input device is chosen at boot). `led_mode` also reverts to `POSITION` after a Configurator session (strip stays solid, no blink) — re-apply `set led_mode STATUS`.
- MPU6050 intermittently fails to detect at boot; i2c errors count toward arming-block (NO_GYRO). Pending test: `set i2c_speed 100`, short solid-core I2C wires, shared ground, 100 nF cap on MPU VCC.
- LED strip showed all-white with the old I2S WS2812 driver (`StatusLed.cpp`); replaced with RMT (channel `RMT_CHANNEL_MAX-1`, 50 ns ticks, 350/850 ns bit edges). `rmt_write_items` with the driver installed points at the item array and the ISR streams it, so the whole frame (28 LEDs + reset) is one non-blocking write; `rmt_wait_tx_done(channel, 0)` guards against rebuilding the array mid-send. Verified: FastLED probe lit the strip correctly, and the RMT FC firmware shows the 4 group colors. LED fade runs at 25 Hz (40 ms step) inside `StatusLed::update()` so the 1 kHz control loop and 50 Hz actuator never stall.
- Strapping pins 0/2/5/12/15: avoid as inputs.
