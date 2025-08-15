// led_controller.cpp
#include "led_controller.h"
#include "rom/ets_sys.h"

const char* LEDController::TAG = "LED_CTRL";

// LED to register bit mapping (matching your MicroPython code)
const uint16_t LEDController::led_to_register[13] = {
    0x0000,                  // LED 0 (not used)
    0b0100000000000000,      // LED 1
    0b0010000000000000,      // LED 2
    0b0001000000000000,      // LED 3
    0b0000100000000000,      // LED 4
    0b0000010000000000,      // LED 5
    0b0000001000000000,      // LED 6
    0b0000000000000010,      // LED 7
    0b0000000000000100,      // LED 8
    0b0000000000001000,      // LED 9
    0b0000000000010000,      // LED 10
    0b0000000000100000,      // LED 11
    0b0000000001000000,      // LED 12
};

LEDController::LEDController() : initialized_(false) {
}

LEDController::~LEDController() {
    if (initialized_) {
        clear_all();
        gpio_reset_pin(CLOCK_PIN);
        gpio_reset_pin(DATA_PIN);
        gpio_reset_pin(LATCH_PIN);
        gpio_reset_pin(RESET_PIN);
        gpio_reset_pin(OE_PIN);
    }
}

bool LEDController::initialize() {
    ESP_LOGI(TAG, "Initializing LED controller pins");
    
    // Configure all pins as outputs
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << CLOCK_PIN) | (1ULL << DATA_PIN) | 
                           (1ULL << LATCH_PIN) | (1ULL << RESET_PIN) | (1ULL << OE_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Initialize pins to safe state (matching your MicroPython init_pins())
    gpio_set_level(OE_PIN, 1);        // Disable output (high impedance)
    gpio_set_level(LATCH_PIN, 0);
    gpio_set_level(CLOCK_PIN, 0);
    gpio_set_level(DATA_PIN, 0);
    
    // Reset shift register
    pulse_pin(RESET_PIN);             // Put low to reset shift register
    gpio_set_level(RESET_PIN, 1);     // Keep high to prevent resetting
    
    // Put 0s in latches
    pulse_pin(LATCH_PIN);
    
    // Enable output
    gpio_set_level(OE_PIN, 0);
    
    initialized_ = true;
    ESP_LOGI(TAG, "LED controller initialized successfully");
    return true;
}

void LEDController::pulse_pin(gpio_num_t pin) {
    ets_delay_us(PULSE_DELAY_US);
    gpio_set_level(pin, 1);
    ets_delay_us(PULSE_DELAY_US * 2);
    gpio_set_level(pin, 0);
    ets_delay_us(PULSE_DELAY_US);
}

void LEDController::feed_register(uint16_t value) {
    // Send each bit (from LSB to MSB) - matching your MicroPython code
    for (int i = 0; i < 16; i++) {
        int bit = (value >> i) & 1;  // Extract the bit (starting from LSB)
        gpio_set_level(DATA_PIN, bit);
        pulse_pin(CLOCK_PIN);
    }
    gpio_set_level(DATA_PIN, 0);     // Reset to 0 in idle state
}

void LEDController::latch_data() {
    pulse_pin(LATCH_PIN);
}

void LEDController::set_leds(uint16_t pattern) {
    if (!initialized_) {
        ESP_LOGE(TAG, "LED controller not initialized");
        return;
    }
    
    feed_register(pattern);
    latch_data();
    ESP_LOGD(TAG, "Set LEDs with pattern: 0x%04X", pattern);
}

void LEDController::set_single_led(int led_number) {
    if (!initialized_) {
        ESP_LOGE(TAG, "LED controller not initialized");
        return;
    }
    
    if (led_number < 1 || led_number > 12) {
        ESP_LOGW(TAG, "Invalid LED number: %d (valid range: 1-12)", led_number);
        return;
    }
    
    uint16_t pattern = led_to_register[led_number];
    set_leds(pattern);
    ESP_LOGI(TAG, "Set LED %d (pattern: 0x%04X)", led_number, pattern);
}

void LEDController::clear_all() {
    if (!initialized_) {
        ESP_LOGE(TAG, "LED controller not initialized");
        return;
    }
    
    set_leds(0x0000);
    ESP_LOGI(TAG, "All LEDs cleared");
}

void LEDController::test_pattern() {
    if (!initialized_) {
        ESP_LOGE(TAG, "LED controller not initialized");
        return;
    }
    
    // Create pattern for LEDs 1, 3, 5, 7, 9, 11
    uint16_t pattern = led_to_register[1] | led_to_register[3] | led_to_register[5] | 
                       led_to_register[7] | led_to_register[9] | led_to_register[11];
    
    set_leds(pattern);
    ESP_LOGI(TAG, "Test pattern set: LEDs 1,3,5,7,9,11 (pattern: 0x%04X)", pattern);
}

void LEDController::set_from_array(const bool states[], size_t count) {
    uint16_t pattern = 0;
    for (size_t i = 0; i < count && i < 13; i++) {
        if (states[i]) {
            pattern |= led_to_register[i];
        }
    }
    set_leds(pattern);
}