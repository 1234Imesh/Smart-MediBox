# Medibox: Smart Medication Management System

![Medibox Dashboard](https://via.placeholder.com/800x400.png?text=Medibox+Dashboard)  
*Medibox Dashboard showcasing real-time sensor data and control parameters*

## 📖 Project Overview

**Medibox** is an advanced IoT-based medication management system developed as part of the EN2853 Embedded Systems and Applications course (Assignment 2). Designed to enhance medication adherence and environmental monitoring, Medibox integrates an ESP32 microcontroller, various sensors, a servo motor, and a Node-RED dashboard to provide real-time monitoring and control. The system is tailored for Wokwi simulation, ensuring seamless integration of hardware and software components for a robust user experience.

This project, authored by **Imesh Abeysinghe (Index No: 220009H)**, demonstrates proficiency in embedded systems, MQTT communication, and user interface design. Medibox monitors light intensity, temperature, and humidity, controls a servo motor for medication dispensing, and provides an intuitive dashboard for remote management. The system also features an OLED display for local interaction, alarms for medication reminders, and a buzzer for alerts, making it a comprehensive solution for smart healthcare applications.

## 🌟 Features

- **Real-Time Environmental Monitoring**: Tracks light intensity (via LDR), temperature, and humidity (via DHT22) with data visualization on a Node-RED dashboard.
- **Servo Motor Control**: Automatically adjusts servo angle based on environmental parameters (light intensity, temperature) and user-defined settings.
- **MQTT Integration**: Publishes sensor data and receives control parameters via MQTT topics using the Mosquitto broker.
- **Interactive OLED Display**: Displays time, sensor data, and system status, with menu navigation for setting alarms and time zones.
- **Alarm System**: Supports two programmable alarms with snooze functionality and scheduled medication reminders.
- **Node-RED Dashboard**: Provides a user-friendly interface with gauges, charts, and sliders for monitoring and controlling system parameters.
- **WiFi Connectivity**: Connects to WiFi for seamless MQTT communication and NTP-based time synchronization.
- **Warning System**: Alerts users via LEDs and a buzzer when temperature or humidity exceeds safe thresholds.
- **Customizable Parameters**: Allows users to adjust sampling intervals, sending intervals, servo angles, control factors, and ideal temperature via the dashboard.

## 🛠️ Hardware Requirements

The Medibox system is designed for simulation on the Wokwi platform but can be adapted for physical hardware. The required components are:

| Component | Description | Pin Connections |
|-----------|-------------|-----------------|
| **ESP32 DevKit V1** | Microcontroller for processing and communication | - |
| **DHT22 Sensor** | Measures temperature and humidity | Pin D12 (SDA) |
| **LDR (Photoresistor)** | Measures light intensity | Pin D33 (Analog) |
| **Servo Motor** | Controls medication dispensing mechanism | Pin D14 (PWM) |
| **SSD1306 OLED Display** | 128x64 display for local interface | Pins D21 (SDA), D22 (SCL) |
| **Buzzer** | Audio alerts for alarms and warnings | Pin D5 |
| **LEDs (x2)** | Visual indicators for warnings and status | Pins D15, D4 |
| **Pushbuttons (x4)** | User input for menu navigation (CANCEL, OK, UP, DOWN) | Pins D25, D26, D27, D13 |
| **Resistors** | Current-limiting resistors for LEDs and pull-up for buttons | 220Ω (LEDs), 1kΩ (buttons), 100Ω (buzzer) |

Refer to `diagram.json` for the complete circuit schematic and connections.

## 💻 Software Requirements

- **Arduino IDE**: For compiling and uploading the ESP32 code (`sketch.ino`).
- **Wokwi Simulator**: For simulating the hardware setup and testing the system.
- **Node-RED**: For deploying the dashboard (`flows.json`) and managing MQTT communication.
- **Libraries**:
  - `WiFi.h`: For ESP32 WiFi connectivity.
  - `PubSubClient.h`: For MQTT communication.
  - `DHTesp.h`: For DHT22 sensor interfacing.
  - `Adafruit_GFX.h` & `Adafruit_SSD1306.h`: For OLED display control.
  - `ESP32Servo.h`: For servo motor control.
- **Mosquitto MQTT Broker**: Hosted at `test.mosquitto.org` (port 1883).
- **Node-RED Dashboard**: Node-RED add-on for the user interface.

## 🚀 Setup Instructions

Follow these steps to set up and run the Medibox project:

### 1. Clone the Repository
git clone [https://github.com/1234imesh/medibox.git](https://github.com/1234Imesh/Smart-MediBox)
cd medibox


### 2. Open the Code in Arduino IDE
- Open `sketch.ino` file
- Select board: **ESP32 Dev Module**
- Select correct COM port
- Install required libraries (listed above)
- Upload the code

### 3. Run Node-RED
- Import `flows.json` into Node-RED
- Ensure MQTT server is set to `test.mosquitto.org` on port `1883`
- Deploy the flow
- Open the dashboard in your browser

### 4. Wokwi Simulation
- Open Wokwi simulator
- Import the project files
- Run simulation and verify dashboard updates
