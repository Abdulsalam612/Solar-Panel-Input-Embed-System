# Solar Panel Input Embedded System PVE Monitor

This project uses an **ESP32** to monitor the performance and status of a solar panel.

## Features
- **Temperature Monitoring**: Uses an NTC Thermistor to measure panel temperature.
- **Irradiance Monitoring**: Uses a Photoresistor (LDR) to estimate light intensity.
- **Overheat Warning**: Triggers a warning (and future active cooling) if temperature exceeds a threshold.

## Hardware Setup
- **Microcontroller**: ESP32 (Adafruit Huzzah32 / Feather compatible)
- **Sensors**:
  - Thermistor Module on **Pin A1 (GPIO 25)**
  - Photoresistor Module on **Pin 34 (GPIO 34)**

## Usage
1. Open `sketch_feb5a.ino` in Arduino IDE.
2. Select your ESP32 board.
3. Upload to the board.
4. Open Serial Monitor (115200 baud).
