// wifi_manager.h
#pragma once

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "storage_manager.h"
#include <string>

#define WIFI_AP_SSID "Bus-Display-LED"
#define WIFI_AP_PASSWORD ""  // Open network
#define WIFI_CONNECT_TIMEOUT_MS (15000)
#define WIFI_MAX_RETRY 5
#define WIFI_RECONNECT_DELAY_MS (10000)

class WiFiManager {
public:
    WiFiManager(StorageManager& storage);
    ~WiFiManager();
    
    bool initialize();
    bool start_ap_mode();
    bool stop_ap_mode();
    bool connect_sta(const std::string& ssid, const std::string& password, bool save = true);
    bool disconnect_sta();
    bool is_connected();
    std::string get_mac_address();
    std::string get_ip_address();
    
    // Auto-reconnect management
    void start_auto_connect_task();
    void stop_auto_connect_task();
    
    // Getters for status
    std::string get_connection_status();
    std::string get_current_ssid();
    bool is_ap_active() const { return ap_mode_active_; }
    
private:
    StorageManager& storage_;
    bool initialized_;
    bool ap_mode_active_;
    bool sta_connected_;
    bool auto_connect_enabled_;
    std::string current_ssid_;
    std::string connection_status_;
    int retry_count_;
    
    TaskHandle_t auto_connect_task_handle_;
    
    // Tasks
    static void auto_connect_task(void* parameter);
    
    // Event handlers
    static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data);
    static void ip_event_handler(void* arg, esp_event_base_t event_base,
                                 int32_t event_id, void* event_data);
    
    EventGroupHandle_t wifi_event_group_;
    static const int WIFI_CONNECTED_BIT = BIT0;
    static const int WIFI_FAIL_BIT = BIT1;
    static const int WIFI_STOP_RECONNECT_BIT = BIT2;
    
    static const char* TAG;
};