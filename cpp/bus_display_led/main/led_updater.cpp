#include "led_updater.h"

const char* LEDUpdater::TAG = "LED_UPDATER";

LEDUpdater::LEDUpdater(LEDController& led_controller, WiFiManager& wifi_manager)
    : led_controller_(led_controller), wifi_manager_(wifi_manager) {}


std::string LEDUpdater::http_get(const std::string& url) {
    esp_http_client_config_t config{};
    config.url = url.c_str();
    // config.cert_pem = root_cert_pem_start; // Use proper CA in production
    config.disable_auto_redirect = false;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    std::string response;

    if (err == ESP_OK) {
        int content_length = esp_http_client_fetch_headers(client);
        if (content_length > 0) {
            char* buffer = (char*)malloc(content_length + 1);
            esp_http_client_read_response(client, buffer, content_length);
            buffer[content_length] = '\0';
            response = std::string(buffer);
            free(buffer);
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return response;
}

bool LEDUpdater::parse_json_to_array(const char* json, bool states[], size_t max_leds) {
    cJSON* root = cJSON_Parse(json);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return false;
    }

    cJSON* strips = cJSON_GetObjectItem(root, "strips");
    if (!strips) {
        ESP_LOGW(TAG, "No 'strips' in JSON");
        cJSON_Delete(root);
        return false;
    }

    memset(states, 0, max_leds * sizeof(bool));

    cJSON* strip = nullptr;
    cJSON_ArrayForEach(strip, strips) {
        cJSON* v = cJSON_GetObjectItem(strip, "v");
        if (!cJSON_IsArray(v)) continue;

        int i = 0;
        cJSON* val = nullptr;
        cJSON_ArrayForEach(val, v) {
            if (i >= max_leds) break;
            if (cJSON_IsTrue(val)) {
                states[i] = true;
            }
            i++;
        }
    }

    cJSON_Delete(root);
    return true;
}

esp_err_t LEDUpdater::fetch_and_update() {
    std::string mac = wifi_manager_.get_mac_address();
    std::string url = "https://transport.trillet.be/api/esp/ledstrips?mac=" + mac;
    std::string response = http_get(url);

    if (response.empty()) {
        ESP_LOGE(TAG, "Empty response from server");
        return ESP_FAIL;
    }

    bool led_states[12] = {false};
    if (!parse_json_to_array(response.c_str(), led_states, 12)) {
        ESP_LOGE(TAG, "Failed to parse LED states");
        return ESP_FAIL;
    }

    led_controller_.set_from_array(led_states, 12);
    return ESP_OK;
}