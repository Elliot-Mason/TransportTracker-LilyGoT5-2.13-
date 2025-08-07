# Transport Tracker E-Ink Display

This project displays live train departure and arrival information on a LilyGO T5 2.13" e-ink screen using data from a transport API. Designed for wall mounting or desk use, it shows origin/destination stations, times, platforms, and the route line in a minimalist format. 

---

## 🛠 Features

- Connects to Wi-Fi and syncs time via NTP
- Fetches live transport data from a configurable API
- Displays departure and arrival stations, platforms, and times
- Includes route label (e.g. T1, Intercity)
- Updates every 30 seconds
- Handles and displays error messages clearly
- Clock shown in 12-hour format with AM/PM

---

## 🧰 Hardware Used

- [LilyGO T5 V2.13" e-ink display](https://github.com/Xinyuan-LilyGO/LilyGo-T5-Epaper-Series)
- ESP32 onboard (included with LilyGO)
- Micro USB power (or battery option)

---

## 📦 Libraries Required

Install the following libraries in the Arduino IDE:

- **WiFi.h** (built-in for ESP32)
- **WiFiClient.h**
- **HTTPClient.h**
- **ArduinoJson** (`v6.x`)
- **GxEPD2** by Jean-Marc Zingg
- **Time / time.h** (built-in for ESP32)

---

## 🔐 `secrets.h` File

Create a `secrets.h` file in the project folder with the following content:

```cpp
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"
#define API_URL "https://your-api-endpoint.com/path"
