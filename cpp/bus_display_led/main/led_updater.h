#pragma once
#include "led_controller.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include <string>
#include <vector>

class LEDUpdater {
public:
    LEDUpdater(LEDController& led_controller, WiFiManager& wifi_manager);
    ~LEDUpdater();

    // Fetch JSON from server and update LEDs
    esp_err_t fetch_and_update();

private:
    LEDController& led_controller_;
    WiFiManager& wifi_manager_;

    static const char* TAG;

    // Heap-safe HTTP GET using reusable chunk buffer
    std::string http_get(const std::string& url);

    // Represents one strip parsed from JSON
    struct StripData {
        int h;
        std::vector<uint8_t> values;  // instead of vector<bool>
    };

    // Parse JSON into a vector of StripData, sorted by h
    bool parse_json_to_strips(const char* json, std::vector<StripData>& strips_out);

    // Reusable buffer for reading chunked HTTP responses
    char* chunk_buffer_;
    size_t chunk_buffer_size_;
};