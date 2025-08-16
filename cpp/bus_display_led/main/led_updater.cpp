#include "led_updater.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include <vector>
#include <algorithm>

const char* LEDUpdater::TAG = "LED_UPDATER";

LEDUpdater::LEDUpdater(LEDController& led_controller, WiFiManager& wifi_manager)
    : led_controller_(led_controller), wifi_manager_(wifi_manager),
      chunk_buffer_(nullptr), chunk_buffer_size_(512) 
{
    chunk_buffer_ = (char*)malloc(chunk_buffer_size_);
    if (!chunk_buffer_) {
        ESP_LOGE(TAG, "Failed to allocate chunk buffer");
        chunk_buffer_size_ = 0;
    }
}

LEDUpdater::~LEDUpdater() {
    if (chunk_buffer_) {
        free(chunk_buffer_);
        chunk_buffer_ = nullptr;
        chunk_buffer_size_ = 0;
    }
}

std::string LEDUpdater::http_get(const std::string& url) {
    ESP_LOGI(TAG, "Starting HTTP GET request to: %s", url.c_str());

    if (!wifi_manager_.is_connected()) {
        ESP_LOGW(TAG, "Wi-Fi disconnected, skipping HTTP request");
        return "";
    }

    esp_http_client_config_t config{};
    config.url = url.c_str();
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 3000; // 3s timeout
    config.buffer_size = 1024;
    config.buffer_size_tx = 1024;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return "";
    }

    esp_http_client_set_header(client, "User-Agent", "ESP32-BusDisplay/1.0");
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Connection", "close");

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return "";
    }

    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);

    //ESP_LOGI(TAG, "HTTP Status: %d, Content-Length: %d", status_code, content_length);

    std::string response;
    response.reserve(4096);  // pre-allocate for efficiency

    if (status_code == 200) {
        if (content_length > 0) {
            char* buffer = (char*)malloc(content_length + 1);
            if (buffer) {
                int total_read = 0;
                while (total_read < content_length) {
                    int read = esp_http_client_read_response(client, buffer + total_read, content_length - total_read);
                    if (read <= 0) break;
                    total_read += read;
                }
                buffer[total_read] = '\0';
                response.assign(buffer, total_read);
                free(buffer);
            } else {
                ESP_LOGE(TAG, "Failed to allocate buffer for fixed-length response");
            }
        } else if (chunk_buffer_ && chunk_buffer_size_ > 0) {
            int total_read = 0;
            while (true) {
                int read = esp_http_client_read_response(client, chunk_buffer_, chunk_buffer_size_ - 1);
                if (read <= 0) break;
                chunk_buffer_[read] = '\0';
                response.append(chunk_buffer_, read);
                total_read += read;
                if (total_read >= 4096) {
                    ESP_LOGW(TAG, "Response too large, truncating");
                    break;
                }
            }
        } else {
            ESP_LOGE(TAG, "No chunk buffer available for reading response");
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed with status: %d", status_code);
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    ESP_LOGI(TAG, "Full response (%d bytes)", response.length());
    if (response.empty()) {
        ESP_LOGW(TAG, "Received empty response from server");
    }

    return response;
}


bool LEDUpdater::parse_json_to_strips(const char* json, std::vector<StripData>& strips_out) {
    //ESP_LOGI(TAG, "Parsing JSON: %s", json);

    cJSON* root = cJSON_Parse(json);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return false;
    }

    cJSON* strips = cJSON_GetObjectItem(root, "strips");
    if (!cJSON_IsArray(strips)) {
        ESP_LOGW(TAG, "No 'strips' array in JSON");
        cJSON_Delete(root);
        return false;
    }

    strips_out.clear();

    cJSON* strip = nullptr;
    cJSON_ArrayForEach(strip, strips) {
        StripData data{};

        cJSON* h = cJSON_GetObjectItem(strip, "h");
        if (!cJSON_IsNumber(h)) {
            ESP_LOGW(TAG, "Strip without valid 'h', skipping");
            continue;
        }
        data.h = h->valueint;

        cJSON* v = cJSON_GetObjectItem(strip, "v");
        if (!cJSON_IsArray(v)) {
            ESP_LOGW(TAG, "Strip without 'v' array, skipping h=%d", data.h);
            continue;
        }

        int idx = 0;
        cJSON* val = nullptr;
        cJSON_ArrayForEach(val, v) {
            uint8_t state = (cJSON_IsTrue(val) || (cJSON_IsNumber(val) && val->valueint != 0)) ? 1 : 0;
            data.values.push_back(state);
            //ESP_LOGI(TAG, "Strip h=%d, v[%d]=%d", data.h, idx, state);
            idx++;
        }

        strips_out.push_back(std::move(data));
    }

    cJSON_Delete(root);

    // sort by h
    std::sort(strips_out.begin(), strips_out.end(), 
              [](const StripData& a, const StripData& b) { return a.h < b.h; });

    //ESP_LOGI(TAG, "Parsed %d strips", (int)strips_out.size());
    return !strips_out.empty();
}


esp_err_t LEDUpdater::fetch_and_update() {
    std::string mac = wifi_manager_.get_mac_address();
    std::string url = "https://transport.trillet.be/api/esp/ledstrips?mac=" + mac;
    std::string response = http_get(url);

    if (response.empty()) {
        ESP_LOGE(TAG, "Empty response from server for url: %s", url.c_str());
        return ESP_FAIL;
    }

    std::vector<StripData> strips;
    if (!parse_json_to_strips(response.c_str(), strips)) {
        ESP_LOGE(TAG, "Failed to parse LED states");
        return ESP_FAIL;
    }

    //ESP_LOGI(TAG, "Parsed %zu strips", strips.size());

    if (strips.empty()) {
        return ESP_OK; // nothing to update
    }

    // Dynamically allocate array for rows
    size_t row_count = strips.size();
    bool (*rows)[12] = new bool[row_count][12]();

    for (size_t r = 0; r < row_count; r++) {
        const auto& s = strips[r];
        //ESP_LOGI(TAG, "Updating strip h=%d with %zu values", s.h, s.values.size());

        for (size_t i = 0; i < s.values.size() && i < 12; i++) {
            rows[r][i] = (s.values[i] != 0);
        }
    }

    // Feed all rows at once
    led_controller_.set_rows(rows, row_count);

    delete[] rows;
    return ESP_OK;
}