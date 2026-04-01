# ESP32-FreeRTOS-IoT
# ESP32 FreeRTOS IoT Project 🚀

## Overview
This project demonstrates a real-time IoT system using ESP32 + FreeRTOS.

## Features
- Multitasking using FreeRTOS
- Queue (sensor → LED communication)
- Semaphore (button interrupt)
- MQTT cloud communication
- Mobile app control

## Architecture
Sensor → Queue → LED  
Button → ISR → Semaphore → LED  
MQTT ↔ ESP32 ↔ Mobile App  

## Technologies
- ESP32
- FreeRTOS
- MQTT (HiveMQ)
- Arduino IDE

## How to Run
1. Add WiFi credentials
2. Upload code to ESP32
3. Use IoT MQTT Panel app
4. Publish:
   - Topic: esp32/led
   - Message: ON / OFF
