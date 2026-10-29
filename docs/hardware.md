# ESP-FC Custom Flight Controller Hardware & PCB Guide

This document provides complete hardware specifications, schematic theory, assembly instructions, and configuration procedures for the custom **ESP-FC 30.5×30.5mm Flight Controller PCB** located in [`/pcb`](../pcb/).

---

## 1. Specifications Overview

| Parameter | Specification | Notes |
| :--- | :--- | :--- |
| **Microcontroller** | Espressif ESP32-WROOM-32E | Dual-core 240MHz, 4MB Flash, 2.4GHz Wi-Fi / ESP-NOW |
| **Form Factor** | 36.0 × 36.0 mm outer | Standard **30.5 × 30.5 mm M3** mounting holes |
| **PCB Stackup** | 4-layer FR4 (1.6mm thick) | Layer 2 unbroken GND plane for ultra-low IMU noise |
| **Primary Gyro/Accel** | InvenSense MPU-6000 / MPU-6500 | QFN-24, high-speed SPI bus (up to 4kHz loop rate) |
| **Auxiliary IMU** | 1×8 (2.54mm) Breakout Header | Supports GY-521, GY-91, GY-88, or ICM-42688-P modules |
| **Input Voltage** | 2S – 6S LiPo (7.4V – 25.2V) | Protected by 28V 400W TVS surge diode (SMAJ28A) |
| **5V BEC Output** | 5V @ 2A Synchronous Buck | Powers receiver, GPS, buzzer, and 3.3V LDO |
| **3.3V LDO Output** | 3.3V @ 500mA Ultra-Low-Noise | < 30µVRMS ripple for ESP32 and sensitive gyro |
| **ESC Interface** | 8-Pin JST-SH 1.0mm + 4 Corner Pads | Standard 4-in-1 ESC pinout with DShot600 support |
| **USB Interface** | USB Type-C + CH340C | Auto-reset download circuit (flashing without buttons) |

---

## 2. Visual Layout & Dimensions

```
                            [FRONT OF DRONE]
                                  ▲
       [M4 Pad]                                         [M1 Pad]
   Motor 4 (Front-Left)                            Motor 1 (Front-Right)
   Signal: GPIO 12                                 Signal: GPIO 27
         │                                               │
         ▼                                               ▼
     ┌───────────────────────────────────────────────────────┐
     │  [H4]                                           [H1]  │
     │   ● ◄────────────── 30.50 mm ─────────────────────► ●   │
     │   │   ┌──────────────┐         ┌───────────────┐  │   │
     │   │   │ [J_RC]       │         │ [J_BUZ]       │  │   │
     │   │   │ 1: 5V        │         │ 1: BUZ+ (5V)  │  │   │
     │   │   │ 2: GND       │         │ 2: BUZ- (MOS) │  │   │
     │   │   │ 3: RX1 (32)  │         └───────────────┘  │   │
     │   │   │ 4: TX1 (33)  │                            │   │
     │   │   └──────────────┘                            │   │
     │   ▲                                               ▲   │
     │   │   ┌──────────────┐         ┌───────────────┐  │   │
     │   │   │ [J_BAT]      │         │ [J_GPS]       │  │   │
     │ 30.50 │ 1: VBAT (LiPo│         │ 1: 5V         │ 30.50│
     │   mm  │ 2: GND       │         │ 2: GND        │  mm  │
     │   │   └──────────────┘         │ 3: RX2 (16)   │  │   │
     │   │                            │ 4: TX2 (17)   │  │   │
     │   │   ┌──────────────┐         └───────────────┘  │   │
     │   │   │ [J_I2C]      │                            │   │
     │   │   │ 1: 3.3V      │                            │   │
     │   │   │ 2: GND       │                            │   │
     │   │   │ 3: SCL (22)  │                            │   │
     │   │   │ 4: SDA (21)  │                            │   │
     │   │   └──────────────┘                            │   │
     │   ▼                                               ▼   │
     │   ● ◄────────────── 30.50 mm ─────────────────────► ●   │
     │  [H3]       [==== J_ESC (8-Pin JST-SH) ====]    [H2]  │
     └───────────────────────────────────────────────────────┘
         ▲                                               ▲
         │                                               │
   Motor 3 (Rear-Left)                             Motor 2 (Rear-Right)
   Signal: GPIO 4                                  Signal: GPIO 25
       [M3 Pad]                                         [M2 Pad]
                                  ▼
                            [REAR OF DRONE]
```

---

## 3. Connector Pinouts & Peripheral Connections

### A. 4-in-1 ESC Port (`J_ESC`)
Connects to standard 4-in-1 ESC ribbon cables (JST-SH 8-pin):
| Pin | Name | MCU Connection | Description |
| :---: | :--- | :--- | :--- |
| **1** | `GND` | Ground Plane | High-current ground return |
| **2** | `VBAT` | `VBAT` Bus | Battery power directly from ESC |
| **3** | `M1` | `GPIO 27` | Motor 1 signal (Front-Right) |
| **4** | `M2` | `GPIO 25` | Motor 2 signal (Rear-Right) |
| **5** | `M3` | `GPIO 4` | Motor 3 signal (Rear-Left) |
| **6** | `M4` | `GPIO 12` | Motor 4 signal (Front-Left) |
| **7** | `CURR` | `GPIO 39` (`ADC1_CH3`) | ESC current sensor analog telemetry |
| **8** | `TELEM`| `GPIO 16` (`UART2_RX`) | ESC telemetry serial data (RPM / Temp) |

> **Note:** Direct solder pads (`M1`–`M4`) are also provided at all 4 corners of the board for individual motor wiring.

---

### B. Radio Receiver Port (`J_RC`)
Supports ExpressLRS (ELRS), TBS Crossfire (CRSF), or SBUS:
| Pin | Name | Connects To |
| :---: | :--- | :--- |
| **1** | `5V` | Receiver `5V` / `VCC` |
| **2** | `GND` | Receiver `GND` |
| **3** | `RX1` (`GPIO 32`) | Receiver `TX` (Channel 1) |
| **4** | `TX1` (`GPIO 33`) | Receiver `RX` (Channel 2 / Telemetry) |

---

### C. GPS & External Compass (`J_GPS` & `J_I2C`)
* **`J_GPS` (UART2)**:
  * Pin 1: `5V`
  * Pin 2: `GND`
  * Pin 3: `RX2` (`GPIO 16`) $\rightarrow$ Connect to GPS `TX`
  * Pin 4: `TX2` (`GPIO 17`) $\rightarrow$ Connect to GPS `RX`
* **`J_I2C` (I2C Bus)**:
  * Pin 1: `3.3V`
  * Pin 2: `GND`
  * Pin 3: `SCL` (`GPIO 22`) $\rightarrow$ Connect to Compass/Barometer `SCL`
  * Pin 4: `SDA` (`GPIO 21`) $\rightarrow$ Connect to Compass/Barometer `SDA`

---

### D. External IMU Test Header (`J_IMU`)
8-pin 2.54mm header for modular testing with GY-521 / GY-91 breakout boards:
| Pin | Name | ESP32 GPIO | Description |
| :---: | :--- | :---: | :--- |
| **1** | `VCC` | — | 3.3V Sensor Power |
| **2** | `GND` | — | Ground |
| **3** | `SCL / SCK` | `GPIO 18` | Hardware SPI Clock |
| **4** | `SDA / MOSI` | `GPIO 23` | Hardware SPI MOSI |
| **5** | `SDO / MISO` | `GPIO 19` | Hardware SPI MISO |
| **6** | `NCS / CS` | `GPIO 5` | Gyro Chip Select (`pin_spi_cs_0`) |
| **7** | `INT` | `GPIO 34` | Data-Ready Interrupt (Input-only) |
| **8** | `3V3` | — | Auxiliary 3.3V |

---

### E. Buzzer & Battery Voltage Sense
* **Buzzer (`J_BUZ`)**:
  * Pin 1 (`BUZ+`): `5V`
  * Pin 2 (`BUZ-`): Switched ground through `Q_BUZ` (2N7002 N-MOSFET driven by `GPIO 26`) with flyback protection diode.
* **Battery Input (`J_BAT`)**:
  * Direct LiPo solder pads (2S–6S).
  * Onboard 100kΩ / 10kΩ divider (11:1 attenuation) feeds attenuated voltage to `GPIO 36` (`ADC1_CH0`).
  * At maximum 6S battery voltage (25.2V), the ADC receives 2.29V, safely within the ESP32 3.3V range.

---

## 4. Theory of Operation & Power Architecture

```
 LiPo (7.4V - 25.2V) ──► SMAJ28A TVS ──► SY8120B Buck (5V @ 2A) ──► BAT54C ──► +5V Bus
                                                                       ▲
 USB Type-C (5.0V) ────────────────────────────────────────────────────┘
                                                                       │
                                             ME6211C33 Ultra-Low-Noise LDO
                                                                       │
                                                                       ▼
                                                          +3.3V Low-Noise Bus
                                                      (ESP32 & MPU-6000 Gyro)
```

1. **Surge Suppression:** Fast active braking from ESCs generates massive inductive flyback spikes. A 28V 400W TVS diode (`SMAJ28A`) clamps voltage spikes before they reach the buck regulator.
2. **5V Step-Down (Buck):** High-efficiency synchronous step-down converter steps LiPo voltage down to 5.0V with minimal heat dissipation.
3. **Dual Power OR-ing:** A `BAT54C` Schottky pair enables seamless, safe bench testing via USB-C without back-feeding the battery or ESCs.
4. **Clean 3.3V Analog Rail:** The ESP32 Wi-Fi bursts draw up to 350mA current spikes. An isolated `ME6211C33` linear regulator isolates the MPU-6000 sensor from digital MCU rail noise.

---

## 5. Assembly & First Power-Up Checklist

### Soldering Tips:
1. **Solder Paste & Stencil:** Use leaded (Sn63Pb37) or lead-free (SAC305) No-Clean solder paste with a stainless steel stencil (0.12mm thickness).
2. **Reflow Profile:** Peak temperature 235°C–245°C. A standard hot plate or reflow oven works well.
3. **QFN-24 Gyro:** Inspect the center thermal ground pad. Ensure no excess solder creates solder bridges to the 0.5mm pitch outer pins.

### First Smoke-Test Procedure:
1. **Continuity Check:** Before applying power, verify with a multimeter:
   * Resistance between `VBAT` and `GND` > 100kΩ.
   * Resistance between `5V` and `GND` > 10kΩ.
   * Resistance between `3.3V` and `GND` > 1kΩ.
2. **Bench Power Test:** Set a bench power supply to **12V with a 100mA current limit**.
3. **Check Voltage Rails:**
   * Verify `+5V` rail measures between **4.95V and 5.10V**.
   * Verify `+3.3V` rail measures between **3.27V and 3.33V**.
4. **Red LED:** `D1` (Power LED) should illuminate solid.
5. **USB Enumeration:** Plug in USB Type-C. Device Manager should show **CH340 (COM port)**.

---

## 6. Initial Firmware Flashing & Configuration

### Flashing via Web ESP-Tool:
1. Open [ESP Tool Web Flasher](https://espressif.github.io/esptool-js/).
2. Select your CH340 COM port at `115200` baud.
3. Load the pre-compiled `esp-fc` binary with address `0x00`.
4. Click **Program**. After flashing, power cycle the board.

### Firmware CLI Configuration:
Open the Betaflight Configurator CLI or serial terminal (`115200` baud) and paste the following commands:

```text
# Motor Outputs (Quad-X layout)
set pin_output_0 27
set pin_output_1 25
set pin_output_2 4
set pin_output_3 12

# Hardware SPI Gyro
set pin_spi_0_sck 18
set pin_spi_0_mosi 23
set pin_spi_0_miso 19
set pin_spi_cs_0 5

# I2C Bus for Compass / Barometer
set pin_i2c_scl 22
set pin_i2c_sda 21

# Serial Ports
set pin_serial_0_rx 3
set pin_serial_0_tx 1
set pin_serial_1_rx 32
set pin_serial_1_tx 33
set pin_serial_2_rx 16
set pin_serial_2_tx 17

# Analog Voltage & Current Telemetry
set pin_input_adc_0 36
set pin_input_adc_1 39

# Status LED & Buzzer
set pin_buzzer 26
set pin_led 2

save
```

---

## 7. Ordering Guide (Fabrication Parameters)

When submitting the board files to **JLCPCB** or **PCBWay**, upload [`esp32-fc-gerbers.zip`](../pcb/gerbers/) and select these parameters:

* **Dimensions:** 36.0 mm × 36.0 mm
* **Layer Count:** 4 Layers
* **PCB Thickness:** 1.6 mm
* **Base Copper Weight:** 1 oz outer / 0.5 oz or 1 oz inner
* **Surface Finish:** ENIG (Electroless Nickel Immersion Gold) recommended for QFN pads, or Lead-free HASL
* **Solder Mask:** Matte Black or Green
* **Silkscreen:** White
* **Impedance Control:** JLC04161H-7628 standard stackup
