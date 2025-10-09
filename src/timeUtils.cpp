#include "TimeUtils.h"
#include <stdio.h> // For snprintf

// Define the Sydney timezone string using the standard POSIX format.
const char *SYDNEY_TZ = "AEST-10AEDT,M10.1.0,M4.1.0/3";

/**
 * @brief Sets the system's time zone to local (Sydney).
 */
void setLocalTimezone() {
  // Use configTzTime to set the local timezone (AEST/AEDT)
  configTzTime(SYDNEY_TZ, "au.pool.ntp.org", "time.nist.gov");
}

/**
 * @brief Converts a UTC ISO 8601 string (e.g., 2025-10-08T09:26:30) to the correct UTC epoch time (time_t).
 * @param isoTime The UTC ISO 8601 time string.
 * @return The UTC epoch time (time_t).
 */
time_t utcIsoToEpoch(const String &isoTime) {
  struct tm t = {};
    
  // 1. Temporarily set TZ to UTC to correctly interpret the API's UTC string
  setenv("TZ", "UTC0", 1);
  tzset();

  strptime(isoTime.c_str(), "%Y-%m-%dT%H:%M:%S", &t);
  t.tm_isdst = 0; // UTC does not observe DST
    
  // mktime now correctly interprets the struct as UTC time because TZ is temporarily UTC.
  time_t utc_epoch = mktime(&t);

  // 2. Restore local TZ configuration
  setLocalTimezone();  // Restores the SYDNEY_TZ setting
    
  return utc_epoch;
}

/**
 * @brief Formats a UTC ISO 8601 string into a local 12-hour time string (e.g., 09:26 AM).
 * @param isoTime The UTC ISO 8601 time string.
 * @return A String with the formatted local time.
 */
String formatTime(const String &isoTime) {
  time_t raw = utcIsoToEpoch(isoTime); // Get the correct UTC epoch time

  struct tm local;
  localtime_r(&raw, &local); // Convert UTC epoch to local time struct (using the restored TZ)

  int hour = local.tm_hour;
  int minute = local.tm_min;
  String ampm = "AM";

  // 12-hour clock conversion logic
  if (hour == 0)
    hour = 12;  
  else if (hour == 12)
    ampm = "PM";
  else if (hour > 12)
  {
    hour -= 12;
    ampm = "PM";
  }

  char buf[10];
  snprintf(buf, sizeof(buf), "%02d:%02d %s", hour, minute, ampm.c_str());
  return String(buf);
}

/**
 * @brief Gets the current system time as a string in 12-hour format with AM/PM.
 * @return A String with the current formatted time.
 */
String getCurrentTimeString() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  int hour = timeinfo.tm_hour;
  int minute = timeinfo.tm_min;
  String ampm = "AM";

  // 12-hour clock conversion logic (duplicate for reliability, but good practice to centralize)
  if (hour == 0)
  {
    hour = 12;
  }
  else if (hour == 12)
  {
    ampm = "PM";
  }
  else if (hour > 12)
  {
    hour -= 12;
    ampm = "PM";
  }
  
  char buf[10];
  snprintf(buf, sizeof(buf), "%02d:%02d %s", hour, minute, ampm.c_str());
  return String(buf);
}