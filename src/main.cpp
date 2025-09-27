#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <WiFiManager.h>   // captive portal
#include <Preferences.h>
#include <secrets.h>  // contains WiFi SSID/PASSWORD and API_URL

#include <GxEPD2_BW.h>
GxEPD2_BW<GxEPD2_213_T5D, GxEPD2_213_T5D::HEIGHT> display(GxEPD2_213_T5D(/*CS=*/ 5, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4));

#define BUTTON_PIN 39
const char* BUILD_TAG = "build_v2.0";   // bump this when flashing new firmware

Preferences prefs;
WiFiManager wifiManager;

const char* api_url = API_URL;
unsigned long lastRefresh = 0;
const unsigned long refreshInterval = 10 * 60 * 1000; // 5 minutes

void displayError(const String &message);

// --- Helpers ---
void showMessage(const char* msg) {
  display.setRotation(1);
  display.setFont(NULL);
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.println(msg);
  display.display();
  Serial.println(msg);
}

void resetCredentials() {
  wifiManager.resetSettings();
  prefs.begin("app", false);
  prefs.putString("build_tag", BUILD_TAG);
  prefs.end();
  showMessage("WiFi creds reset.\nRebooting...");
  delay(2000);
  ESP.restart();
}

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
  return routeType;
}

// Force full inversion refresh to reduce ghosting
void fullRefresh() {
    display.fillScreen(GxEPD_BLACK);
    display.display(false);  // full refresh
    delay(500);
    display.fillScreen(GxEPD_WHITE);
    display.display(false);  // full refresh
}

void displayError(const String &message) {
  display.fillScreen(GxEPD_WHITE);
  String currentTime = getCurrentTimeString();
  display.setCursor(130, 10);
  display.print(currentTime);

  display.setCursor(0, 20);
  display.print(message);
  display.display(true);
}

void setup() {
  Serial.begin(115200);
  display.init(115200);
  pinMode(BUTTON_PIN, INPUT);

  showMessage("Starting...");

  // firmware build check
  prefs.begin("app", false);
  String savedTag = prefs.getString("build_tag", "");
  prefs.end();
  if (savedTag != BUILD_TAG) {
    showMessage("New build.\nReset WiFi...");
    resetCredentials();
  }

  // button check
  if (digitalRead(BUTTON_PIN) == LOW) {
    showMessage("Button pressed.\nReset WiFi...");
    resetCredentials();
  }

  // connect or open AP
  showMessage("Connecting WiFi...");
  if (!wifiManager.autoConnect("T5-Setup-XXXX")) {
    showMessage("WiFi setup failed");
    ESP.restart();
  }

  showMessage("WiFi OK!");
  configTime(10 * 3600, 0, "au.pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    now = time(nullptr);
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure(); // accept any SSL cert
    HTTPClient http;

    String currentUrl = api_url;
    bool requestDone = false;

    while (!requestDone) {
      http.begin(client, currentUrl);
      int httpCode = http.GET();

      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        StaticJsonDocument<8192> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
          // Find the next train departing after current time
          JsonObject nextTrain;
          time_t nowTime = time(nullptr);

          for (JsonObject train : doc.as<JsonArray>()) {
            String depTimeStr = train["legs"][0]["origin"]["departureTimePlanned"].as<String>();
            struct tm depTimeInfo = {};
            strptime(depTimeStr.c_str(), "%Y-%m-%dT%H:%M:%S", &depTimeInfo);
            time_t depTime = mktime(&depTimeInfo);
            depTime -= 10 * 3600; // adjust for AEST (UTC+10)

            if (depTime >= nowTime) {
              nextTrain = train;
              break;
            }
          }

          if (nextTrain.isNull()) nextTrain = doc[0].as<JsonObject>();

          JsonObject origin = nextTrain["legs"][0]["origin"];
          JsonObject destination = nextTrain["legs"][0]["destination"];

          String originName = origin["name"].as<String>();
          String originTimeRaw = origin["departureTimePlanned"].as<String>();
          String destName = destination["name"].as<String>();
          String destTimeRaw = destination["arrivalTimePlanned"].as<String>();

          String routeType = nextTrain["legs"][0]["transportation"]["disassembledName"].as<String>();
          String routeLabel = getRouteLabel(routeType);

          auto extractPlatform = [](const String& name) -> String {
            int idx = name.indexOf("Platform ");
            if (idx >= 0) {
              int end = name.indexOf(",", idx);
              if (end < 0) end = name.length();
              return name.substring(idx, end);
            }
            return "";
          };

          auto extractStation = [](const String& name) -> String {
            int idx = name.indexOf(",");
            if (idx >= 0) return name.substring(0, idx);
            return name;
          };

          auto formatTime = [](const String& isoTime) -> String {
            int hour = isoTime.substring(11, 13).toInt();
            int minute = isoTime.substring(14, 16).toInt();
            hour += 10;
            if (hour >= 24) hour -= 24;
            String ampm = "AM";
            if (hour == 0) hour = 12;
            else if (hour == 12) ampm = "PM";
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

          display.fillScreen(GxEPD_WHITE);
          display.fillRect(0, 0, display.width(), 15, GxEPD_BLACK);
          display.setTextColor(GxEPD_WHITE);
          display.setCursor(10, 5);
          display.print(originStation);
          display.setCursor(display.width() / 2 + 10, 5);
          display.print(destStation);

          display.drawLine(display.width() / 2, 0, display.width() / 2, display.height(), GxEPD_BLACK);

          display.setTextColor(GxEPD_BLACK);
          display.setCursor(10, 30);
          display.print(originPlatform);
          display.setCursor(10, 50);
          display.print("Dep: ");
          display.print(originTime);

          display.setCursor(display.width() / 2 + 10, 30);
          display.print(destPlatform);
          display.setCursor(display.width() / 2 + 10, 50);
          display.print("Arr: ");
          display.print(destTime);

          display.setCursor(10, display.height() - 20);
          display.print(routeLabel);

          display.display(true);
          delay(30000);
        } else {
          displayError("JSON Error: " + String(error.c_str()));
          Serial.print("JSON parse error: ");
          Serial.println(error.c_str());
          delay(30000);
        }
        requestDone = true;
      } else if (httpCode == 308) {
        String newLocation = http.getLocation();
        http.end();
        currentUrl = newLocation; // follow redirect
      } else if (httpCode == -1) {
        String errMsg = "HTTP Connection failed Retrying now";
        Serial.print(errMsg);
        Serial.print(": ");
        Serial.println(http.errorToString(httpCode));
        displayError(errMsg);
        delay(1000);
      } else {
        String errorResponse = http.getString();
        Serial.print("HTTP Error: ");
        Serial.print(httpCode);
        Serial.print(" - Response: ");
        Serial.println(errorResponse);

        StaticJsonDocument<1024> errorDoc;
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
        delay(30000);
        requestDone = true;
      }
      http.end();
    }
  } else {
    displayError("WiFi Lost!");
    Serial.println("WiFi disconnected");
    WiFi.begin(ssid, password);
    delay(30000);
  }
  
  if (millis() - lastRefresh > refreshInterval) {
    fullRefresh();
    lastRefresh = millis();
  }
}
