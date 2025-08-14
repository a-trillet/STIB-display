// wifi_manager.cpp
#include "wifi_manager.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <cstring>

const char* WiFiManager::TAG = "WIFI_MGR";

WiFiManager::WiFiManager(StorageManager& storage) 
    : storage_(storage), initialized_(false), ap_mode_active_(false), sta_connected_(false),
      auto_connect_enabled_(false), current_ssid_(""), connection_status_("Not connected"), 
      retry_count_(0), auto_connect_task_handle_(nullptr), wifi_event_group_(nullptr) {
}

WiFiManager::~WiFiManager() {
    stop_auto_connect_task();
    if (wifi_event_group_) {
        vEventGroupDelete(wifi_event_group_);
    }
    if (initialized_) {
        esp_wifi_deinit();
    }
}

bool WiFiManager::initialize() {
    ESP_LOGI(TAG, "Initializing WiFi manager");
    
    // Initialize TCP/IP
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create event group
    wifi_event_group_ = xEventGroupCreate();
    if (!wifi_event_group_) {
        ESP_LOGE(TAG, "Failed to create event group");
        return false;
    }
    
    // Create network interfaces
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Register event handlers
    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                            ESP_EVENT_ANY_ID,
                                            &wifi_event_handler,
                                            this,
                                            nullptr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = esp_event_handler_instance_register(IP_EVENT,
                                            IP_EVENT_STA_GOT_IP,
                                            &ip_event_handler,
                                            this,
                                            nullptr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Set WiFi mode to APSTA (both AP and STA)
    ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi set mode failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi start failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "WiFi manager initialized successfully");
    return true;
}

bool WiFiManager::start_ap_mode() {
    if (!initialized_) {
        ESP_LOGE(TAG, "WiFi manager not initialized");
        return false;
    }
    
    ESP_LOGI(TAG, "Starting AP mode: %s", WIFI_AP_SSID);
    
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.ap.ssid, WIFI_AP_SSID);
    wifi_config.ap.ssid_len = strlen(WIFI_AP_SSID);
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    wifi_config.ap.beacon_interval = 100;
    
    esp_err_t ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi AP config failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    ap_mode_active_ = true;
    ESP_LOGI(TAG, "AP mode started successfully");
    return true;
}

bool WiFiManager::stop_ap_mode() {
    if (!ap_mode_active_) {
        return true;
    }
    
    ESP_LOGI(TAG, "Stopping AP mode");
    ap_mode_active_ = false;
    return true;
}

bool WiFiManager::connect_sta(const std::string& ssid, const std::string& password, bool save) {
    if (!initialized_) {
        ESP_LOGE(TAG, "WiFi manager not initialized");
        return false;
    }
    
    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid.c_str());
    current_ssid_ = ssid;
    connection_status_ = "Connecting to " + ssid + "...";
    retry_count_ = 0;
    
    // Save credentials if requested
    if (save) {
        if (!storage_.save_wifi_credentials(ssid, password)) {
            ESP_LOGW(TAG, "Failed to save WiFi credentials");
        }
    }
    
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid.c_str(), sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password.c_str(), sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi STA config failed: %s", esp_err_to_name(ret));
        connection_status_ = "Failed to configure WiFi";
        return false;
    }
    
    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(ret));
        connection_status_ = "Failed to connect to " + ssid;
        return false;
    }
    
    return true; // Connection attempt started, result will come via events
}

bool WiFiManager::disconnect_sta() {
    if (!sta_connected_) {
        return true;
    }
    
    ESP_LOGI(TAG, "Disconnecting from WiFi");
    esp_wifi_disconnect();
    sta_connected_ = false;
    connection_status_ = "Disconnected";
    return true;
}

bool WiFiManager::is_connected() {
    return sta_connected_;
}

std::string WiFiManager::get_mac_address() {
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    return std::string(mac_str);
}

std::string WiFiManager::get_ip_address() {
    if (!sta_connected_) {
        return "0.0.0.0";
    }
    
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        return "0.0.0.0";
    }
    
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif, &ip_info);
    
    char ip_str[16];
    esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
    
    return std::string(ip_str);
}

std::string WiFiManager::get_connection_status() {
    return connection_status_;
}

std::string WiFiManager::get_current_ssid() {
    return current_ssid_;
}

void WiFiManager::start_auto_connect_task() {
    if (auto_connect_task_handle_) {
        ESP_LOGW(TAG, "Auto-connect task already running");
        return;
    }
    
    auto_connect_enabled_ = true;
    xTaskCreate(&auto_connect_task, "wifi_auto_connect", 4096, this, 5, &auto_connect_task_handle_);
    ESP_LOGI(TAG, "Auto-connect task started");
}

void WiFiManager::stop_auto_connect_task() {
    if (!auto_connect_task_handle_) {
        return;
    }
    
    auto_connect_enabled_ = false;
    xEventGroupSetBits(wifi_event_group_, WIFI_STOP_RECONNECT_BIT);
    
    // Wait for task to finish
    vTaskDelay(pdMS_TO_TICKS(100));
    
    if (auto_connect_task_handle_) {
        vTaskDelete(auto_connect_task_handle_);
        auto_connect_task_handle_ = nullptr;
    }
    
    ESP_LOGI(TAG, "Auto-connect task stopped");
}

void WiFiManager::auto_connect_task(void* parameter) {
    WiFiManager* wifi_mgr = static_cast<WiFiManager*>(parameter);
    
    ESP_LOGI(TAG, "Auto-connect task running");
    
    while (wifi_mgr->auto_connect_enabled_) {
        // Check if we should stop
        EventBits_t bits = xEventGroupWaitBits(wifi_mgr->wifi_event_group_,
                                              WIFI_STOP_RECONNECT_BIT,
                                              pdTRUE, pdFALSE,
                                              pdMS_TO_TICKS(1000));
        
        if (bits & WIFI_STOP_RECONNECT_BIT) {
            break;
        }
        
        // If not connected, try to connect using saved credentials
        if (!wifi_mgr->sta_connected_) {
            std::string ssid, password;
            if (wifi_mgr->storage_.load_wifi_credentials(ssid, password)) {
                ESP_LOGI(TAG, "Auto-reconnecting to saved WiFi: %s", ssid.c_str());
                wifi_mgr->connect_sta(ssid, password, false); // Don't save again
                
                // Wait for connection result or timeout
                EventBits_t result = xEventGroupWaitBits(wifi_mgr->wifi_event_group_,
                                                        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | WIFI_STOP_RECONNECT_BIT,
                                                        pdTRUE, pdFALSE,
                                                        pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
                
                if (result & WIFI_STOP_RECONNECT_BIT) {
                    break;
                }
                
                if (!(result & WIFI_CONNECTED_BIT)) {
                    ESP_LOGW(TAG, "Auto-reconnect failed, retrying in %d seconds", WIFI_RECONNECT_DELAY_MS / 1000);
                    vTaskDelay(pdMS_TO_TICKS(WIFI_RECONNECT_DELAY_MS));
                }
            } else {
                ESP_LOGD(TAG, "No saved WiFi credentials found");
                vTaskDelay(pdMS_TO_TICKS(WIFI_RECONNECT_DELAY_MS));
            }
        } else {
            // Connected, check periodically
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
    
    ESP_LOGI(TAG, "Auto-connect task finished");
    wifi_mgr->auto_connect_task_handle_ = nullptr;
    vTaskDelete(NULL);
}

void WiFiManager::wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data) {
    WiFiManager* wifi_mgr = static_cast<WiFiManager*>(arg);
    
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi station started");
                break;
                
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi station connected");
                break;
                
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* disconnected = (wifi_event_sta_disconnected_t*) event_data;
                ESP_LOGW(TAG, "WiFi disconnected, reason: %d", disconnected->reason);
                wifi_mgr->sta_connected_ = false;
                wifi_mgr->connection_status_ = "Disconnected from " + wifi_mgr->current_ssid_;
                
                if (wifi_mgr->wifi_event_group_) {
                    xEventGroupSetBits(wifi_mgr->wifi_event_group_, WiFiManager::WIFI_FAIL_BIT);
                }
                break;
            }
            
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
                ESP_LOGI(TAG, "Station connected to AP, AID=%d", event->aid);
                break;
            }
            
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
                ESP_LOGI(TAG, "Station disconnected from AP, AID=%d", event->aid);
                break;
            }
            
            default:
                break;
        }
    }
}

void WiFiManager::ip_event_handler(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data) {
    WiFiManager* wifi_mgr = static_cast<WiFiManager*>(arg);
    
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        
        wifi_mgr->sta_connected_ = true;
        wifi_mgr->connection_status_ = "Connected to " + wifi_mgr->current_ssid_;
        wifi_mgr->retry_count_ = 0;
        
        if (wifi_mgr->wifi_event_group_) {
            xEventGroupSetBits(wifi_mgr->wifi_event_group_, WiFiManager::WIFI_CONNECTED_BIT);
        }
    }
}
