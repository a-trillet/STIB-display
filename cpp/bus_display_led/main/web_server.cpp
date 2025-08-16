// web_server.cpp
#include "web_server.h"
#include <cstring>
#include <sstream>

const char* WebServer::TAG = "WEB_SRV";

const char* WebServer::CSS_STYLE = R"(
body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    margin: 1rem;
    background-color: #f8f9fa;
    color: #212529;
}

h1, h2 {
    font-weight: 600;
}

button {
    background-color: #007bff;
    border: none;
    color: white;
    padding: 0.5rem 1rem;
    font-size: 1rem;
    border-radius: 0.25rem;
    cursor: pointer;
}

button:hover {
    background-color: #0056b3;
}

button.ota-button {
    background-color: #28a745;
}

button.ota-button:hover {
    background-color: #218838;
}

button.ota-button:disabled {
    background-color: #6c757d;
    cursor: not-allowed;
}

input[type="text"], input[type="password"] {
    padding: 0.375rem 0.75rem;
    font-size: 1rem;
    border: 1px solid #ced4da;
    border-radius: 0.25rem;
    width: 100%;
    max-width: 300px;
    box-sizing: border-box;
}

form {
    max-width: 400px;
}

#status {
    margin-bottom: 1rem;
    padding: 0.75rem;
    border-radius: 0.25rem;
    background-color: #e9ecef;
}

.ota-section {
    margin-top: 2rem;
    padding: 1rem;
    border: 1px solid #dee2e6;
    border-radius: 0.25rem;
    background-color: #ffffff;
}

.ota-status {
    margin: 0.5rem 0;
    padding: 0.5rem;
    border-radius: 0.25rem;
    background-color: #f8f9fa;
    font-family: monospace;
}

.register-section {
    margin-top: 2rem;
    padding-top: 1rem;
    border-top: 1px solid #dee2e6;
}

.info-row {
    display: flex;
    justify-content: space-between;
    margin: 0.25rem 0;
}

.info-label {
    font-weight: 600;
}
)";

WebServer::WebServer(WiFiManager& wifi_manager) 
    : wifi_manager_(wifi_manager), ota_manager_(nullptr), server_(nullptr) {
}

WebServer::~WebServer() {
    stop();
}

bool WebServer::start() {
    if (server_) {
        ESP_LOGW(TAG, "Server already running");
        return true;
    }
    
    ESP_LOGI(TAG, "Starting HTTP server");
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.task_priority = 5;
    config.stack_size = 8192;
    config.max_uri_handlers = 10;
    config.lru_purge_enable = true;
    
    esp_err_t ret = httpd_start(&server_, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Register URI handlers
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &root_uri);
    
    httpd_uri_t apply_uri = {
        .uri = "/apply",
        .method = HTTP_POST,
        .handler = apply_handler,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &apply_uri);
    
    httpd_uri_t status_uri = {
        .uri = "/status",
        .method = HTTP_GET,
        .handler = status_handler,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &status_uri);
    
    httpd_uri_t style_uri = {
        .uri = "/style.css",
        .method = HTTP_GET,
        .handler = style_handler,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &style_uri);
    
    httpd_uri_t ota_check_uri = {
        .uri = "/ota_check",
        .method = HTTP_POST,
        .handler = ota_check_handler,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &ota_check_uri);
    
    ESP_LOGI(TAG, "HTTP server started successfully");
    return true;
}

bool WebServer::stop() {
    if (!server_) {
        return true;
    }
    
    ESP_LOGI(TAG, "Stopping HTTP server");
    esp_err_t ret = httpd_stop(server_);
    server_ = nullptr;
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop HTTP server: %s", esp_err_to_name(ret));
        return false;
    }
    
    ESP_LOGI(TAG, "HTTP server stopped");
    return true;
}

esp_err_t WebServer::root_handler(httpd_req_t *req) {
    WebServer* server = static_cast<WebServer*>(req->user_ctx);
    
    std::string page = server->generate_main_page();
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, page.c_str(), page.length());
    
    return ESP_OK;
}

esp_err_t WebServer::apply_handler(httpd_req_t *req) {
    WebServer* server = static_cast<WebServer*>(req->user_ctx);
    
    // Read POST data
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    ESP_LOGI(WebServer::TAG, "Received POST data: %s", buf);
    
    // Parse form data
    std::string ssid, password;
    if (!server->parse_post_data(std::string(buf), ssid, password)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid form data");
        return ESP_FAIL;
    }
    
    ESP_LOGI(WebServer::TAG, "WiFi credentials: SSID=%s", ssid.c_str());
    
    // Start WiFi connection
    if (server->wifi_config_callback_) {
        server->wifi_config_callback_(ssid, password);
    }
    
    // Redirect back to main page
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, nullptr, 0);
    
    return ESP_OK;
}

esp_err_t WebServer::status_handler(httpd_req_t *req) {
    WebServer* server = static_cast<WebServer*>(req->user_ctx);
    
    std::string status_html = server->generate_status_html();
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, status_html.c_str(), status_html.length());
    
    return ESP_OK;
}

esp_err_t WebServer::style_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, CSS_STYLE, strlen(CSS_STYLE));
    return ESP_OK;
}

esp_err_t WebServer::ota_check_handler(httpd_req_t *req) {
    WebServer* server = static_cast<WebServer*>(req->user_ctx);
    
    ESP_LOGI(WebServer::TAG, "Manual OTA check requested");
    
    // Check if OTA manager is available
    if (!server->ota_manager_) {
        ESP_LOGE(WebServer::TAG, "OTA manager not available");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA manager not available");
        return ESP_FAIL;
    }
    
    // Check if WiFi is connected
    if (!server->wifi_manager_.is_connected()) {
        ESP_LOGW(WebServer::TAG, "Cannot check OTA - no internet connection");
        
        // Return JSON response
        std::string response = R"({"status": "error", "message": "No internet connection"})";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response.c_str(), response.length());
        return ESP_OK;
    }
    
    // Check if update is already in progress
    if (server->ota_manager_->is_update_in_progress()) {
        ESP_LOGW(WebServer::TAG, "OTA update already in progress");
        
        std::string response = R"({"status": "error", "message": "Update already in progress"})";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response.c_str(), response.length());
        return ESP_OK;
    }
    
    // Start OTA check in background task to avoid blocking HTTP response
    xTaskCreate([](void* param) {
        OTAManager* ota_mgr = static_cast<OTAManager*>(param);
        esp_err_t result = ota_mgr->check_for_updates();
        ESP_LOGI(WebServer::TAG, "Manual OTA check completed with result: %s", 
                 esp_err_to_name(result));
        vTaskDelete(NULL);
    }, "manual_ota_check", 4096, server->ota_manager_, 5, NULL);
    
    // Return immediate response
    std::string response = R"({"status": "success", "message": "OTA check started"})";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response.c_str(), response.length());
    
    return ESP_OK;
}

std::string WebServer::generate_main_page() {
    std::string mac = wifi_manager_.get_mac_address();
    std::string status_html = generate_status_html();
    std::string registration_url = "https://transport.trillet.be/devices/register_new_device?mac=" + mac;
    
    // Get OTA information
    std::string current_version = "Unknown";
    std::string ota_status = "OTA manager not available";
    bool ota_available = false;
    bool update_in_progress = false;
    
    if (ota_manager_) {
        current_version = ota_manager_->get_current_version();
        ota_status = ota_manager_->get_last_check_status();
        ota_available = true;
        update_in_progress = ota_manager_->is_update_in_progress();
    }
    
    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Bus Display LED - Configuration</title>
    <link rel="stylesheet" href="/style.css">
    <script>
        function refreshStatus() {
            fetch('/status').then(r => r.text()).then(d => {
                document.getElementById('status').innerHTML = d;
            });
        }
        
        function checkOTA() {
            const button = document.getElementById('ota-button');
            const statusDiv = document.getElementById('ota-status');
            
            button.disabled = true;
            button.textContent = 'Checking...';
            statusDiv.textContent = 'Checking for updates...';
            
            fetch('/ota_check', {method: 'POST'})
                .then(r => r.json())
                .then(data => {
                    if (data.status === 'success') {
                        statusDiv.textContent = 'Update check started. Please wait...';
                        // Poll for status updates
                        const pollStatus = () => {
                            refreshStatus();
                            setTimeout(pollStatus, 2000);
                        };
                        setTimeout(pollStatus, 2000);
                    } else {
                        statusDiv.textContent = 'Error: ' + data.message;
                        button.disabled = false;
                        button.textContent = 'Check for Updates';
                    }
                })
                .catch(err => {
                    statusDiv.textContent = 'Failed to start update check';
                    button.disabled = false;
                    button.textContent = 'Check for Updates';
                });
        }
        
        setInterval(refreshStatus, 3000);
    </script>
</head>
<body>
    <div id="status">)" << status_html << R"(</div>
    
    <h1>Enter your Wi-Fi credentials</h1>
    <p>These will be stored locally only</p>
    
    <form action="/apply" method="post">
        <label for="ssid">Wi-Fi name (SSID):</label><br>
        <input type="text" id="ssid" name="ssid" required><br><br>
        
        <label for="pswd">Wi-Fi password:</label><br>
        <input type="password" id="pswd" name="pswd"><br><br>
        
        <input type="submit" value="Connect">
    </form>
    
    <div class="ota-section">
        <h2>Firmware Information</h2>
        <div class="info-row">
            <span class="info-label">Current Version:</span>
            <span>)" << current_version << R"(</span>
        </div>
        <div class="info-row">
            <span class="info-label">Device MAC:</span>
            <span>)" << mac << R"(</span>
        </div>
        <div class="ota-status" id="ota-status">)" << ota_status << R"(</div>)";
    
    if (ota_available && wifi_manager_.is_connected()) {
        html << "<button id=\"ota-button\" class=\"ota-button\" onclick=\"checkOTA()\" "
            << (update_in_progress ? "disabled" : "") << ">"
            << (update_in_progress ? "Update in Progress..." : "Check for Updates")
            << "</button>";

    } else if (ota_available && !wifi_manager_.is_connected()) {
        html << R"(<p><em>Connect to WiFi to check for updates</em></p>)";
    }
    
    html << R"(
    </div>
    
    <div class="register-section">
        <h2>Register this device online</h2>
        <p><b>Important:</b> Because this Wi-Fi has no internet, your phone may block the link below.</p>
        <p>Please turn off Wi-Fi (or open the link using mobile data) to register your device:</p>
        <a href=")" << registration_url << R"(" target="_blank">
            <button style="font-size: 18px; padding: 10px 20px;">Register device</button>
        </a>
    </div>
</body>
</html>)";
    
    return html.str();
}

std::string WebServer::generate_status_html() {
    std::ostringstream status;
    
    std::string wifi_status = wifi_manager_.get_connection_status();
    status << "<h2>" << wifi_status << "</h2>";
    
    // Add OTA status if available
    if (ota_manager_) {
        std::string ota_status = ota_manager_->get_last_check_status();
        if (ota_manager_->is_update_in_progress()) {
            status << "<p><strong>OTA Update:</strong> <span style='color: orange;'>IN PROGRESS</span></p>";
        } else {
            status << "<p><strong>Last OTA Check:</strong> " << ota_status << "</p>";
        }
    }
    
    return status.str();
}

std::string WebServer::url_decode(const std::string& str) {
    std::string decoded;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int hex = 0;
            sscanf(str.substr(i + 1, 2).c_str(), "%x", &hex);
            decoded += static_cast<char>(hex);
            i += 2;
        } else if (str[i] == '+') {
            decoded += ' ';
        } else {
            decoded += str[i];
        }
    }
    return decoded;
}

bool WebServer::parse_post_data(const std::string& data, std::string& ssid, std::string& password) {
    // Parse form data: ssid=value&pswd=value
    size_t ssid_pos = data.find("ssid=");
    size_t pswd_pos = data.find("pswd=");
    
    if (ssid_pos == std::string::npos) {
        return false;
    }
    
    // Extract SSID
    size_t ssid_start = ssid_pos + 5; // "ssid=" length
    size_t ssid_end = data.find('&', ssid_start);
    if (ssid_end == std::string::npos) {
        ssid_end = data.length();
    }
    ssid = url_decode(data.substr(ssid_start, ssid_end - ssid_start));
    
    // Extract password
    if (pswd_pos != std::string::npos) {
        size_t pswd_start = pswd_pos + 5; // "pswd=" length
        size_t pswd_end = data.find('&', pswd_start);
        if (pswd_end == std::string::npos) {
            pswd_end = data.length();
        }
        password = url_decode(data.substr(pswd_start, pswd_end - pswd_start));
    }
    
    return !ssid.empty();
}