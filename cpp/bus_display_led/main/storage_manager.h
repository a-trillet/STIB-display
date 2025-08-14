// storage_manager.h
#pragma once

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string>

#define NVS_NAMESPACE "bus_display"
#define NVS_WIFI_SSID "wifi_ssid"
#define NVS_WIFI_PASSWORD "wifi_password"

class StorageManager {
public:
    StorageManager();
    ~StorageManager();
    
    bool initialize();
    bool save_wifi_credentials(const std::string& ssid, const std::string& password);
    bool load_wifi_credentials(std::string& ssid, std::string& password);
    bool clear_wifi_credentials();
    bool has_wifi_credentials();
    
private:
    nvs_handle_t nvs_handle_;
    bool initialized_;
    static const char* TAG;
};