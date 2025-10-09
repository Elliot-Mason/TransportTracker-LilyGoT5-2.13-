#ifndef DATA_FETCHER_H
#define DATA_FETCHER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "TimeUtils.h" // For utcIsoToEpoch and formatTime

// Struct to hold the relevant train data
struct TrainData {
  String originStation;
  String originPlatform;
  String originTime;
  String destStation;
  String destPlatform;
  String destTime;
  String routeLabel;
  bool success; // True if data was fetched and parsed successfully
  String errorMessage; // Used if success is false
};

// Function to get the label from the route type
String getRouteLabel(const String &routeType);

// Main function to fetch and parse data
TrainData fetchTrainData(const String& origin_code, const String& destination_code, const String& api_url_base);

#endif // DATA_FETCHER_H