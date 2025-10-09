#include "DisplayManager.h"

// Instantiate the global display object
// GxEPD2_213_T5D pins: CS=5, DC=17, RST=16, BUSY=4
GxEPD2_BW<GxEPD2_213_T5D, GxEPD2_213_T5D::HEIGHT> display(GxEPD2_213_T5D(/*CS=*/5, /*DC=*/17, /*RST=*/16, /*BUSY=*/4));

/**
 * @brief Simple function to show a message on the display and Serial.
 * @param msg The message to display.
 */
void showMessage(const char *msg)
{
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

/**
 * @brief Displays a non-fatal error message with the current time.
 * @param message The error message to display.
 */
void displayError(const String &message)
{
  display.fillScreen(GxEPD_WHITE);
  String currentTime = getCurrentTimeString();
  display.setCursor(130, 10);
  display.print(currentTime);

  display.setCursor(0, 20);
  display.print(message);
  display.display(true); // partial update
}

/**
 * @brief Forces a full inversion refresh to reduce ghosting on the e-paper.
 */
void fullRefresh()
{
  display.fillScreen(GxEPD_BLACK);
  display.display(false); // full refresh
  delay(500);
  display.fillScreen(GxEPD_WHITE);
  display.display(false); // full refresh
}

/**
 * @brief Renders the main train schedule data on the e-paper display.
 */
void displayTrainData(
  const String& originStation, 
  const String& originPlatform, 
  const String& originTime,
  const String& destStation, 
  const String& destPlatform, 
  const String& destTime,
  const String& routeLabel
)
{
  display.fillScreen(GxEPD_WHITE);
  // Header background
  display.fillRect(0, 0, display.width(), 15, GxEPD_BLACK);
  display.setTextColor(GxEPD_WHITE);
  
  // Origin and Destination names
  display.setCursor(10, 5);
  display.print(originStation);
  display.setCursor(display.width() / 2 + 10, 5);
  display.print(destStation);

  // Separator line
  display.drawLine(display.width() / 2, 0, display.width() / 2, display.height(), GxEPD_BLACK);

  display.setTextColor(GxEPD_BLACK);
  
  // Origin Details
  display.setCursor(10, 30);
  display.print(originPlatform);
  display.setCursor(10, 50);
  display.print("Dep: ");
  display.print(originTime);

  // Destination Details
  display.setCursor(display.width() / 2 + 10, 30);
  display.print(destPlatform);
  display.setCursor(display.width() / 2 + 10, 50);
  display.print("Arr: ");
  display.print(destTime);

  // Route Label
  display.setCursor(10, display.height() - 20);
  display.print(routeLabel);
  
  // Current time
  display.setCursor(display.width() - 50, display.height() - 10);
  display.print(getCurrentTimeString()); 

  display.display(true); // partial update
}