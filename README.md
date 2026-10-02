# Diode Experiment Hardware and Firmware

This folder contains the hardware design files and the ESP32 data-acquisition firmware for the dual-current p–n junction temperature experiment.

## Contents

- `diode_experiment.eprj2`: the schematic and PCB project.
- `GERBER.zip`: the PCB fabrication files (Gerber and drill), ready for prototype ordering.
- `esp32_adc_temp/`: the ESP32 firmware for synchronized voltage and temperature sampling, developed with Arduino.

## Schematic and PCB

`diode_experiment.eprj2` contains both the schematic and the PCB layout. Open it with **EasyEDA Pro** (嘉立创EDA专业版), either the desktop client or the web editor at <https://pro.easyeda.com>, using **File → Open → Project**.

## PCB Fabrication

`GERBER.zip` contains the production files for a two-layer board:

- Top and bottom copper, solder mask and silkscreen layers
- Top paste mask
- Board outline
- Plated-through-hole and via drill files

To order boards, upload `GERBER.zip` unchanged to a PCB manufacturer such as JLCPCB; do not extract it first. Most manufacturers detect the board size and layer count from the files automatically.

## ESP32 Firmware (`esp32_adc_temp`)

The firmware samples the circuit's output voltage on an ADC pin and reads a DS18B20 reference thermometer. It sends each voltage reading together with the latest temperature over USB serial.

### Hardware Connections

The analog voltage is read on GPIO34 (ADC1 channel 6). 

The DS18B20 data line (DQ) connects to GPIO21, with a 4.7 kΩ pull-up resistor to 3.3 V. Power the sensor from the ESP32's 3.3 V and GND pins.

### Sampling

- The ADC is sampled every **5 ms** at 12-bit resolution.
- Every **40 samples** (200 ms, i.e. 10 power-line cycles at 50 Hz) are averaged into one output value. This 10-NPLC averaging suppresses mains interference.
- The DS18B20 is read at 12-bit resolution (0.0625 °C) about every 0.8 s. It runs in a separate FreeRTOS task on core 0, so the 1-Wire conversion never disturbs the 5 ms ADC timing on core 1.

### Serial Output

The firmware runs at 115200 baud and sends one line every 200 ms:

```
<ADC>,<Temperature>
```

For example:

```
364.42,25.6875
```

- `ADC` is the average raw 12-bit ADC code (0–4095), with two decimal places. Convert it to volts with your own calibration against a digital multimeter.
- `Temperature` is the most recent DS18B20 reading in °C. Because the sensor updates more slowly than the ADC, about four consecutive lines carry the same temperature.
- `-127.0000` means no valid temperature reading in the last 3 s, for example right after power-up or if the sensor is disconnected. Discard these lines.
- Lines starting with `#` are status messages.

### Build and Upload

Requirements:

- [Arduino IDE](https://www.arduino.cc/en/software) or `arduino-cli`
- ESP32 board package by Espressif (tested with `esp32:esp32` 2.0.9). Board: **ESP32 Dev Module**.
- **OneWire** library (tested with 2.3.8), available from the Library Manager

To build and upload with the Arduino IDE:

1. Open `esp32_adc_temp/esp32_adc_temp.ino`.
2. Select **Tools → Board → ESP32 Dev Module** and the correct COM port.
3. Click **Upload**.
4. Open the Serial Monitor at 115200 baud to check the output.

To build and upload with `arduino-cli`, replacing `COMx` with your port:

```
arduino-cli compile --fqbn esp32:esp32:esp32 -u -p COMx esp32_adc_temp
```
