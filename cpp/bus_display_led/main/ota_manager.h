// ota_manager.h
#pragma once

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "cJSON.h"
#include "wifi_manager.h"
#include "led_controller.h"
#include <string>

#define OTA_CHECK_INTERVAL_MS (60 * 60 * 1000) // 1 hour
#define OTA_RECV_TIMEOUT_MS (5000)
#define OTA_BUFFER_SIZE (1024)

class OTAManager {
public:
    OTAManager(WiFiManager& wifi_manager, LEDController& led_controller);
    ~OTAManager();
    
    bool initialize();
    void start_ota_timer();
    void stop_ota_timer();
    
    // Manual OTA check/update
    esp_err_t check_for_updates();
    esp_err_t perform_ota_update(const std::string& update_url);
    
    // Status
    bool is_update_in_progress() const { return update_in_progress_; }
    std::string get_current_version() const { return current_version_; }
    std::string get_last_check_status() const { return last_check_status_; }

private:
    WiFiManager& wifi_manager_;
    LEDController& led_controller_;
    
    bool initialized_;
    bool update_in_progress_;
    std::string current_version_;
    std::string last_check_status_;
    
    TimerHandle_t ota_timer_;
    
    // Version info from server
    struct VersionInfo {
        std::string app_version;
        std::string app_url;
    };
    
    // Helper methods
    std::string get_hardware_info();
    std::string http_post_json(const std::string& url, const std::string& json_data);
    bool parse_version_response(const std::string& json_response, VersionInfo& version_info);
    bool version_is_newer(const std::string& server_version, const std::string& current_version);
    esp_err_t validate_update_partition();
    
    // Static callbacks
    static void ota_timer_callback(TimerHandle_t timer);
    static esp_err_t ota_http_event_handler(esp_http_client_event_t *evt);
    
    static const char* TAG;
};