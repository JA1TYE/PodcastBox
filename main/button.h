#ifndef _BUTTON_H_
#define _BUTTON_H_

#include "driver/gpio.h"

class Button {
private:
    gpio_num_t gpio_num;
    bool active_low;
    bool current_state;
    bool last_state;
    bool pressed_flag;
    bool long_pressed_flag;
    int press_count;
    int64_t press_start_time;
    const int64_t LONG_PRESS_THRESHOLD = 2000000; // 2 seconds (in microseconds)
    const int DEBOUNCE_COUNT = 2; // Count for debounce handling

public:
    // Constructor
    Button(gpio_num_t gpio, bool active_low = false);

    // Update the button state
    void update();

    // Check if the button was short-pressed
    bool is_pressed();

    // Check if the button was long-pressed
    bool is_long_pressed();
};

#endif // _BUTTON_H_