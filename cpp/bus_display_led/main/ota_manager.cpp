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
    
    ESP_LOGI(TAG, "Preparing OTA request - MAC: %s, Hardware: %s", mac.c_str(), hardware.c_str());
    
    cJSON* json = cJSON_CreateObject();
    if (!json) {
        last_check_status_ = "JSON creation failed";
        ESP_LOGE(TAG, "Failed to create JSON object");
        return ESP_FAIL;
    }
    
    cJSON* json_hardware = cJSON_CreateString(hardware.c_str());
    cJSON* json_mac = cJSON_CreateString(mac.c_str());
    
    if (!json_hardware || !json_mac) {
        cJSON_Delete(json);
        last_check_status_ = "JSON creation failed";
        ESP_LOGE(TAG, "Failed to create JSON string objects");
        return ESP_FAIL;
    }
    
    cJSON_AddItemToObject(json, "hardware", json_hardware);
    cJSON_AddItemToObject(json, "mac", json_mac);
    
    char* json_string = cJSON_Print(json);
    if (!json_string) {
        cJSON_Delete(json);
        last_check_status_ = "JSON serialization failed";
        ESP_LOGE(TAG, "Failed to serialize JSON");
        return ESP_FAIL;
    }
    
    std::string post_data(json_string);
    ESP_LOGI(TAG, "JSON payload: %s", post_data.c_str());
    
    cJSON_Delete(json);
    free(json_string);
    
    // Query server for version info
    std::string url = "https://transport.trillet.be/api/update/versions";
    ESP_LOGI(TAG, "Making HTTP POST request to: %s", url.c_str());
    std::string response = http_post_json(url, post_data);

    ESP_LOGI(TAG, "OTA server response (length=%zu): '%s'", response.length(), response.c_str());
    
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
    
    esp_http_client_config_t config = {};
    config.url = update_url.c_str();
    config.cert_pem = nullptr;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 60000; // 60 second timeout
    config.keep_alive_enable = false; // Disable keep-alive
    config.event_handler = ota_http_event_handler;
    config.user_data = this;
    config.buffer_size = 8192; // Larger buffer
    config.buffer_size_tx = 1024;
    
    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &config;
    ota_config.http_client_init_cb = nullptr;
    ota_config.bulk_flash_erase = true;
    ota_config.partial_http_download = false; // DISABLE partial downloads
    ota_config.max_http_request_size = 0; // Single request
    
    ESP_LOGI(TAG, "Starting single-request OTA download...");
    esp_err_t ret = esp_https_ota(&ota_config);
    
    update_in_progress_ = false;
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA update successful");
        led_controller_.clear_all();
    } else {
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ret));
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
    hw_info << "ESP32_WROOM";
    
    // if (chip_info.model == CHIP_ESP32) {
    //     hw_info << "_WROOM";
    // }
    
    return hw_info.str();
}

std::string OTAManager::http_post_json(const std::string& url, const std::string& json_data) {
    ESP_LOGI(TAG, "Starting HTTP POST to: %s", url.c_str());
    ESP_LOGI(TAG, "POST body: %s", json_data.c_str());
    
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = OTA_RECV_TIMEOUT_MS;
    config.buffer_size = 1024;
    config.buffer_size_tx = 1024;
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return "";
    }
    
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "User-Agent", "ESP32-BusDisplay/1.0");
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Connection", "close");
    esp_http_client_set_post_field(client, json_data.c_str(), json_data.length());
    
    ESP_LOGI(TAG, "Opening HTTP connection...");
    esp_err_t err = esp_http_client_open(client, json_data.length());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return "";
    }
    
    ESP_LOGI(TAG, "Writing POST data...");
    int wlen = esp_http_client_write(client, json_data.c_str(), json_data.length());
    if (wlen < 0) {
        ESP_LOGE(TAG, "Failed to write POST data");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return "";
    }
    ESP_LOGI(TAG, "Wrote %d bytes", wlen);
    
    ESP_LOGI(TAG, "Fetching headers...");
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    
    ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d", status_code, content_length);
    
    std::string response;
    response.reserve(1024);
    
    if (status_code == 200) {
        if (content_length > 0) {
            ESP_LOGI(TAG, "Reading response with known length: %d", content_length);
            char* buffer = (char*)malloc(content_length + 1);
            if (buffer) {
                int total_read = 0;
                while (total_read < content_length) {
                    int read = esp_http_client_read_response(client, buffer + total_read, content_length - total_read);
                    if (read <= 0) {
                        ESP_LOGW(TAG, "Read returned %d, breaking", read);
                        break;
                    }
                    total_read += read;
                    ESP_LOGI(TAG, "Read %d bytes, total: %d/%d", read, total_read, content_length);
                }
                buffer[total_read] = '\0';
                response.assign(buffer, total_read);
                free(buffer);
                ESP_LOGI(TAG, "Successfully read %d bytes", total_read);
            } else {
                ESP_LOGE(TAG, "Failed to allocate buffer for response");
            }
        } else {
            ESP_LOGI(TAG, "Reading chunked response...");
            char* chunk_buffer = (char*)malloc(512);
            if (chunk_buffer) {
                int total_read = 0;
                while (true) {
                    int read = esp_http_client_read_response(client, chunk_buffer, 511);
                    if (read <= 0) {
                        ESP_LOGI(TAG, "No more data (read=%d)", read);
                        break;
                    }
                    chunk_buffer[read] = '\0';
                    response.append(chunk_buffer, read);
                    total_read += read;
                    ESP_LOGI(TAG, "Read chunk: %d bytes, total: %d", read, total_read);
                    if (total_read >= 2048) {
                        ESP_LOGW(TAG, "Response too large, truncating");
                        break;
                    }
                }
                free(chunk_buffer);
            } else {
                ESP_LOGE(TAG, "Failed to allocate chunk buffer");
            }
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed with status: %d", status_code);
    }
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    
    ESP_LOGI(TAG, "HTTP POST completed, response length: %zu", response.length());
    ESP_LOGI(TAG, "Response content: '%s'", response.c_str());
    
    return response;
}

bool OTAManager::parse_version_response(const std::string& json_response, VersionInfo& version_info) {
    ESP_LOGI(TAG, "Parsing version response: %s", json_response.c_str());
    
    cJSON* root = cJSON_Parse(json_response.c_str());
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON response: %s", cJSON_GetErrorPtr());
        return false;
    }
    
    cJSON* app_version = cJSON_GetObjectItem(root, "app_version");
    cJSON* app_url = cJSON_GetObjectItem(root, "app_url");
    
    bool success = false;
    if (cJSON_IsString(app_version) && cJSON_IsString(app_url)) {
        version_info.app_version = std::string(app_version->valuestring);
        version_info.app_url = std::string(app_url->valuestring);
        success = true;
        ESP_LOGI(TAG, "Parsed version info - Version: %s, URL: %s", 
                 version_info.app_version.c_str(), version_info.app_url.c_str());
    } else {
        ESP_LOGE(TAG, "Invalid JSON structure in version response");
        ESP_LOGE(TAG, "app_version is %s: %s", 
                 cJSON_IsString(app_version) ? "string" : "not string",
                 app_version ? (cJSON_IsString(app_version) ? app_version->valuestring : "not a string") : "null");
        ESP_LOGE(TAG, "app_url is %s: %s", 
                 cJSON_IsString(app_url) ? "string" : "not string",
                 app_url ? (cJSON_IsString(app_url) ? app_url->valuestring : "not a string") : "null");
    }
    
    cJSON_Delete(root);
    return success;
}

bool OTAManager::version_is_newer(const std::string& server_version, const std::string& current_version) {
    // Simple string comparison - you can implement semantic versioning if needed
    // For now, just check if versions are different
    bool different = (server_version != current_version);
    
    ESP_LOGI(TAG, "Version comparison: server='%s', current='%s', different=%s",
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
    static int last_progress_log = 0;
    static int bytes_downloaded = 0;
    
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED - OTA download starting");
            bytes_downloaded = 0;
            last_progress_log = 0;
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            if (strcmp(evt->header_key, "Content-Length") == 0) {
                ESP_LOGI(TAG, "OTA file size: %s bytes", evt->header_value);
            }
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", 
                     evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            bytes_downloaded += evt->data_len;
            // Log progress every 10KB to avoid spam
            if (bytes_downloaded - last_progress_log >= 10240) {
                ESP_LOGI(TAG, "OTA Progress: %d bytes downloaded", bytes_downloaded);
                last_progress_log = bytes_downloaded;
            }
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d, total=%d", evt->data_len, bytes_downloaded);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH - OTA download completed, total: %d bytes", bytes_downloaded);
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}