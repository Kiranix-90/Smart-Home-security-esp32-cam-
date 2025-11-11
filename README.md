# 🏠 Smart Home Security System (ESP32-CAM & Blynk IoT)

## Project Overview

This project implements an affordable and efficient Internet of Things (IoT) solution for real-time home surveillance and intruder alerting[cite: 4]. [cite_start]It uses the powerful ESP32-CAM module to act as a camera, motion detector, and web server, integrated with the Blynk IoT platform for remote monitoring and instant alerts.

The system ensures security through both local automation (sounding a buzzer and activating a relay/light) and remote connectivity (sending push notifications and image snapshots)[cite: 8].

## ✨ Key Features

* **Motion-Activated Alerts:** The PIR Motion Sensor continuously scans the area and triggers the system upon movement.
* **Instant Image Capture:** The ESP32-CAM automatically captures a high-quality JPEG snapshot when motion is detected, serving it via a local web server (V2).
* **Remote Notification:** Sends instant push notifications to the user's mobile app via the Blynk Cloud[cite: 7].
* **Local Alarms:** Activates local alert systems, including an audible **Buzzer** and a **Status LED**[cite: 6].
* **External Control:** Controls high-power devices (like a siren or external lights) via a **Relay Module**[cite: 6].
* **Manual Snapshot:** Allows the user to trigger a snapshot on-demand using a button widget (V3) in the Blynk app.

## 🧱 Hardware Requirements (Essentials)

| Component | Purpose in Project |
| :--- | :--- |
| **ESP32-CAM (OV2640)** | [cite_start]Main controller, camera sensor, and Wi-Fi module. |
| **PIR Motion Sensor** | [cite_start]Input to detect human movement. |
| **1-Channel Relay Module** | [cite_start]Controls external siren or light devices. |
| **Active Buzzer & LED** | [cite_start]Provides immediate local audible and visual feedback. |
| **FTDI Programmer** | Required for initial code upload (programming mode). |
| **5V 2A Power Adapter** | Essential for reliable power when the camera and Wi-Fi are active. |

## 💻 Software and Setup Steps

1.  **Code Upload:** Ensure the correct board and partition scheme are selected. To upload code, the $\mathbf{GPIO 0}$ pin must be connected to $\mathbf{GND}$. This connection must be removed when the code runs.
2.  **Blynk Console:** Generate the **Template ID**, **Template Name**, and **Auth Token**. Configure a custom **Event** named **`motion_detected`** for push notifications.
3.  **Circuit Connection:** The system uses a **$5\text{V}$ $\text{DC}$ Adapter** (via a $\text{DC Jack}$) for power stability.
4.  **Pin Highlights:** The key $\text{GPIO}$ assignments are: $\mathbf{GPIO 13}$ ($\text{PIR}$ input), $\mathbf{GPIO 12}$ ($\text{Buzzer}$), $\mathbf{GPIO 4}$ ($\text{LED}$), and $\mathbf{GPIO 14}$ ($\text{Relay}$).
5.  **Blynk Dashboard:** Set up the mobile app dashboard utilizing the following $\text{Virtual Pins}$:
    * **V1:** $\text{LED}$ or $\text{Gauge}$ Widget (Displays $\mathbf{1}$ for motion, $\mathbf{0}$ for clear).
    * **V2:** **Image Gallery** Widget (Receives the snapshot $\text{URL}$ on motion or manual click).
    * **V3:** **Button** Widget (Set to $\text{PUSH}$ mode; used to command a picture capture manually).
