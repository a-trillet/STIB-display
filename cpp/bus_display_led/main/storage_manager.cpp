// storage_manager.cpp
#include "storage_manager.h"

const char* StorageManager::TAG = "STORAGE";

StorageManager::StorageManager() : nvs_handle_(0), initialized_(false) {}

StorageManager::~StorageManager() {
    if (initialized_) {
        nvs_close(nvs_handle_);
    }
}

bool StorageManager::initialize() {
    ESP_LOGI(TAG, "Initializing NVS storage");
    
    // Open NVS handle
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(ret));
        return false;
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "Storage manager initialized successfully");
    return true;
}

bool StorageManager::save_wifi_credentials(const std::string& ssid, const std::string& password) {
    if (!initialized_) {
        ESP_LOGE(TAG, "Storage manager not initialized");
        return false;
    }
    
    esp_err_t ret;
    
    // Save SSID
    ret = nvs_set_str(nvs_handle_, NVS_WIFI_SSID, ssid.c_str());
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error saving SSID: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Save password
    ret = nvs_set_str(nvs_handle_, NVS_WIFI_PASSWORD, password.c_str());
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error saving password: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Commit changes
    ret = nvs_commit(nvs_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error committing changes: %s", esp_err_to_name(ret));
        return false;
    }
    
    ESP_LOGI(TAG, "WiFi credentials saved successfully: %s", ssid.c_str());
    return true;
}

bool StorageManager::load_wifi_credentials(std::string& ssid, std::string& password) {
    if (!initialized_) {
        ESP_LOGE(TAG, "Storage manager not initialized");
        return false;
    }
    
    size_t required_size = 0;
    esp_err_t ret;
    
    // Get SSID size
    ret = nvs_get_str(nvs_handle_, NVS_WIFI_SSID, NULL, &required_size);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGD(TAG, "WiFi SSID not found in NVS");
        return false;
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error reading SSID size: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Read SSID
    char* ssid_buf = new char[required_size];
    ret = nvs_get_str(nvs_handle_, NVS_WIFI_SSID, ssid_buf, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error reading SSID: %s", esp_err_to_name(ret));
        delete[] ssid_buf;
        return false;
    }
    ssid = std::string(ssid_buf);
    delete[] ssid_buf;
    
    // Get password size
    ret = nvs_get_str(nvs_handle_, NVS_WIFI_PASSWORD, NULL, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error reading password size: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Read password
    char* pass_buf = new char[required_size];
    ret = nvs_get_str(nvs_handle_, NVS_WIFI_PASSWORD, pass_buf, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error reading password: %s", esp_err_to_name(ret));
        delete[] pass_buf;
        return false;
    }
    password = std::string(pass_buf);
    delete[] pass_buf;
    
    ESP_LOGI(TAG, "WiFi credentials loaded successfully: %s", ssid.c_str());
    return true;
}

bool StorageManager::has_wifi_credentials() {
    std::string ssid, password;
    return load_wifi_credentials(ssid, password);
}

bool StorageManager::clear_wifi_credentials() {
    if (!initialized_) {
        ESP_LOGE(TAG, "Storage manager not initialized");
        return false;
    }
    
    esp_err_t ret;
    
    ret = nvs_erase_key(nvs_handle_, NVS_WIFI_SSID);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Error erasing SSID: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = nvs_erase_key(nvs_handle_, NVS_WIFI_PASSWORD);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Error erasing password: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = nvs_commit(nvs_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error committing erase: %s", esp_err_to_name(ret));
        return false;
    }
    
    ESP_LOGI(TAG, "WiFi credentials cleared");
    return true;
}