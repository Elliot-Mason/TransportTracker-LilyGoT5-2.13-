#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"

#include <GxEPD2_BW.h>
GxEPD2_BW<GxEPD2_213_T5D, GxEPD2_213_T5D::HEIGHT> display(GxEPD2_213_T5D(/*CS=*/ 5, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4));

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* api_url = API_URL;

void displayError(const String &message);

// Get current time as a string in 12-hour format with AM/PM
String getCurrentTimeString() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  int hour = timeinfo.tm_hour;
  int minute = timeinfo.tm_min;
  String ampm = "AM";
  if (hour == 0) {
    hour = 12;
  } else if (hour == 12) {
    ampm = "PM";
  } else if (hour > 12) {
    hour -= 12;
    ampm = "PM";
  }
  char buf[10];
  snprintf(buf, sizeof(buf), "%02d:%02d %s", hour, minute, ampm.c_str());
  return String(buf);
}

// Map route type to label
String getRouteLabel(const String& routeType) {
  if (routeType == "BMT") return "Intercity";
  if (routeType == "T1") return "T1";
  // Add more as needed
  return routeType;
}

void setup() {
  Serial.begin(115200);
  display.init();
  display.setRotation(1);
  display.setTextColor(GxEPD_BLACK);

  WiFi.begin(ssid, password);
  display.fillScreen(GxEPD_WHITE);
  display.setCursor(0, 20);
  display.print("Connecting to WiFi...");
  display.display(true);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  display.fillScreen(GxEPD_WHITE);
  display.setCursor(0, 20);
  display.print("WiFi Connected!");
  display.display(true);
  delay(1000);

  // Set up NTP time (AEST, UTC+10)
  configTime(10 * 3600, 0, "au.pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP time sync...");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println(" done!");
}

void loop() {
  // Show loading animation
  

  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    http.begin(client, api_url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        JsonObject train = doc[0].as<JsonObject>();
        String depTime = train["legs"][0]["origin"]["departureTimePlanned"].as<String>();
        String dest = train["legs"][0]["destination"]["name"].as<String>();
        String rawTime = train["legs"][0]["origin"]["departureTimePlanned"].as<String>();
        int hour = rawTime.substring(11, 13).toInt();
        int minute = rawTime.substring(14, 16).toInt();

        // Adjust for AEST (+10 hours)
        hour += 10;
        if (hour >= 24) hour -= 24; // wrap around if next day

        // Convert to 12-hour format
        String ampm = "AM";
        if (hour == 0) {
          hour = 12;
          ampm = "AM";
        } else if (hour == 12) {
          ampm = "PM";
        } else if (hour > 12) {
          hour -= 12;
          ampm = "PM";
        }

        // Format with leading zeroes
        char localTime[9];
        snprintf(localTime, sizeof(localTime), "%02d:%02d %s", hour, minute, ampm.c_str());
        depTime = String(localTime);

        // Extract origin and destination info
        JsonObject origin = train["legs"][0]["origin"];
        JsonObject destination = train["legs"][0]["destination"];

        // Extract names and times
        String originName = origin["name"].as<String>(); // e.g. "Penrith Station, Platform 3, Sydney"
        String originTimeRaw = origin["departureTimePlanned"].as<String>(); // e.g. "2025-08-07T09:26:00Z"
        String destName = destination["name"].as<String>(); // e.g. "Central Station, Platform 16, Sydney"
        String destTimeRaw = destination["arrivalTimePlanned"].as<String>(); // e.g. "2025-08-07T11:39:00Z"

        // Extract route type
        String routeType = train["legs"][0]["transportation"]["disassembledName"].as<String>();
        String routeLabel = getRouteLabel(routeType);

        // Helper to extract platform from name (assumes "Platform X" is always present)
        auto extractPlatform = [](const String& name) -> String {
          int idx = name.indexOf("Platform ");
          if (idx >= 0) {
            int end = name.indexOf(",", idx);
            if (end < 0) end = name.length();
            return name.substring(idx, end);
          }
          return "";
        };

        // Helper to extract station title (before first comma)
        auto extractStation = [](const String& name) -> String {
          int idx = name.indexOf(",");
          if (idx >= 0) return name.substring(0, idx);
          return name;
        };

        // Helper to format time as 12-hour with AM/PM
        auto formatTime = [](const String& isoTime) -> String {
          int hour = isoTime.substring(11, 13).toInt();
          int minute = isoTime.substring(14, 16).toInt();
          // Adjust for AEST (+10 hours)
          hour += 10;
          if (hour >= 24) hour -= 24;
          String ampm = "AM";
          if (hour == 0) { hour = 12; }
          else if (hour == 12) { ampm = "PM"; }
          else if (hour > 12) { hour -= 12; ampm = "PM"; }
          char buf[10];
          snprintf(buf, sizeof(buf), "%02d:%02d %s", hour, minute, ampm.c_str());
          return String(buf);
        };

        String originStation = extractStation(originName);
        String originPlatform = extractPlatform(originName);
        String originTime = formatTime(originTimeRaw);

        String destStation = extractStation(destName);
        String destPlatform = extractPlatform(destName);
        String destTime = formatTime(destTimeRaw);

        // Draw split header bar
        display.fillScreen(GxEPD_WHITE);
        display.fillRect(0, 0, display.width(), 15, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(10, 5);
        display.print(originStation);
        display.setCursor(display.width() / 2 + 10, 5);
        display.print(destStation);

        // Draw vertical split line
        display.drawLine(display.width() / 2, 0, display.width() / 2, display.height(), GxEPD_BLACK);

        // Set text color back to black for info
        display.setTextColor(GxEPD_BLACK);

        // Left side: origin info
        display.setCursor(10, 30);
        display.print(originPlatform);
        display.setCursor(10, 50);
        display.print("Dep: ");
        display.print(originTime);

        // Right side: destination info
        display.setCursor(display.width() / 2 + 10, 30);
        display.print(destPlatform);
        display.setCursor(display.width() / 2 + 10, 50);
        display.print("Arr: ");
        display.print(destTime);

        // Bottom left: route label
        display.setCursor(10, display.height() - 20);
        display.print(routeLabel);

        display.display(true);
      } else {
        displayError("JSON Error: " + String(error.c_str()));
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
      }
    } else {
      // Handle error responses
      String errorResponse = http.getString();
      Serial.print("HTTP Error: ");
      Serial.print(httpCode);
      Serial.print(" - Response: ");
      Serial.println(errorResponse);

      // Try to parse the error message
      JsonDocument errorDoc;
      DeserializationError jsonError = deserializeJson(errorDoc, errorResponse);

      if (!jsonError && errorDoc.containsKey("error")) {
        String errorMessage = errorDoc["error"].as<String>();
        if (errorDoc.containsKey("details")) {
          errorMessage += ": " + errorDoc["details"].as<String>();
        }
        displayError(errorMessage);
      } else {
        displayError("HTTP Error " + String(httpCode));
      }
    }
    http.end();
  } else {
    displayError("WiFi Lost!");
    Serial.println("WiFi disconnected");
    WiFi.begin(ssid, password);
  }
  delay(30000); // 30 seconds
}

void displayError(const String &message) {
  display.fillScreen(GxEPD_WHITE);
  // Draw current time in top right
  String currentTime = getCurrentTimeString();
  display.setCursor(130, 10); // Adjust X for your display width
  display.print(currentTime);

  display.setCursor(0, 20);
  display.print(message);
  display.display(true);
}

