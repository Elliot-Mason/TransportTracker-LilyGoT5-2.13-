#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "TimeUtils.h" // For getCurrentTimeString

// Define the display object externally so all files can use the same instance
// This assumes the same pins as your original code
extern GxEPD2_BW<GxEPD2_213_T5D, GxEPD2_213_T5D::HEIGHT> display;

void showMessage(const char *msg);
void displayError(const String &message);
void fullRefresh();
void displayTrainData(
  const String& originStation, 
  const String& originPlatform, 
  const String& originTime,
  const String& destStation, 
  const String& destPlatform, 
  const String& destTime,
  const String& routeLabel
);

#endif // DISPLAY_MANAGER_H