// led_updater.h
#pragma once
#include "led_controller.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include <string>

class LEDUpdater {
public:
    LEDUpdater(LEDController& led_controller, WiFiManager& wifi_manager);
    ~LEDUpdater() = default;

    // Fetch JSON from server and update LEDs
    esp_err_t fetch_and_update();

private:
    LEDController& led_controller_;
    WiFiManager& wifi_manager_;

    static const char* TAG;

    std::string http_get(const std::string& url);
    bool parse_json_to_array(const char* json, bool states[], size_t max_leds);
};
