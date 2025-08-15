// led_controller.h
#pragma once

#include "driver/gpio.h"
#include "esp_log.h"
#include <cstdint>

// GPIO Pin Definitions
#define CLOCK_PIN GPIO_NUM_18
#define DATA_PIN GPIO_NUM_15
#define LATCH_PIN GPIO_NUM_5
#define RESET_PIN GPIO_NUM_19
#define OE_PIN GPIO_NUM_2

// Timing
#define PULSE_DELAY_US 100

class LEDController {
public:
    LEDController();
    ~LEDController();
    
    bool initialize();
    void set_leds(uint16_t pattern);
    void set_single_led(int led_number);
    void clear_all();
    void test_pattern();
    void set_from_array(const bool states[], size_t count);
    
private:
    void pulse_pin(gpio_num_t pin);
    void feed_register(uint16_t value);
    void latch_data();
    bool initialized_;
    
    static const char* TAG;
    
    // LED to register bit mapping (from your original code)
    static const uint16_t led_to_register[13];
};