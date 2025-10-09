#include "TimeUtils.h"
#include <stdio.h> 
#include <time.h> // Required for struct tm, mktime, etc.
#include <unistd.h> // Required for tzset and setenv

// Define the Sydney timezone string
const char *SYDNEY_TZ = "AEST-10AEDT,M10.1.0,M4.1.0/3";

/**
 * @brief Sets the system's time zone to local (Sydney).
 */
void setLocalTimezone() {
  // Use configTzTime to set the local timezone (AEST/AEDT)
  configTzTime(SYDNEY_TZ, "au.pool.ntp.org", "time.nist.gov");
}

/**
 * @brief Converts a UTC ISO 8601 string (e.g., 2025-10-09T04:06:00) to the correct UTC epoch time (time_t).
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
 * @brief Manually calculates the DST offset (in seconds) for a given local time.
 * Sydney DST runs from the first Sunday in October to the first Sunday in April.
 * @param local_t The broken-down time struct (in AEST).
 * @return 3600 (1 hour) if DST is active, 0 otherwise.
 */
long getSydneyDSTOffset(struct tm *local_t) {
    int month = local_t->tm_mon; // 0=Jan, 9=Oct, 3=Apr
    int day = local_t->tm_mday;
    int wday = local_t->tm_wday; // 0=Sun

    // 1. Always DST: Nov (10), Dec (11), Jan (0), Feb (1), Mar (2)
    if (month >= 10 || (month >= 0 && month <= 2)) {
        return 3600; // 1 hour offset
    }

    // 2. Always Standard Time: May (4) through Sep (8)
    if (month >= 4 && month <= 8) {
        return 0; // 0 hour offset
    }

    // 3. Transition Months: October (9) and April (3)

    // Check October (Start of DST: 2am on first Sunday in Oct)
    if (month == 9) {
        // Find the day of the first Sunday
        int day_of_first_sunday = 1 + (7 - ((1 + local_t->tm_wday - wday + 7) % 7));
        
        // If past the first Sunday, it's DST.
        if (day > day_of_first_sunday) return 3600; 
        
        // If it is the first Sunday, check the hour (DST starts at 2:00 AM)
        if (day == day_of_first_sunday && local_t->tm_hour >= 2) return 3600;

        return 0; // Before the transition
    }
    
    // Check April (End of DST: 3am on first Sunday in Apr)
    if (month == 3) {
        // Similar logic to find the first Sunday of April
        int day_of_first_sunday = 1 + (7 - ((1 + local_t->tm_wday - wday + 7) % 7));
        
        // If before the first Sunday, it's DST.
        if (day < day_of_first_sunday) return 3600; 
        
        // If it is the first Sunday, check the hour (DST ends at 3:00 AM, so 2:59 AM is still DST)
        if (day == day_of_first_sunday && local_t->tm_hour < 3) return 3600;

        return 0; // After the transition
    }

    return 0;
}


/**
 * @brief Formats a UTC ISO 8601 string into a local 12-hour time string (e.g., 04:06 PM).
 * @param isoTime The UTC ISO 8601 time string.
 * @return A String with the formatted local time.
 */
String formatTime(const String &isoTime) {
  // 1. Get the UTC epoch time
  time_t utc_raw = utcIsoToEpoch(isoTime); 

  // 2. Determine the base AEST time (UTC + 10 hours)
  time_t aest_raw = utc_raw; 

  // 3. Convert AEST to a struct tm to check DST status
  struct tm local_t;
  gmtime_r(&aest_raw, &local_t); 

  // 4. Manually check for DST and get the extra offset (0 or 3600 seconds)
  long dst_offset = getSydneyDSTOffset(&local_t);

  // 5. Calculate the final local epoch time (AEST + DST offset)
  time_t local_raw = aest_raw + dst_offset;
  
  // 6. Convert final local epoch to broken-down time struct for formatting
  localtime_r(&local_raw, &local_t); 

  int hour = local_t.tm_hour;
  int minute = local_t.tm_min;
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

  // The local time struct already has the correct DST offset applied by the ESP32
  // after calling setLocalTimezone(), so we format it directly.

  int hour = timeinfo.tm_hour;
  int minute = timeinfo.tm_min;
  String ampm = "AM";

  // 12-hour clock conversion logic
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