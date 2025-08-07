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

        display.fillScreen(GxEPD_WHITE);
        // Draw black bar at the top (adjust height as needed, e.g., 0,0 to 212,20 for 212px wide, 20px tall)
        display.fillRect(0, 0, display.width(), 15, GxEPD_BLACK);

        // Set text color to white for the time
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(160, 5); // Adjust X and Y as needed
        display.print(getCurrentTimeString());
        display.setCursor(2, 5);
        display.print("Next Train:");

        display.setTextColor(GxEPD_BLACK);
        display.setCursor(0, 30);
        display.print("To: ");
        display.print(dest);
        display.setCursor(0, 50);
        display.print("Dep: ");
        display.print(depTime);
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

