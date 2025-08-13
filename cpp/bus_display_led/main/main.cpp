// main.cpp
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "led_controller.h"
#include "wifi_manager.h"
#include "web_server.h"

static const char* TAG = "MAIN";

// Global objects
LEDController* led_controller = nullptr;
WiFiManager* wifi_manager = nullptr;
WebServer* web_server = nullptr;

// LED pattern task
void led_pattern_task(void* parameter) {
    bool leds_on = false;
    
    ESP_LOGI(TAG, "LED pattern task started - toggling LEDs 1,3,5,7,9,11 every 5 seconds");
    
    while (1) {
        if (!leds_on) {
            ESP_LOGI(TAG, "Turning ON LEDs 1,3,5,7,9,11");
            led_controller->test_pattern();
            leds_on = true;
        } else {
            ESP_LOGI(TAG, "Turning OFF all LEDs");
            led_controller->clear_all();
            leds_on = false;
        }
        
        // Wait 5 seconds
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// WiFi configuration callback
void wifi_config_callback(const std::string& ssid, const std::string& password) {
    ESP_LOGI(TAG, "Attempting to connect to WiFi: %s", ssid.c_str());
    
    // Start connection in background
    wifi_manager->connect_sta(ssid, password);
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Bus Display LED - ESP-IDF Version Starting");
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
    
    // Initialize NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Create LED controller
    led_controller = new LEDController();
    if (!led_controller->initialize()) {
        ESP_LOGE(TAG, "Failed to initialize LED controller");
        return;
    }
    ESP_LOGI(TAG, "LED controller initialized successfully");
    
    // Clear all LEDs initially
    led_controller->clear_all();
    
    // Create WiFi manager
    wifi_manager = new WiFiManager();
    if (!wifi_manager->initialize()) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager");
        return;
    }
    ESP_LOGI(TAG, "WiFi manager initialized successfully");
    
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
    
    // Start LED pattern task
    xTaskCreate(&led_pattern_task, "led_pattern_task", 2048, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "System initialization complete!");
    ESP_LOGI(TAG, "Device MAC: %s", wifi_manager->get_mac_address().c_str());
    
    // Main loop - just monitor system status
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