// main.cpp
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "led_controller.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "led_updater.h"
#include "storage_manager.h"

static const char* TAG = "MAIN";

// Global objects
LEDController* led_controller = nullptr;
StorageManager* storage_manager = nullptr;
WiFiManager* wifi_manager = nullptr;
WebServer* web_server = nullptr;
LEDUpdater* led_updater = nullptr;


// WiFi configuration callback from web server
void wifi_config_callback(const std::string& ssid, const std::string& password) {
    ESP_LOGI(TAG, "New WiFi credentials received: %s", ssid.c_str());
    
    // Connect with save=true to store credentials
    wifi_manager->connect_sta(ssid, password, true);
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Bus Display LED - ESP-IDF Version Starting");
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
    
    // Initialize NVS (required for WiFi and storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create storage manager
    storage_manager = new StorageManager();
    if (!storage_manager->initialize()) {
        ESP_LOGE(TAG, "Failed to initialize storage manager");
        return;
    }
    ESP_LOGI(TAG, "Storage manager initialized successfully");
    
    // Create LED controller
    led_controller = new LEDController();
    if (!led_controller->initialize()) {
        ESP_LOGE(TAG, "Failed to initialize LED controller");
        return;
    }
    ESP_LOGI(TAG, "LED controller initialized successfully");
    
    led_controller->clear_all(); // Ensure LEDs start off
    
    // Create WiFi manager
    wifi_manager = new WiFiManager(*storage_manager);
    if (!wifi_manager->initialize()) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager");
        return;
    }
    ESP_LOGI(TAG, "WiFi manager initialized successfully");

    // Check if we have saved WiFi credentials and try to connect
    if (storage_manager->has_wifi_credentials()) {
        ESP_LOGI(TAG, "Found saved WiFi credentials, attempting connection...");
        std::string ssid, password;
        if (storage_manager->load_wifi_credentials(ssid, password)) {
            wifi_manager->connect_sta(ssid, password, false); // Don't save again
        }
    } else {
        ESP_LOGI(TAG, "No saved WiFi credentials found");
    }
    
    // Start auto-connect task for handling reconnections
    wifi_manager->start_auto_connect_task();
    
    // Start AP mode
    if (!wifi_manager->start_ap_mode()) {
        ESP_LOGE(TAG, "Failed to start AP mode");
        return;
    }
    ESP_LOGI(TAG, "AP mode started: %s", WIFI_AP_SSID);
    
    // Create and start web server
    web_server = new WebServer(*wifi_manager);
    web_server->set_wifi_config_callback(wifi_config_callback);
    
    if (!web_server->start()) {
        ESP_LOGE(TAG, "Failed to start web server");
        return;
    }
    ESP_LOGI(TAG, "Web server started successfully");
    ESP_LOGI(TAG, "Connect to WiFi '%s' and go to http://192.168.4.1", WIFI_AP_SSID);
    

    led_updater = new LEDUpdater(*led_controller, *wifi_manager);

    xTaskCreate([](void* param) {
        LEDUpdater* updater = static_cast<LEDUpdater*>(param);
        while (true) {
            updater->fetch_and_update();
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }, "led_update_task", 4096, led_updater, 5, NULL);
    
    ESP_LOGI(TAG, "System initialization complete!");
    ESP_LOGI(TAG, "Device MAC: %s", wifi_manager->get_mac_address().c_str());
    
    // Main loop - monitor system status
    while (1) {
        ESP_LOGI(TAG, "Status - AP: %s, STA: %s, Web: %s", 
                 wifi_manager->is_ap_active() ? "ON" : "OFF",
                 wifi_manager->is_connected() ? "CONNECTED" : "DISCONNECTED",
                 web_server->is_running() ? "RUNNING" : "STOPPED");
        
        if (wifi_manager->is_connected()) {
            ESP_LOGI(TAG, "WiFi Status: %s, IP: %s", 
                     wifi_manager->get_connection_status().c_str(),
                     wifi_manager->get_ip_address().c_str());
        }
        
        vTaskDelay(pdMS_TO_TICKS(30000)); // Status update every 30 seconds
    }
}
