# main/CMakeLists.txt (save this as main/CMakeLists.txt)
idf_component_register(
    SRCS 
        "main.cpp"
        "led_controller.cpp"
        "led_updater.cpp"
        "wifi_manager.cpp"
        "web_server.cpp"
        "storage_manager.cpp"
    INCLUDE_DIRS "."
    REQUIRES 
        driver
        esp_common
        esp_wifi
        esp_netif
        esp_event
        esp_http_server
        nvs_flash
        log
        json
        esp_http_client
        mbedtls
)