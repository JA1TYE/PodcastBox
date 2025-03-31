#include "driver/gpio.h"
#include "esp_timer.h"
#include "button.h"

// Constructor
Button::Button(gpio_num_t gpio, bool active_low) 
    : gpio_num(gpio), active_low(active_low), current_state(false), last_state(false),
        pressed_flag(false), long_pressed_flag(false), press_count(0), press_start_time(0) {
    // Initialize GPIO
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << gpio_num);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
}

// Update the button state
void Button::update() {
    bool raw_state = gpio_get_level(gpio_num);
    bool new_state = active_low ? !raw_state : raw_state; // Invert if active_low is true

    // Debounce: Update the state only if the same state is read consecutively
    if (new_state == last_state) {
        press_count++;
    } else {
        press_count = 0;
    }

    if (press_count >= DEBOUNCE_COUNT) {
        if (new_state != current_state) {
            current_state = new_state;

            if (current_state == 1) { // Button pressed
                press_start_time = esp_timer_get_time();
            } else { // Button released
                pressed_flag = true;
            }
        }
    }

    last_state = new_state;
}

// Check if the button was short-pressed
bool Button::is_pressed() {
    if (pressed_flag) {
        pressed_flag = false; // Clear the flag
        return true;
    }
    return false;
}