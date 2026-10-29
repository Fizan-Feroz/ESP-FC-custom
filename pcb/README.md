# ESP-FC 30.5x30.5mm Flight Controller PCB

A high-performance, 4-layer drone flight controller designed for the **ESP-FC** firmware, powered by an **ESP32-WROOM-32E** (dual-core 240MHz with Wi-Fi / ESP-NOW telemetry) in a standard **30.5 × 30.5 mm M3** stack form factor.

---

## 1. Specifications & Features

* **MCU**: Espressif **ESP32-WROOM-32E** (Dual-core Xtensa 240MHz, 4MB Flash, integrated antenna overhang).
* **Form Factor**: 36.0 × 36.0 mm outer dimension, **30.5 × 30.5 mm M3** mounting holes (standard 4-in-1 ESC stack).
* **Stackup**: 4-layer high-speed PCB:
  * `Layer 1 (Top / F.Cu)`: RF & high-speed SPI bus routing, status LEDs, primary ICs.
  * `Layer 2 (Inner 1 / In1.Cu)`: Dedicated, unbroken **Ground Plane** (noise isolation for IMU).
  * `Layer 3 (Inner 2 / In2.Cu)`: Power planes (5V, 3.3V, VBAT).
  * `Layer 4 (Bottom / B.Cu)`: Motor pads, ESC harness connector, USB-UART bridge, solder pads.
* **Sensors**:
  * **Primary Onboard IMU**: InvenSense **MPU-6000 / MPU-6500** (QFN-24) connected via high-speed SPI bus (up to 4kHz loop rate).
  * **Dual IMU Support**: Dedicated 1×8 (2.54mm) breakout header for modular external sensors (GY-521, GY-91, GY-88, ICM-42688-P).
  * **I2C Bus**: Dedicated 4-pin port for external Barometer (BMP280 / SPL06) and GPS Magnetometer (QMC5883L).
* **Power Management**:
  * **Input Voltage**: 2S – 6S LiPo (7.4V – 25.2V) with TVS voltage-spike suppression diode (SMAJ28A).
  * **5V BEC**: Synchronous step-down buck converter (SY8120B / MP2359) delivering **5V @ 2A**.
  * **3.3V LDO**: Ultra-low-noise linear regulator (ME6211C33 / AP2112K-3.3, 500mA) dedicated to ESP32 and sensitive IMU.
  * **Power OR-ing**: BAT54C Schottky diode for seamless automatic power switching between USB and LiPo.
* **ESC & Motor Outputs**:
  * Standard JST-SH 8-pin 1.0mm 4-in-1 ESC connector (GND, VBAT, M1, M2, M3, M4, CURR, TELEM).
  * 4 corner high-current solder pads for direct ESC motor signal wiring (DShot150/300/600 & PWM).
* **USB & Flashing**:
  * USB Type-C connector with 5.1kΩ CC1/CC2 pull-downs.
  * Onboard CH340C USB-to-UART bridge.
  * Automatic bootloader / reset circuit (UMH3N dual-NPN) for one-click flashing via PlatformIO or Web ESP-Tool.

---

## 2. Hardware Pinout Table (Firmware Aligned)

| Signal Name | ESP32 GPIO | Description / Destination |
| :--- | :---: | :--- |
| **M1** (Motor 1) | `GPIO 27` | Front-Right Motor / ESC Signal 1 |
| **M2** (Motor 2) | `GPIO 25` | Rear-Right Motor / ESC Signal 2 |
| **M3** (Motor 3) | `GPIO 4` | Rear-Left Motor / ESC Signal 3 |
| **M4** (Motor 4) | `GPIO 12` | Front-Left Motor / ESC Signal 4 |
| **SPI_SCK** | `GPIO 18` | Hardware SPI Clock -> Onboard MPU & IMU Header |
| **SPI_MISO** | `GPIO 19` | Hardware SPI MISO -> Onboard MPU & IMU Header |
| **SPI_MOSI** | `GPIO 23` | Hardware SPI MOSI -> Onboard MPU & IMU Header |
| **SPI_CS0** | `GPIO 5` | Gyro Chip Select (`pin_spi_cs_0`) |
| **GYRO_INT** | `GPIO 34` | Gyro Data Ready Interrupt (Input only) |
| **I2C_SDA** | `GPIO 21` | I2C Data (Compass / Baro) with 4.7kΩ pull-up |
| **I2C_SCL** | `GPIO 22` | I2C Clock (Compass / Baro) with 4.7kΩ pull-up |
| **UART0_TX** | `GPIO 1` | USB Serial CLI / MSP / Betaflight Configurator |
| **UART0_RX** | `GPIO 3` | USB Serial CLI / MSP / Betaflight Configurator |
| **UART1_RX** | `GPIO 32` | CRSF / ELRS / SBUS Receiver RX |
| **UART1_TX** | `GPIO 33` | CRSF / ELRS Receiver Telemetry TX |
| **UART2_RX** | `GPIO 16` | GPS Module RX2 / Blackbox Logger |
| **UART2_TX** | `GPIO 17` | GPS Module TX2 / Blackbox Logger |
| **VBAT_ADC** | `GPIO 36` | LiPo Voltage Divider (100kΩ / 10kΩ, 11:1 scale) |
| **CURR_ADC** | `GPIO 39` | Current Sensor input from 4-in-1 ESC |
| **BUZZER** | `GPIO 26` | Active Buzzer driver (N-MOSFET 2N7002) |
| **LED_STAT** | `GPIO 2` | Status Indicator LED (Green) |
| **BOOT** | `GPIO 0` | Boot Switch / Auto-download circuit |
| **EN / RST** | `EN` | Chip Reset with 10kΩ pull-up and 1µF debounce |

---

## 3. Directory Contents

* **`esp32-fc.kicad_pcb`**: 4-layer KiCad PCB layout file with exact 30.5×30.5mm M3 geometry, footprint placement, and silkscreen markings.
* **`esp32-fc.kicad_pro`**: KiCad 10 project settings with net classes (`Default` and `Power`).
* **`build_fc_pcb.py`**: Python script that builds and automates board geometry via KiCad 10's native `pcbnew` API.
* **`esp32_fc_top_render.png`**: High-resolution 3D ray-traced render of the top board assembly.
* **`esp32_fc_bottom_render.png`**: High-resolution 3D ray-traced render of the bottom board assembly.
* **`gerbers/`**: Complete fabrication archive (Gerber RS-274X and Excellon Drill files) ready for JLCPCB / PCBWay manufacturing.

---

## 4. Bill of Materials (BOM)

| Ref Des | Component / Value | Package | Recommended Part Number |
| :--- | :--- | :--- | :--- |
| **U1** | ESP32-WROOM-32E (4MB) | Module SMD | Espressif ESP32-WROOM-32E-N4 |
| **U2** | MPU-6000 or MPU-6500 | QFN-24 (4×4mm, 0.5mm pitch) | InvenSense MPU-6000 / MPU-6500 |
| **U3** | CH340C USB-to-UART | SOIC-16 (1.27mm pitch) | WCH CH340C (internal clock) |
| **U4** | 5V 2A Synchronous Buck | SOT-23-6 | Silergy SY8120B / MPS MP2359 |
| **U5** | 3.3V 500mA Low-Noise LDO | SOT-23-5 | MicrOne ME6211C33M5G / Diodes AP2112K-3.3 |
| **L1** | 4.7µH 3A Power Inductor | SMD 0805 / 2012 | Sunlord SWPA4020S4R7MT |
| **D_TVS**| 28V 400W TVS Diode | SMA / DO-214AC | Littelfuse SMAJ28A |
| **D_OR** | Dual Schottky Barrier Diode | SOT-23 | Diodes Inc BAT54C |
| **Q_BUZ**| 60V 300mA N-MOSFET | SOT-23 | ON Semi 2N7002 / AO3400 |
| **C_IN1**| 10µF 50V X7R Ceramic | 0805 | Murata GRM21BR71H106KE43L |
| **C_OUT1**| 22µF 10V X5R Ceramic | 0805 | Samsung CL21A226MPCLRNC |
| **J_ESC**| 8-Pin 1.0mm SMD Horizontal | JST-SH 8P | JST SM08B-SRSS-TB |
| **H1-H4**| M3 Mounting Holes (3.2mm ID, 6.0mm OD) | Pad via annular ring | Standard 30.5×30.5mm M3 |
