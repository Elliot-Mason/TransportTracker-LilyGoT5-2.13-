#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <WiFiManager.h>   // captive portal
#include <Preferences.h>

#include <GxEPD2_BW.h>
GxEPD2_BW<GxEPD2_213_T5D, GxEPD2_213_T5D::HEIGHT> display(GxEPD2_213_T5D(/*CS=*/ 5, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4));

#define BUTTON_PIN 39
const char* BUILD_TAG = "build_v3.4";   // bump this when flashing new firmware

#define FORCE_SETUP_KEY "force_setup"

Preferences prefs;
WiFiManager wifiManager;

// --- Station codes (saved in Preferences) ---
char origin_code[16] = "10101252";      // default Penrith
char destination_code[16] = "10101100"; // default Central
 
// --- WiFiManager Parameters ---
WiFiManagerParameter custom_origin("origin_code", "Origin Station Code", origin_code, 16);
WiFiManagerParameter custom_dest("dest_code", "Destination Station Code", destination_code, 16);

String api_url_base = "https://transport-tracker-server.vercel.app/api/trains?name_origin=";

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
  prefs.putBool(FORCE_SETUP_KEY, true);   // tell next boot to go into setup mode
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

  prefs.begin("app", false);
  bool forceSetup = prefs.getBool(FORCE_SETUP_KEY, false);
  prefs.end();

  if (forceSetup == true) {
    prefs.putBool(FORCE_SETUP_KEY, false);
    showMessage("Please configure WiFi");
  } else {
    showMessage("Starting...");
  }
 
  // firmware build check
  prefs.begin("app", false);
  String savedTag = prefs.getString("build_tag", "");
  prefs.end();
  if (savedTag != BUILD_TAG) {
    showMessage("New build.\nReset WiFi...");
    delay(2000);
    resetCredentials();
  }

  // button check
  if (digitalRead(BUTTON_PIN) == LOW) {
    showMessage("Button pressed.\nReset WiFi...");
    resetCredentials();
  }

  // Load stored station codes
  prefs.begin("app", true);
  String storedOrigin = prefs.getString("origin", origin_code);
  String storedDest   = prefs.getString("destination", destination_code);
  prefs.end();
  storedOrigin.toCharArray(origin_code, sizeof(origin_code));
  storedDest.toCharArray(destination_code, sizeof(destination_code));

  // Add station parameters to WiFiManager portal
  wifiManager.addParameter(&custom_origin);
  wifiManager.addParameter(&custom_dest);

  // connect or open AP
  if (!wifiManager.autoConnect("TransportTrackerSetup")) {
    showMessage("WiFi setup failed");
    ESP.restart();
  }

  // Save station codes entered by user
  prefs.begin("app", false);
  prefs.putString("origin", custom_origin.getValue());
  prefs.putString("destination", custom_dest.getValue());
  prefs.end();

  String originStr = prefs.getString("origin", origin_code);
  String destStr   = prefs.getString("destination", destination_code);
  originStr.toCharArray(origin_code, sizeof(origin_code));
  destStr.toCharArray(destination_code, sizeof(destination_code));

  showMessage("WiFi OK!\n Requesting information, Please wait...");

  // Time sync
  configTime(10 * 3600, 0, "au.pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    now = time(nullptr);
  }
}


void loop() {
  // --- Check IO39 button (network reset trigger) ---
  if (digitalRead(39) == LOW) {
    Serial.println("IO39 pressed - resetting WiFi credentials...");
    displayError("Resetting WiFi...");
    resetCredentials();
    Serial.println("Access Point started: TransportTrackerSetup");
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(0, 20);
    display.print("Setup AP: TransportTrackerSetup");
    display.display(true);
    while (true) { delay(1000); }
  }

  // --- Normal WiFi + Tracker Logic ---
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure(); // accept any SSL cert
    HTTPClient http;

    // Build API URL dynamically
    String currentUrl = api_url_base + origin_code + "&name_destination=" + destination_code;
    Serial.println("API Request: " + currentUrl);

    bool requestDone = false;

    while (!requestDone) {
      http.begin(client, currentUrl.c_str());
      int httpCode = http.GET();

      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        StaticJsonDocument<8192> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
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
          delay(30000);
        }
        requestDone = true;
      } else {
        displayError("HTTP Error " + String(httpCode));
        delay(30000);
        requestDone = true;
      }
      http.end();
    }
  } else {
    displayError("WiFi Lost! Reconnecting...");
    WiFi.reconnect();
  }

  if (millis() - lastRefresh > refreshInterval) {
    fullRefresh();
    lastRefresh = millis();
  }
}
