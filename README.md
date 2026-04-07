# ESP32-CAM Smart DoorCam with OTA & Motion Detection

## Overview

This project is a **smart IoT-based door camera system** built using **ESP32-CAM** and a **self-hosted server (Android + Termux)**.

It supports:

* OTA (Over-The-Air) firmware updates
* Motion detection (custom embedded CV logic)
* Image capture & upload to local server
* Doorbell trigger system
* Server-side logging and storage

The system is designed to work **fully locally without cloud dependency**, making it lightweight, private, and extensible.

---

## Architecture

```
ESP32-CAM (Edge Device)
    ↓
WiFi (Local Network)
    ↓
Android Phone (Termux Server)
    ↓
- OTA Server
- Image Storage
- Doorbell Trigger
- Logging System
```

---

## Tech Stack

### Hardware

* ESP32-CAM (AI Thinker)
* FTDI Programmer
* Android phone (Samsung F13) as server

### Software

* Arduino (ESP32 firmware)
* Python (custom HTTP server in Termux)
* Linux (via Termux)

---

## Features Implemented

### 1. OTA Firmware Update

* Custom HTTP server serving firmware.bin
* Version-based update system
* ESP checks `/version` before downloading firmware

---

### 2. Remote Logging System

* ESP sends logs via HTTP POST
* Logs visible in Termux server
* Helps debug without UART

---

### 3. Doorbell Trigger System

* ESP sends `/ring` request
* Server plays sound using Termux API

---

### 4. Image Capture & Upload

* ESP captures JPEG frames
* Sends via HTTP POST `/upload`
* Server saves images locally

---

### 5. Motion Detection (Custom CV Logic)

* Lightweight embedded motion detection
* No external ML model
* Optimized for ESP32 constraints

---

## Development Journey (Key Challenges & Fixes)

---

### Issue 1: Flashing Errors

**Error:**

```
Failed to erase flash / chip stopped responding
```

**Fix:**

* Proper grounding accross ESP32, FTDI and power source
* Stable USB connection
* Lower baud rate

---

### Issue 2: Brownout Detector

```
E BOD: Brownout detector was triggered
```

**Cause:**

* Insufficient power supply

**Fix:**

* Use stable 5V power
* Avoid weak USB / OTG supply
* (Optional) capacitor for stabilization

---

### Issue 3: OTA Failures

* Broken pipe errors
* Not enough space
* Continuous OTA loop

**Fixes:**

* Correct Content-Length handling
* Add version check logic
* Ensure partition supports OTA

---

### Issue 4: HTTP Timeout (-11)

**Cause:**

* Improper socket handling in server

**Fix:**

* Custom socket-based HTTP server
* Proper response headers + connection close

---

### Issue 5: Camera Crash on Re-init

```
gpio_install_isr_service already installed
camera not supported
```

**Cause:**

* Repeated `esp_camera_init()` / `deinit()`

**Fix:**

* Avoid runtime camera reinitialization
* Use single stable camera configuration

---

### Issue 6: Motion Detection Not Working (Noisy JPEG)

#### Phase 1:

* Diff always high → false triggers
  Cause: JPEG compression noise

#### Phase 2:

* Diff stable but no detection
  Cause: Over-filtering

#### Phase 3:

* Threshold adapting incorrectly
  Cause: baseline rising with motion

---

### Final Motion Detection Solution

Implemented:

#### 1. Sparse Sampling

* Sample selected bytes instead of full frame

#### 2. Temporal Smoothing

* Rolling average of pixel values

#### 3. Adaptive Baseline

* Tracks noise level dynamically

#### 4. Baseline Freeze Logic

```cpp
if (diff < baseline + 10)
```

#### 5. Dynamic Threshold

```cpp
threshold = baseline + 10
```

#### 6. Strong Motion Override

```cpp
if (diff > baseline + 30)
```

#### 7. Multi-frame Confirmation

```cpp
motionCounter >= 2
```

---

## Final System Behavior

| Condition      | Diff    | Result         |
| -------------- | ------- | -------------- |
| No motion      | 90–110  |  No trigger    |
| Small movement | 110–130 |  Conditional   |
| Real movement  | 130–180 |  Trigger       |

---

## Key Learnings

* JPEG data is unreliable for raw motion detection
* Embedded systems require **signal filtering, not brute force**
* Power stability is critical for ESP32
* OTA systems need proper version control
* Real-world CV requires:

  * Noise filtering
  * Temporal validation
  * Adaptive thresholds

---

## Future Improvements

* Human detection using server-side AI
* Push notifications (Telegram / mobile app)
* Video clip capture (multi-frame burst)
* Face detection (optional)
* Dockerized server deployment (full linux system)

---

## Project Highlights

* Fully local (no cloud dependency)
* Runs on low-cost hardware
* Real-time edge processing
* Custom-built motion detection algorithm
* OTA-enabled embedded system

---
