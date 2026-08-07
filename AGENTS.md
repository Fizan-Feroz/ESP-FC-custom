# ESP-FC build notes

## Hardware (current build)
- Board: LOLIN32 (ESP32 WROOM, 240 MHz)
- Only serial device: **COM3** (CP210x) @ 115200 8N1
- Gyro/accel: MPU6050 on I2C0 (SDA 21, SCL 22). Uses hardware I2C — the software bit-bang path (EspWire) is disabled because WiFi/UART/timer interrupts corrupt it (see `Target/TargetESP32.h` + `TargetEsp32Common.h`).
- Baro: BMP180 on the same I2C bus.
- WiFi: AP `ESP-FC` / 192.168.4.1, TCP-to-MSP bridge on port 1111. `Wireless::startAp()` calls `WiFi.softAP("ESP-FC")` **and then `WiFi.softAPConfig(192.168.4.1, 192.168.4.1, 255.255.255.0)`** to force the AP DHCP server on — plain `softAP()` on Arduino core 2.0.17 did not hand out leases (clients stuck on 169.254.x APIPA, so Betaflight's `tcp://192.168.4.1:1111` was unreachable).

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
set led_color_2 40,40,255    set led_color_3 40,255,40
set led_idle 0,0,0
set i2c_speed 200         # default now 200 in ModelConfig.h (was 800); 400 kHz gave intermittent MPU6050 timeouts
set gyro_dev 3            # MPU6050 = skip probing all other gyros at boot (was AUTO: ~15 s of I2C timeouts per boot)
set pid_sync 1            # gyro is capped at 500 Hz (ESPFC_GYRO_I2C_RATE_MAX); loop 500 = max for PWM outputs anyway
set feature_soft_serial 1  # WiFi always on at boot
set airmode_activate_throttle 1400  # airmode latches armed+throttle>1400, releases below minCheck (1050)
# PID loop is capped at 500 Hz for standard PWM outputs (sanitize() rule in Model.h); pid_sync > 1 is clamped at boot
```
Group → LED mapping (count 28, groups 4): LEDs 0-6 front-left (white), 7-13 front-right (red), 14-20 back-right (blue), 21-27 back-left (green). Racing scheme: white+red = front, blue+green = back.
Data order (verified by probe): every leg's data starts at the frame center — nose seam for front legs, tail seam for back legs — and runs outward to the tip. `_frontPos(i)` maps that to a common "frame-center → tip" position: front legs `pos = i%gs`, back legs `pos = gs-1-(i%gs)` so all four legs move simultaneously.
LED behavior (STATUS mode, Betaflight-style): **disarmed** = fancy idle: a dim wash slowly cycles through all hues (~12s) while a bright hue-offset wave ping-pongs back-forth between the back and front seams (2.4s full cycle); **armed + ANGLE** = 250ms arm flash then back legs stay solid while both front legs flash together in an asymmetric double-flash (150ms on / 150ms off / 150ms on / 500ms off); **armed + ACRO** = arm flash then a back-first wave: the back legs sweep center→tip first (1.2s), then the front legs sweep center→tip (1.2s), as one continuous tail-to-nose pass over 2.4s (`_sweepBackToFront()` gives a global fixed-point pos in [0, 2*max]; back legs use the band at `pos`, front legs at `pos - max`, so both keep the confirmed center→tip direction); **armed + AIRMODE** = arm flash (250ms solid) then a symmetric dual-color strobe — front legs **red** `{255,0,0}`, back legs **blue** `{0,0,255}`, all ON 150ms / all OFF 150ms (`LED_AIRMODE_PATTERN`); **disarm** = last frame fades out over 400ms before the idle wave resumes; **failsafe or arming-blocked** = fast triple-blink (red group colors); **throttle arming-block** (`ARMING_DISABLED_THROTTLE`, throttle not low) = steady green flash (`LED_THROTTLE`, 200ms on/200ms off, pure green, takes priority over the general error blink). POSITION mode = steady group colors, AUX mode = strip on/off by an aux channel. The sweep/idle band uses a fractional (fixed-point, 1/256 LED) position with a smooth `(1-t²)²` falloff over BAND_RADIUS LEDs, and fades in/out at the ends (EDGE_FADE) so the ACRO reset is invisible.

## Wiring (actual build)
- FS-R6B PWM receiver CH1-6 → GPIO 32, 33, 34, 35, 26, 14 (Roll, Pitch, Throttle, Yaw, Aux1, Aux2). See `docs/pwm-receiver.md`.
- Motors: GPIO 27, 25, 4, 13.
- LED strip data → GPIO 16 (28 pixels, 4 legs of 7 in series).

## Gotchas
- **CPU load / sample rates**: the MPU6050 I2C reads dominate loop time (~550 µs each via Arduino TwoWire's 2-transaction `readFast` at 200 kHz; ~340 µs at 400 kHz). The I2C bus **cannot reliably run 400 kHz** (verified: at 400 kHz the MPU6050 consistently fails to answer at boot → flood of `i2cRead returned Error 263`, i2cErrorCount 7, frozen ~0 g data, NO_GYRO arming block; at 200 kHz it boots clean). So the fixed config is **`i2c_speed 200` + gyro 500 Hz (`ESPFC_GYRO_I2C_RATE_MAX` 500 in `TargetESP32.h`) + accel 250 Hz → CPU TOTAL ~48.6%** (loop 500 Hz, under setup.md's 50%; the earlier 400 kHz/gyro-1000 combo was 45.5% but unreliable). gyro 500/loop 500/accel 250 is the standard combo for a 450-class PWM quad — no stability loss. If MPU6050 detection ever fails again at 200 kHz, it's a wiring issue (pull-ups, shared ground, 100 nF on MPU VCC, or a flaky baro dragging the bus).
- **PWM RX failsafe latch (fixed)**: random `RX_FAILSAFE` arming blocks that would NOT clear until a reset were a real firmware bug, not an RF issue — `InputPWM::update()` checked `now - _last_frame_us > BROKEN_LINK_US` (100 ms) and returned `INPUT_LOST` **before** consuming a completed frame and never refreshed `_last_frame_us`, so one transient gap (RX dropout, missed interrupt edge, WiFi hiccup) latched the FC in `INPUT_LOST` forever: `arm flags: RX_FAILSAFE` stuck ON, `input:` frame rate read ~510 Hz (loop rate), yet channels still moved in the Configurator (the ISR data keeps flowing and `processInputs()` still runs on LOST). Reset always "fixed" it. The fix reorders `update()` to consume a full frame whenever all channels have updated (self-heals ~1 frame after the RX recovers) and only returns `INPUT_LOST` when no complete frame exists past the timeout — a real loss still failsafes, but recovers without a reboot. Healthy idle shows `input: 53 Hz` (50 Hz PWM); the ~510 Hz reading + latched flag = this bug (now fixed).
- **Gyro failure recovery (recovery-first, NOT disarm-first)**: a mid-flight gyro loss is a crash either way, and disarming = falling, so the firmware holds the last good data and re-inits fast instead. `GyroSensor::read()` keeps `gyro.raw`/`sampled` at the last good value on a failed read (PID holds the rate), and triggers `tryReinit()` after **10 consecutive failed reads (~20 ms)**; it also runs a **periodic health check (~125 reads ≈ 0.25 s)** that detects a stuck/sleeping chip by its ~0 g accel magnitude (reads "succeed" with zeros, so the read-failure path alone never fires) and re-inits. `tryReinit()` has a 2 s cooldown and logs `GYRO REINIT OK/FAILED`. `GyroMPU6050::begin()` retries WHO_AM_I up to 5× and verifies PWR_MGMT_1 wake by read-back (a chip that won't wake now fails cleanly instead of reporting present-but-dead).
- `airmode_activate_throttle` (default **1400**, Betaflight-style): airmode activates once armed + throttle > threshold, and **deactivates** again when throttle drops below `minCheck` (1050) — so it releases on the ground at low stick (matches Betaflight `pid.c`). Stored in `ModelConfig` (tail field) + wired to `MSP_RX_CONFIG`; set via CLI. Minimum usable value ≈ 1200 — anything lower triggers at rest on some TX trims (the earlier 0-value bug made airmode activate "without throttle").
- `Storage` (EEPROM, 2048 B): `ModelConfig` is persisted as a raw struct dump. When appending fields to `ModelConfig`, **only add them at the very end (after `modelName`)** and bump `EEPROM_VERSION` in `Storage.h` (load() migrates old configs by prefix-load + re-applying the new field default). Do NOT insert fields mid-struct — it shifts layout, corrupts the loaded config (caused a `mixerSync`=0 divide-by-zero panic at boot once), and new fields can be absorbed into old tail padding so a size check alone cannot detect an old config.
- Configurator **Save & Reboot** or factory reset wipes the CLI-only params above (`pin_input_pwm_*`, `serial_1`, etc.) — the Receiver tab then falls back to 12 dead channels. Re-apply via CLI and reboot (input device is chosen at boot). `led_mode` also reverts to `POSITION` after a Configurator session (strip stays solid, no blink) — re-apply `set led_mode STATUS`.
- MPU6050 detection: 400 kHz is unreliable on this bus (see CPU-load gotcha) — run `i2c_speed 200`. `set gyro_dev 3` (MPU6050) makes boot **~4-5 s**: `Hardware::detectGyro()` now skips probing every other supported gyro (the CLI-only `gyro_dev` param, added in `Hardware.cpp` via `selectGyroDevice()`; note the CLI accepts the numeric value, not the name — `set gyro_dev GYRO_MPU6050` silently parses to 0/AUTO). With `gyro_dev AUTO`, boot is ~19 s and floods serial with `i2cRead returned Error 263` spam (16 timeouts, ~1.05 s each, for the absent MPU9250/MPU6500/ICM20602/BMI160/LSM6DSO probes) — those show up as I2C errors in Betaflight Configurator; they are absent-chip probes, not a hang, and disappear with `gyro_dev 3`.
- LED strip showed all-white with the old I2S WS2812 driver (`StatusLed.cpp`); replaced with RMT (channel `RMT_CHANNEL_MAX-1`, 50 ns ticks, 350/850 ns bit edges). `rmt_write_items` with the driver installed points at the item array and the ISR streams it, so the whole frame (28 LEDs + reset) is one non-blocking write; `rmt_wait_tx_done(channel, 0)` guards against rebuilding the array mid-send. Verified: FastLED probe lit the strip correctly, and the RMT FC firmware shows the 4 group colors. LED fade runs at 25 Hz (40 ms step) inside `StatusLed::update()` so the 1 kHz control loop and 50 Hz actuator never stall.
- Strapping pins 0/2/5/12/15: avoid as inputs.
