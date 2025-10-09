#include "DataFetcher.h"

// Helper to map route type to a shorter label
String getRouteLabel(const String &routeType)
{
  if (routeType == "BMT")
    return "Intercity";
  if (routeType == "T1")
    return "T1";
  return routeType;
}

// Lambda to extract platform or station (local to this file)
auto extractPlatform = [](const String &name) -> String
{
  int idx = name.indexOf("Platform ");
  if (idx >= 0)
  {
    int end = name.indexOf(",", idx);
    if (end < 0)
      end = name.length();
    return name.substring(idx, end);
  }
  return "";
};

auto extractStation = [](const String &name) -> String
{
  int idx = name.indexOf(",");
  if (idx >= 0)
    return name.substring(0, idx);
  return name;
};

/**
 * @brief Fetches train data from the API and parses the JSON response.
 * @param origin_code The station code for the origin.
 * @param destination_code The station code for the destination.
 * @param api_url_base The base URL for the API.
 * @return A TrainData struct containing the result.
 */
TrainData fetchTrainData(const String& origin_code, const String& destination_code, const String& api_url_base)
{
  TrainData result = { .success = false };

  WiFiClientSecure client;
  client.setInsecure(); // accept any SSL cert
  HTTPClient http;

  // Build API URL dynamically
  String currentUrl = api_url_base + origin_code + "&name_destination=" + destination_code;
  Serial.println("API Request: " + currentUrl);

  http.begin(client, currentUrl.c_str());
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK)
  {
    String payload = http.getString();
    // Increase size for larger payloads, though 8192 seemed to work.
    StaticJsonDocument<8192> doc; 
    DeserializationError error = deserializeJson(doc, payload);

    if (!error)
    {
      JsonObject nextTrain;
      time_t nowTime = time(nullptr);  // Current UTC epoch time

      // Find the next train that has not yet departed
      for (JsonObject train : doc.as<JsonArray>())
      {
        String depTimeStr = train["legs"][0]["origin"]["departureTimePlanned"].as<String>();
        time_t depTime = utcIsoToEpoch(depTimeStr); // CORRECT: Convert UTC string to UTC epoch
        
        if (depTime >= nowTime)
        {
          nextTrain = train;
          break;
        }
      }

      // Fallback: if no future trains found, use the first one in the list (might be the one just departed)
      if (nextTrain.isNull())
        nextTrain = doc[0].as<JsonObject>();

      if (!nextTrain.isNull()) {
        JsonObject origin = nextTrain["legs"][0]["origin"];
        JsonObject destination = nextTrain["legs"][0]["destination"];

        String originName = origin["name"].as<String>();
        String originTimeRaw = origin["departureTimePlanned"].as<String>();
        String destName = destination["name"].as<String>();
        String destTimeRaw = destination["arrivalTimePlanned"].as<String>();
        String routeType = nextTrain["legs"][0]["transportation"]["disassembledName"].as<String>();
        
        // Populate the result struct
        result.originStation = extractStation(originName);
        result.originPlatform = extractPlatform(originName);
        result.originTime = formatTime(originTimeRaw);
        result.destStation = extractStation(destName);
        result.destPlatform = extractPlatform(destName);
        result.destTime = formatTime(destTimeRaw);
        result.routeLabel = getRouteLabel(routeType);
        result.success = true;
      } else {
        result.errorMessage = "No train data found.";
      }
    }
    else
    {
      result.errorMessage = "JSON Error: " + String(error.c_str());
    }
  }
  else
  {
    result.errorMessage = "HTTP Error " + String(httpCode);
  }
  http.end();
  
  return result;
}