#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <Arduino.h>
#include <time.h>

// Define the Sydney timezone string
extern const char *SYDNEY_TZ;

void setLocalTimezone();
time_t utcIsoToEpoch(const String &isoTime);

// New declaration needed for the manual DST offset calculation
long getSydneyDSTOffset(struct tm *local_t);

String formatTime(const String &isoTime);
String getCurrentTimeString();

#endif // TIME_UTILS_H