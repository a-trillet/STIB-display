// web_server.h
#pragma once

#include "esp_http_server.h"
#include "esp_log.h"
#include "wifi_manager.h"
#include <string>
#include <functional>

class WebServer {
public:
    WebServer(WiFiManager& wifi_manager);
    ~WebServer();
    
    bool start();
    bool stop();
    bool is_running() const { return server_ != nullptr; }
    
    // Callback for WiFi configuration
    void set_wifi_config_callback(std::function<void(const std::string&, const std::string&)> callback) {
        wifi_config_callback_ = callback;
    }
    
private:
    WiFiManager& wifi_manager_;
    httpd_handle_t server_;
    std::function<void(const std::string&, const std::string&)> wifi_config_callback_;
    
    // HTTP handlers
    static esp_err_t root_handler(httpd_req_t *req);
    static esp_err_t apply_handler(httpd_req_t *req);
    static esp_err_t status_handler(httpd_req_t *req);
    static esp_err_t style_handler(httpd_req_t *req);
    
    // Helper functions
    std::string generate_main_page();
    std::string generate_status_html();
    std::string url_decode(const std::string& str);
    bool parse_post_data(const std::string& data, std::string& ssid, std::string& password);
    
    static const char* TAG;
    static const char* CSS_STYLE;
};