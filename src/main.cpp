#include <WiFi.h>
#include <WiFiManager.h> // captive portal
#include <Preferences.h>

// Include your custom modules
#include "TimeUtils.h"
#include "DisplayManager.h"
#include "DataFetcher.h"

#define BUTTON_PIN 39
const char *BUILD_TAG = "build_v3.8"; // bump this when flashing new firmware

// Global/external objects
Preferences prefs;
WiFiManager wifiManager;

// --- Station codes (saved in Preferences) ---
char origin_code[16] = "10101252";       // default Penrith
char destination_code[16] = "10101100";  // default Central

// --- WiFiManager Parameters ---
WiFiManagerParameter custom_origin("origin_code", "Origin Station Code", origin_code, 16);
WiFiManagerParameter custom_dest("dest_code", "Destination Station Code", destination_code, 16);

String api_url_base = "https://transport-tracker-server.vercel.app/api/trains?name_origin=";

unsigned long lastRefresh = 0;
const unsigned long refreshInterval = 10 * 60 * 1000; // 10 minutes (10 minutes * 60 seconds * 1000 ms)


/**
 * @brief Resets WiFi credentials and station codes, then reboots the ESP32.
 */
void resetCredentials()
{
  wifiManager.resetSettings();
  prefs.begin("app", false);
  prefs.putString("build_tag", BUILD_TAG);
  prefs.end();
  showMessage("WiFi creds reset.\nRebooting...");
  delay(2000);
  ESP.restart();
}


void setup()
{
  Serial.begin(115200);
  display.init(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Assuming IO39 is pulled up and button pulls it LOW

  // --- Firmware build check ---
  prefs.begin("app", false);
  String savedTag = prefs.getString("build_tag", "");
  prefs.end();
  if (savedTag != BUILD_TAG)
  {
    showMessage("New build.\nResetting WiFi...");
    delay(4000);
    resetCredentials();
  }

  // --- Button check at boot (initial press) ---
  if (digitalRead(BUTTON_PIN) == LOW)
  {
    showMessage("Button pressed at boot.\nResetting WiFi...");
    resetCredentials();
  }

  // --- Load stored station codes ---
  prefs.begin("app", true); // read-only
  String storedOrigin = prefs.getString("origin", origin_code);
  String storedDest = prefs.getString("destination", destination_code);
  prefs.end();
  storedOrigin.toCharArray(origin_code, sizeof(origin_code));
  storedDest.toCharArray(destination_code, sizeof(destination_code));

  // --- WiFiManager Configuration ---
  wifiManager.addParameter(&custom_origin);
  wifiManager.addParameter(&custom_dest);

  // connect or open AP
  if (!wifiManager.autoConnect("TransportTrackerSetup"))
  {
    showMessage("WiFi setup failed");
    ESP.restart();
  }

  // Save station codes entered by user in the captive portal
  prefs.begin("app", false); // writable
  prefs.putString("origin", custom_origin.getValue());
  prefs.putString("destination", custom_dest.getValue());
  prefs.end();

  // Reload the potentially new station codes
  String originStr = custom_origin.getValue();
  String destStr = custom_dest.getValue();
  originStr.toCharArray(origin_code, sizeof(origin_code));
  destStr.toCharArray(destination_code, sizeof(destination_code));

  showMessage("WiFi OK!\nRequesting information.\nPlease wait...");

  // --- Time sync and TZ configuration ---
  setLocalTimezone();
  time_t now = time(nullptr);
  // Wait for time sync (check if time is reasonable)
  while (now < 8 * 3600 * 2) 
  {
    delay(500);
    now = time(nullptr);
    Serial.print(".");
  }
  Serial.println("\nTime Synced.");
}

void loop()
{
  // --- Check IO39 button (network reset trigger during runtime) ---
  if (digitalRead(BUTTON_PIN) == LOW)
  {
    Serial.println("IO39 pressed - resetting WiFi credentials...");
    displayError("Resetting WiFi...");
    resetCredentials(); 
    // resetCredentials calls ESP.restart(), so code after this is unreachable.
  }

  // --- Normal WiFi + Tracker Logic ---
  if (WiFi.status() == WL_CONNECTED)
  {
    // Fetch data using the modular function
    TrainData data = fetchTrainData(origin_code, destination_code, api_url_base);

    if (data.success)
    {
      // Display the successful result
      displayTrainData(
        data.originStation, 
        data.originPlatform, 
        data.originTime,
        data.destStation, 
        data.destPlatform, 
        data.destTime,
        data.routeLabel
      );
    }
    else
    {
      // Display the error message
      displayError("Fetch Failed: " + data.errorMessage);
    }

    // Wait 30 seconds after a successful or failed fetch attempt
    delay(30000); 
  }
  else
  {
    displayError("WiFi Lost! Reconnecting...");
    WiFi.reconnect();
    delay(5000); // Wait a bit before checking status again
  }

  // --- Full refresh interval check ---
  if (millis() - lastRefresh > refreshInterval)
  {
    Serial.println("Performing full refresh.");
    fullRefresh();
    lastRefresh = millis();
  }
}