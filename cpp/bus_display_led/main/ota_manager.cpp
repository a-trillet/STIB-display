// ota_manager.cpp
#include "ota_manager.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include <algorithm>
#include <sstream>

const char* OTAManager::TAG = "OTA_MGR";

OTAManager::OTAManager(WiFiManager& wifi_manager, LEDController& led_controller)
    : wifi_manager_(wifi_manager), led_controller_(led_controller),
      initialized_(false), update_in_progress_(false),
      current_version_(""), last_check_status_("Never checked"),
      ota_timer_(nullptr) {
    
    // Get current firmware version
    const esp_app_desc_t* app_desc = esp_app_get_description();
    if (app_desc) {
        current_version_ = std::string(app_desc->version);
    }
}

OTAManager::~OTAManager() {
    stop_ota_timer();
}

bool OTAManager::initialize() {
    ESP_LOGI(TAG, "Initializing OTA manager");
    ESP_LOGI(TAG, "Current firmware version: %s", current_version_.c_str());
    
    // Validate OTA partitions
    if (validate_update_partition() != ESP_OK) {
        ESP_LOGE(TAG, "OTA partition validation failed");
        return false;
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "OTA manager initialized successfully");
    return true;
}

void OTAManager::start_ota_timer() {
    if (ota_timer_) {
        ESP_LOGW(TAG, "OTA timer already running");
        return;
    }
    
    ota_timer_ = xTimerCreate("ota_check_timer",
                             pdMS_TO_TICKS(OTA_CHECK_INTERVAL_MS),
                             pdTRUE,  // Auto-reload
                             this,    // Timer ID (pass this object)
                             ota_timer_callback);
    
    if (ota_timer_ && xTimerStart(ota_timer_, 0) == pdPASS) {
        ESP_LOGI(TAG, "OTA check timer started (interval: %d minutes)", 
                 OTA_CHECK_INTERVAL_MS / (60 * 1000));
    } else {
        ESP_LOGE(TAG, "Failed to start OTA timer");
    }
}

void OTAManager::stop_ota_timer() {
    if (ota_timer_) {
        xTimerStop(ota_timer_, 0);
        xTimerDelete(ota_timer_, 0);
        ota_timer_ = nullptr;
        ESP_LOGI(TAG, "OTA timer stopped");
    }
}

esp_err_t OTAManager::check_for_updates() {
    if (!initialized_) {
        ESP_LOGE(TAG, "OTA manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (update_in_progress_) {
        ESP_LOGW(TAG, "Update already in progress");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!wifi_manager_.is_connected()) {
        last_check_status_ = "No internet connection";
        ESP_LOGW(TAG, "Cannot check for updates - no internet connection");
        return ESP_ERR_WIFI_NOT_CONNECT;
    }
    
    ESP_LOGI(TAG, "Checking for firmware updates...");
    last_check_status_ = "Checking for updates...";
    
    // Prepare JSON payload
    std::string mac = wifi_manager_.get_mac_address();
    std::string hardware = get_hardware_info();
    
    cJSON* json = cJSON_CreateObject();
    if (!json) {
        last_check_status_ = "JSON creation failed";
        return ESP_FAIL;
    }
    
    cJSON* json_hardware = cJSON_CreateString(hardware.c_str());
    cJSON* json_mac = cJSON_CreateString(mac.c_str());
    
    if (!json_hardware || !json_mac) {
        cJSON_Delete(json);
        last_check_status_ = "JSON creation failed";
        return ESP_FAIL;
    }
    
    cJSON_AddItemToObject(json, "hardware", json_hardware);
    cJSON_AddItemToObject(json, "mac", json_mac);
    
    char* json_string = cJSON_Print(json);
    if (!json_string) {
        cJSON_Delete(json);
        last_check_status_ = "JSON serialization failed";
        return ESP_FAIL;
    }
    
    std::string post_data(json_string);
    
    cJSON_Delete(json);
    free(json_string);
    
    // Query server for version info
    std::string url = "https://transport.trillet.be/api/update/versions";
    std::string response = http_post_json(url, post_data);
    
    if (response.empty()) {
        last_check_status_ = "Server communication failed";
        ESP_LOGE(TAG, "Failed to get response from update server");
        return ESP_FAIL;
    }
    
    // Parse response
    VersionInfo version_info;
    if (!parse_version_response(response, version_info)) {
        last_check_status_ = "Invalid server response";
        ESP_LOGE(TAG, "Failed to parse version response");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Server version: %s, Current version: %s", 
             version_info.app_version.c_str(), current_version_.c_str());
    
    // Check if update is needed
    if (!version_is_newer(version_info.app_version, current_version_)) {
        last_check_status_ = "Firmware up to date (v" + current_version_ + ")";
        ESP_LOGI(TAG, "Firmware is up to date");
        return ESP_OK;
    }
    
    // Perform update
    ESP_LOGI(TAG, "New firmware available: %s", version_info.app_version.c_str());
    last_check_status_ = "Updating to v" + version_info.app_version;
    
    esp_err_t ret = perform_ota_update(version_info.app_url);
    if (ret == ESP_OK) {
        last_check_status_ = "Update successful - restarting...";
        ESP_LOGI(TAG, "OTA update completed successfully, restarting...");
        vTaskDelay(pdMS_TO_TICKS(2000)); // Give time for logging
        esp_restart();
    } else {
        last_check_status_ = "Update failed";
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t OTAManager::perform_ota_update(const std::string& update_url) {
    if (update_in_progress_) {
        return ESP_ERR_INVALID_STATE;
    }
    
    update_in_progress_ = true;
    
    ESP_LOGI(TAG, "Starting OTA update from: %s", update_url.c_str());
    
    // Show update indication on LEDs (flash pattern)
    //led_controller_.test_pattern();
    
    esp_http_client_config_t config = {};
    config.url = update_url.c_str();
    config.cert_pem = nullptr; // Use cert bundle
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = OTA_RECV_TIMEOUT_MS;
    config.keep_alive_enable = true;
    config.event_handler = ota_http_event_handler;
    config.user_data = this;
    
    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &config;
    ota_config.http_client_init_cb = nullptr;
    ota_config.bulk_flash_erase = true;
    ota_config.partial_http_download = true;
    ota_config.max_http_request_size = OTA_BUFFER_SIZE;
    
    esp_err_t ret = esp_https_ota(&ota_config);
    
    update_in_progress_ = false;
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA update successful");
        // Clear LEDs before restart
        led_controller_.clear_all();
    } else {
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ret));
        // Show error pattern (all LEDs on)
        led_controller_.set_leds(0xFFFF);
        vTaskDelay(pdMS_TO_TICKS(3000));
        led_controller_.clear_all();
    }
    
    return ret;
}

std::string OTAManager::get_hardware_info() {
    // Return hardware identifier (you can customize this)
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    std::ostringstream hw_info;
    hw_info << "ESP32";
    
    if (chip_info.model == CHIP_ESP32) {
        hw_info << "_WROOM";
    }
    
    return hw_info.str();
}

std::string OTAManager::http_post_json(const std::string& url, const std::string& json_data) {
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = OTA_RECV_TIMEOUT_MS;
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return "";
    }
    
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_data.c_str(), json_data.length());
    
    esp_err_t err = esp_http_client_perform(client);
    std::string response;
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d", status_code, content_length);
        
        if (status_code == 200 && content_length > 0) {
            char* buffer = (char*)malloc(content_length + 1);
            if (buffer) {
                int read_len = esp_http_client_read_response(client, buffer, content_length);
                buffer[read_len] = '\0';
                response = std::string(buffer);
                free(buffer);
            }
        } else if (status_code != 200) {
            ESP_LOGW(TAG, "HTTP error response: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    return response;
}

bool OTAManager::parse_version_response(const std::string& json_response, VersionInfo& version_info) {
    cJSON* root = cJSON_Parse(json_response.c_str());
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return false;
    }
    
    cJSON* app_version = cJSON_GetObjectItem(root, "app_version");
    cJSON* app_url = cJSON_GetObjectItem(root, "app_url");
    
    bool success = false;
    if (cJSON_IsString(app_version) && cJSON_IsString(app_url)) {
        version_info.app_version = std::string(app_version->valuestring);
        version_info.app_url = std::string(app_url->valuestring);
        success = true;
    } else {
        ESP_LOGE(TAG, "Invalid JSON structure in version response");
    }
    
    cJSON_Delete(root);
    return success;
}

bool OTAManager::version_is_newer(const std::string& server_version, const std::string& current_version) {
    // Simple string comparison - you can implement semantic versioning if needed
    // For now, just check if versions are different
    bool different = (server_version != current_version);
    
    ESP_LOGD(TAG, "Version comparison: server='%s', current='%s', different=%s",
             server_version.c_str(), current_version.c_str(), different ? "true" : "false");
    
    return different;
}

esp_err_t OTAManager::validate_update_partition() {
    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "No OTA update partition found");
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "OTA update partition found: %s (size: %d bytes)", 
             update_partition->label, update_partition->size);
    
    return ESP_OK;
}

void OTAManager::ota_timer_callback(TimerHandle_t timer) {
    OTAManager* ota_manager = static_cast<OTAManager*>(pvTimerGetTimerID(timer));
    if (ota_manager) {
        ESP_LOGI(TAG, "Periodic OTA check triggered");
        ota_manager->check_for_updates();
    }
}

esp_err_t OTAManager::ota_http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", 
                     evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}