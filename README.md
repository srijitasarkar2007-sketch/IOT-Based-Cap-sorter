# IoT-Based Bottle Cap Color Sorting System

An ESP32-based IoT system that detects and classifies bottle cap colours using the TCS3200 colour sensor. The system supports real-time colour detection, Bluetooth communication, MQTT cloud monitoring, and a web dashboard for monitoring sensor data.

---

## Features

- Real-time bottle cap colour detection
- Colour calibration using Teach Mode
- MQTT-based cloud communication
- Bluetooth communication with Android application
- Live web dashboard for monitoring
- RGB LED indication of detected colour
- Dual communication mode (Wi-Fi or Bluetooth)

---

## Working Principle

1. The user calibrates the system using **Teach Mode**.
2. The TCS3200 colour sensor measures the RGB frequency values of the bottle cap.
3. The ESP32 compares the measured values with the stored calibration data.
4. The detected colour is transmitted using **MQTT** or **Bluetooth**, depending on the available communication mode.
5. The Android application displays the detected colour and allows the user to control various hardware functions.
6. The web dashboard displays the detected colour and sensor data in real time.
7. The RGB LED indicates the detected bottle cap colour.

---

## Technologies Used

### Hardware

- ESP32 DevKit V1
- TCS3200 Colour Sensor
- HC-05 Bluetooth Module
- RGB LED Module

### Software

- Arduino IDE
- Embedded C++
- MQTT
- HiveMQ Cloud
- HTML
- CSS
- JavaScript
- MIT App Inventor

---

## Repository Structure

```
firmware/       → ESP32 source code
website/        → Web dashboard
mobile-app/     → Android application
hardware/       → Circuit diagrams 
docs/           → Technical documentation

```

## Challenges Faced

- Colour calibration under varying lighting conditions
- Secure MQTT communication over Wi-Fi
- Managing Bluetooth and Wi-Fi communication on the ESP32
- Reducing sensor noise using averaging techniques
- Integrating hardware, firmware, mobile application, and web dashboard into a single system

---

## Future Improvements

- Conveyor belt integration
- Servo-based automatic bottle cap sorting
- Industrial-grade PCB design
- Improved enclosure and mechanical assembly
- Machine learning-based colour classification
- Enhanced calibration algorithms

---

## Author

**Srijita Sarkar**
