# Solar Panel Emulator - Input Embedded System

An offline, standalone Internet of Things (IoT) embedded system built on the ESP32 to serve as the environmental sensing and control node for an educational Solar Panel Simulator.

## 📌 About The Project

This repository contains the firmware and web server assets for the **Input Embedded System**, a core subsystem of a university-level Solar PV Emulator project. The system is designed to provide students with a hands-on, interactive platform to observe how solar panels respond to varying environmental conditions (irradiance and temperature).

Designed with strict budget constraints and offline-capability requirements, this system completely bypasses the need for external internet infrastructure, cloud databases, or physical RTC modules through advanced software engineering.

## ✨ Key Features

* **Offline WPA2 Access Point & Captive Portal:** The ESP32 broadcasts its own secured local Wi-Fi network. A custom DNS server intercepts traffic, automatically redirecting users to the local dashboard without requiring manual IP entry.
* **Interactive Web Dashboard:** A locally hosted HTML/JS UI providing live Canvas charting telemetry, dynamic environmental mapping (e.g., simulating weather in Spain vs. the UK), and system status updates.
* **Internal Data Logging (LittleFS):** Continuous empirical data (Timestamp, Temp, Irradiance) is formatted as CSV and logged directly to the ESP32's internal flash memory, eliminating the need for an external SD card module.
* **"Browser Sync" Timekeeping:** Solves the lack of a physical Real-Time Clock (RTC) by utilizing a hidden background script to capture the precise local UNIX Epoch time from the connected user's smartphone.
* **Automated Thermal Safety Loop:** Non-blocking `millis()` architecture continuously monitors an NTC 10k thermistor (processed via the Steinhart-Hart equation). If thresholds are breached, the system instantly triggers a 12V cooling fan (via hardware PWM) and auditory alarms.
* **Inclusive Design & Accessibility:** Integrates a DFPlayer Mini MP3 module to provide spoken-word auditory feedback during thermal overload conditions for visually impaired users.
* **Role-Based Access Control (RBAC):** Utilizes HTTP Basic Authentication to create a dual-view interface. Students get read-only access, while Administrators can securely access endpoints to download CSV logs, wipe memory, and manually toggle hardware.

## 🧰 Hardware Components

* **Microcontroller:** ESP32-WROOM-32E DevKit (mounted via Seeit Terminal Adapter)
* **Temperature Sensor:** NTC 10k Thermistor
* **Irradiance Sensor:** LDR Photoresistor
* **Cooling:** 12V Axial Fan driven by an IRF520 MOSFET Power Module
* **Auditory Alarms:** DFPlayer Mini MP3 Module (with mini speaker) & Passive Buzzer
* **Power Distribution:** 12V to 5V Step-Down Buck Converter, WAGO Lever Connectors, 5A Inline Blade Fuse

## ⚙️ Software Architecture

The entire codebase is engineered on a **non-blocking architecture**. Instead of using traditional `delay()` functions, the main loop utilizes differential time checks. This ensures the processor remains available to instantly process incoming HTTP requests from multiple students while maintaining a strict 1-second interval for sensor polling and safety actuation.

### Dependencies
Ensure the following libraries are installed in your Arduino IDE:
* `WiFi.h` (Built-in)
* `WebServer.h` (Built-in)
* `DNSServer.h` (Built-in)
* `LittleFS.h` (Built-in)
* `DFRobotDFPlayerMini.h` (For the MP3 accessibility module)

## 🚀 Setup and Installation

1. **Clone the Repository:**
   ```bash
   git clone [https://github.com/Abdulsalam612/Solar-Panel-Input-Embed-System.git](https://github.com/Abdulsalam612/Solar-Panel-Input-Embed-System.git)
