# ESP32 Smart MedBox (OLED + MQTT + Sensors)

An ESP32-based medicine reminder/monitoring project with an OLED dashboard, MQTT reminders, and basic environmental + light sensing. Designed to run both on real hardware **and** in the included **Wokwi** simulation.

## Features

- **OLED status dashboard (SSD1306 I2C, 128×64)**
  - Shows time (RTC), temperature/humidity (DHT22), and device status
  - Displays **"TAKE MEDS!"** when an MQTT reminder arrives
- **MQTT reminders + acknowledgements**
  - Subscribes for reminders (and also publishes scheduled reminders)
  - Publishes sensor telemetry and “medicine taken” acknowledgements
- **Alarm outputs**
  - **LED blink** + **buzzer tone** while alarm is active
  - **Button** stops alarm and sends an acknowledgement
- **Boot reliability**
  - WiFi connection has a timeout (continues in offline mode)
  - MQTT reconnect is non-blocking (retries periodically)

## Hardware (Wokwi diagram matches this)

### Pin map (ESP32 DevKit)

| Component | Signal | ESP32 Pin |
|---|---:|---:|
| OLED SSD1306 | SDA | GPIO **21** |
| OLED SSD1306 | SCL | GPIO **22** |
| RTC DS1307 | SDA | GPIO **21** |
| RTC DS1307 | SCL | GPIO **22** |
| DHT22 | DATA | GPIO **15** |
| LDR (analog out) | AO | GPIO **34** |
| LED | Anode | GPIO **2** |
| Buzzer | Signal | GPIO **4** |
| Button | Input (pull-up) | GPIO **5** |

### Notes

- OLED I2C address is commonly **0x3C** (some modules use **0x3D**). The code tries **0x3C** then falls back to **0x3D**.
- I2C uses `Wire.begin(21, 22)` (SDA=21, SCL=22).

## MQTT

### Broker (default)

- `broker.hivemq.com` on port `1883`

### Topics

- **Subscribe**
  - `medbox/reminder`
    - Payload: `TIME_TO_TAKE_MEDS` → triggers the alarm
- **Publish**
  - `medbox/reminder`
    - Payload: `TIME_TO_TAKE_MEDS` (sent at scheduled times)
  - `medbox/status`
    - Payload: `MEDICINE_TAKEN` (sent when button is pressed during alarm)
  - `medbox/sensors`
    - JSON payload every ~10s:
      - Example: `{"t":25.1,"h":60,"l":1234}`

## How it works (high level)

- On boot, the OLED briefly shows **“System Online”**
- Device attempts WiFi for ~15 seconds; if it fails, it continues running **offline**
- MQTT reconnect is attempted periodically when WiFi is connected
- At 08:00, 14:00, and 20:00 the device publishes a reminder (`TIME_TO_TAKE_MEDS`) to `medbox/reminder` (flags reset at 00:01)
- Every ~2 seconds the OLED updates:
  - Time from the RTC
  - Temperature/humidity from the DHT22
  - Status text (or “TAKE MEDS!” if alarm active)
- When an MQTT reminder arrives (local or remote):
  - Alarm turns on (LED blinking + buzzer tone)
  - Pressing the button stops the alarm and publishes `MEDICINE_TAKEN`

## Run locally with PlatformIO

### Prerequisites

- VS Code + **PlatformIO** extension

### Build

```bash
pio run
```

### Upload (real ESP32)

```bash
pio run -t upload
```

### Serial monitor

```bash
pio device monitor
```

## Run in Wokwi (simulation)

This repo includes:

- `diagram.json` (circuit)
- `wokwi.toml` (points Wokwi at the PlatformIO firmware output)

Typical flow:

1. Build firmware:

```bash
pio run
```

2. Start Wokwi simulation using your Wokwi tooling (VS Code Wokwi extension / Wokwi CLI) and this project’s `wokwi.toml`.

## Configuration

Edit these constants in `src/main.cpp`:

- WiFi:
  - `ssid`
  - `password`
- MQTT:
  - `mqtt_server`

## Project structure

- `src/main.cpp` — main firmware (WiFi, MQTT, OLED UI, sensors, alarm logic)
- `platformio.ini` — PlatformIO environment + library dependencies
- `diagram.json` — Wokwi circuit (wiring + parts)
- `wokwi.toml` — Wokwi configuration for `.pio/build/...` artifacts
- `libraries.txt` — human-readable list of used libraries

## Troubleshooting

- **Boot seems “stuck” on OLED**
  - Open the Serial Monitor (`115200`) to see whether WiFi or MQTT is retrying.
  - If using a different OLED module, confirm the I2C address (**0x3C** vs **0x3D**) and wiring to SDA/SCL.
- **No time shown / RTC issues**
  - Ensure DS1307 is wired to the same I2C bus and powered correctly.
  - The code prints an RTC warning and continues if the RTC isn’t detected.
- **No MQTT messages**
  - Verify broker hostname/port.
  - Make sure your publisher sends exactly `TIME_TO_TAKE_MEDS` to `medbox/reminder`.

## Credits / Libraries

Declared in `platformio.ini`:

- PubSubClient (MQTT)
- Adafruit DHT sensor library
- Adafruit GFX + Adafruit SSD1306
- RTClib

