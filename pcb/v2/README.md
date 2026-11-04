# ESP-FC v2 Flight Controller PCB

Revision 2 of the custom 30.5x30.5mm 4-layer flight controller PCB for the **ESP-FC** flight control firmware.

---

## 1. Revision Highlights & Changelog (v2)

* **TVS Diode Upgrade (D1)**: Upgraded from `SMAJ20A` to **`SMAJ28A`** (400W unidirectional, 28V standoff voltage). Safely accommodates 6S LiPo batteries (up to 25.2V nominal charge + flyback margin) without false clamping or thermal breakdown during active braking.
* **100% Complete Routing**:
  * `UART1_RX` (GPIO 16) routed cleanly to J3 (Pin 3).
  * `UART1_TX` (GPIO 17) routed cleanly to J3 (Pin 4).
  * `I2C_SDA` (GPIO 21) routed cleanly between ESP32, pull-up resistor R11, and external I2C header J4.
* **DRC Verification**:
  * **0 unconnected items**
  * **0 electrical clearance errors**
  * **0 shorting errors**
  * Fully compliant with standard JLCPCB / PCBWay manufacturing rules (min clearance: 0.15mm, power: 0.20mm, min via: 0.45mm / 0.20mm drill).
* **Production Gerbers**:
  * Generated and packaged in `esp-fc-v2-gerbers.zip`.

---

## 2. Specifications

| Parameter | Specification | Notes |
| :--- | :--- | :--- |
| **MCU** | Espressif ESP32-WROOM-32E | Dual-core 240MHz, 4MB Flash |
| **Form Factor** | 36.0 x 36.0 mm outer | Standard 30.5 x 30.5 mm M3 mounting holes |
| **Layer Stackup** | 4-Layer FR4 (1.6mm thickness) | L1: Signals/RF, L2: GND Plane, L3: Power Planes, L4: Signals/Power |
| **Copper Weight** | 1 oz outer / 1 oz inner | 35µm copper thickness |
| **Input Voltage** | 2S - 6S LiPo (7.4V - 25.2V) | D1 TVS clamping: SMAJ28A |
| **5V BEC** | Synchronous Buck 5V @ 2A | Powers receiver, buzzer, GPS, and LDO |
| **3.3V LDO** | Low-noise LDO 3.3V @ 500mA | Powers ESP32 and onboard IMU |
| **IMU** | InvenSense MPU-6000 / MPU-6500 | Dedicated SPI bus + external modular 1x8 IMU header |
| **Connectors** | JST-SH 1.0mm 8-pin (ESC) | Standard 4-in-1 ESC pinout with telemetry & current sense |
| | JST-SH 1.0mm 4-pin (UART1/GPS) | 5V, GND, RX1, TX1 |
| | JST-SH 1.0mm 4-pin (I2C/Mag/Baro) | 3.3V, GND, SCL, SDA |
| | USB Type-C | CH340C auto-reset programming interface |

---

## 3. Manufacturing & Ordering Guide (JLCPCB / PCBWay)

Upload `esp-fc-v2-gerbers.zip` directly to the PCB manufacturer with the following parameters:

* **Dimensions**: 36.0 mm x 36.0 mm
* **Layers**: 4
* **PCB Thickness**: 1.6 mm
* **Impedance Control**: No (or standard JLC04161H-7628 stackup)
* **Surface Finish**: ENIG (recommended for QFN MPU-6000) or HASL-lead-free
* **Copper Weight**: 1 oz outer / 1 oz inner
* **Solder Mask**: Matte Black or Blue
* **Silkscreen**: White
* **Minimum Track/Clearance**: 0.15mm / 0.15mm (6/6 mil)
* **Minimum Via Hole Size**: 0.20mm (via diameter 0.45mm)
