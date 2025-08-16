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
#include "ota_manager.h"

static const char* TAG = "MAIN";

// Global objects
LEDController* led_controller = nullptr;
StorageManager* storage_manager = nullptr;
WiFiManager* wifi_manager = nullptr;
WebServer* web_server = nullptr;
LEDUpdater* led_updater = nullptr;
OTAManager* ota_manager = nullptr;

// WiFi configuration callback from web server
void wifi_config_callback(const std::string& ssid, const std::string& password) {
    ESP_LOGI(TAG, "New WiFi credentials received: %s", ssid.c_str());
    
    // Connect with save=true to store credentials
    wifi_manager->connect_sta(ssid, password, true);
}

// Task for initial OTA check (runs once)
void initial_ota_check_task(void* param) {
    ESP_LOGI(TAG, "Initial OTA check task started");
    
    // Wait for WiFi connection
    int max_wait = 30; // 30 seconds
    while (!wifi_manager->is_connected() && max_wait > 0) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        max_wait--;
    }
    
    if (wifi_manager->is_connected()) {
        ESP_LOGI(TAG, "Performing initial OTA check...");
        ota_manager->check_for_updates();
    } else {
        ESP_LOGI(TAG, "No WiFi connection, skipping initial OTA check");
    }
    
    ESP_LOGI(TAG, "Initial OTA check task completed");
    vTaskDelete(NULL);
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
    
    led_controller->set_all(false); // Ensure LEDs start off
    led_controller->set_all(true); // check if LEDs are working
    vTaskDelay(pdMS_TO_TICKS(1000));    
    led_controller->set_all(false); // turn off LEDs after check
    //led_controller->test_sequence(); // check if LEDs are working
    
    // Create WiFi manager
    wifi_manager = new WiFiManager(*storage_manager);
    if (!wifi_manager->initialize()) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager");
        return;
    }
    ESP_LOGI(TAG, "WiFi manager initialized successfully");

    // Initialize OTA manager
    ota_manager = new OTAManager(*wifi_manager, *led_controller);
    if (!ota_manager->initialize()) {
        ESP_LOGE(TAG, "Failed to initialize OTA manager");
        return;
    }
    ESP_LOGI(TAG, "OTA manager initialized successfully");

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
    web_server->set_ota_manager(*ota_manager);
    
    if (!web_server->start()) {
        ESP_LOGE(TAG, "Failed to start web server");
        return;
    }
    ESP_LOGI(TAG, "Web server started successfully");
    ESP_LOGI(TAG, "Connect to WiFi '%s' and go to http://192.168.4.1", WIFI_AP_SSID);
    
    // // Create LED updater
    // led_updater = new LEDUpdater(*led_controller, *wifi_manager);

    // // Start LED update task
    // xTaskCreate([](void* param) {
    //     LEDUpdater* updater = static_cast<LEDUpdater*>(param);
    //     while (true) {
    //         if (wifi_manager->is_connected()) {
    //             updater->fetch_and_update();
    //         }
    //         vTaskDelay(pdMS_TO_TICKS(5000));
    //     }
    // }, "led_update_task", 4096, led_updater, 5, NULL);

    // Start OTA update timer (checks every hour)
    ota_manager->start_ota_timer();
    
    ESP_LOGI(TAG, "System initialization complete!");
    ESP_LOGI(TAG, "Device MAC: %s", wifi_manager->get_mac_address().c_str());
    ESP_LOGI(TAG, "Current firmware version: %s", ota_manager->get_current_version().c_str());
    
    // Create separate task for initial OTA check with larger stack
    xTaskCreate(initial_ota_check_task, "initial_ota_check", 8192, NULL, 5, NULL);
    
    // Main loop - monitor system status
    while (1) {
        ESP_LOGI(TAG, "Status - AP: %s, STA: %s, Web: %s, OTA: %s", 
                 wifi_manager->is_ap_active() ? "ON" : "OFF",
                 wifi_manager->is_connected() ? "CONNECTED" : "DISCONNECTED",
                 web_server->is_running() ? "RUNNING" : "STOPPED",
                 ota_manager->get_last_check_status().c_str());
        
        if (wifi_manager->is_connected()) {
            ESP_LOGI(TAG, "WiFi Status: %s, IP: %s", 
                     wifi_manager->get_connection_status().c_str(),
                     wifi_manager->get_ip_address().c_str());
        }
        
        vTaskDelay(pdMS_TO_TICKS(30000)); // Status update every 30 seconds
    }
}