# Smart IoT Electric Fence Detection & Isolation System - SIH 2026

## Selected Problem Statement
- **PS ID:** SIH25077
- **PS Title:** A hardware that can detect and prevent unauthorized use of electric fences
- **Category:** Hardware / Smart Automation (Government of Kerala)

## System Overview
An automated safety and security hardware system built with an ESP32 micro-controller. It continuously monitors electric fence line voltage via an analog step-down voltage divider, detects unauthorized high-voltage connections or lethal grid tapping, automatically isolates the fence using a heavy-duty power relay, and triggers local LCD/buzzer/LED alerts as well as remote alerts and sends message on phone  and the location of the fence via Telegram app.

## Hardware Circuit & Pin Configuration
| Component | ESP32 Pin / Destination | Function |
| :--- | :--- | :--- |
| Voltage Sense Probe | GPIO 34 (ADC Input) | Senses stepped-down fence voltage |
| Relay Module | GPIO 18 | Trips/isolates fence power on fault |
| Green LED | GPIO 16 (TX2) | Authorized Pass indicator |
| Red LED | GPIO 4 | Trip Alarm indicator |
| Piezo Buzzer | GPIO 19 | Audible Alert |
| I2C LCD (16x2) | GPIO 21 (SDA) / GPIO 22 (SCL) | Local status display |

## How It Works
1. **Normal Operation:** Senses standard legal line voltage (~1.5V) and displays `AUTHORIZED PASS` on the LCD.
2. **High-Voltage / Illegal Fault:** Senses excessive voltage (>2.5V via 3-resistor step-down divider).
3. **Automated Cutoff:** GPIO 18 opens the relay to isolate the line, turns on the Red LED and Buzzer, and displays `ALERT: ILLEGAL!` on the LCD screen.
4. **Shows message on phone:** The device uses built in ESP32 Wi-fi signaling to send alert messages on Telegram app of  the user.
5. **Shows real time location of fence:** The ESP32's built in chip to send the location of fence on the Telegram app of  the user.
6. **Shows real time voltage:** The ESP32's built in ADC pin is used to read the live voltage running in the fence.
