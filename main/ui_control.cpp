#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/gpio.h"

#include <cstring>

#include "ui_control.h"
#include "button.h"

#include "message_queue.h"

static const char *TAG = "UI_CONTROL";

void ui_task(void *pvParameters) {

    QueueHandle_t *mailboxes = (QueueHandle_t *) pvParameters;

    //Create Button objects for GPIO21,47,48
    Button rightButton(GPIO_NUM_21, true); // Active low
    Button leftButton(GPIO_NUM_47, true); // Active low
    Button centerButton(GPIO_NUM_48, true); // Active low

    // Initialize GPIO38-41 as PWM output
    ledc_timer_config_t ledc_timer;
    memset(&ledc_timer, 0, sizeof(ledc_timer));
    ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_timer.timer_num = LEDC_TIMER_0;
    ledc_timer.duty_resolution = LEDC_TIMER_10_BIT;
    ledc_timer.freq_hz = 5000;  // Frequency in Hz
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel[4];
    memset(ledc_channel, 0, sizeof(ledc_channel));

    // Set common values for all channels
    for (int i = 0; i < 4; i++) {
        ledc_channel[i].speed_mode = LEDC_LOW_SPEED_MODE;
        ledc_channel[i].timer_sel = LEDC_TIMER_0;
        ledc_channel[i].channel = (ledc_channel_t)(LEDC_CHANNEL_0 + i);
        ledc_channel[i].gpio_num = (gpio_num_t)(GPIO_NUM_38 + i);
        ledc_channel[i].duty = 0;
        ledc_channel_config(&ledc_channel[i]);
    }

    int lum = 0;
    
    while(1){
        static TickType_t xLastWakeTime = xTaskGetTickCount();

        leftButton.update();
        rightButton.update();
        centerButton.update();

        if (leftButton.is_pressed()) {
            ESP_LOGI(TAG, "Left button pressed");
            // Send message to UI queue
            uint8_t msg = LEFT_BUTTON_PRESSED;
            xQueueSend(mailboxes[UI_QUEUE], &msg, 0);
        }

        if (rightButton.is_pressed()) {
            ESP_LOGI(TAG, "Right button pressed");
            // Send message to UI queue
            uint8_t msg = RIGHT_BUTTON_PRESSED;
            xQueueSend(mailboxes[UI_QUEUE], &msg, 0);
        }

        if (centerButton.is_pressed()) {
            ESP_LOGI(TAG, "Center button pressed");
            // Send message to PODCAST queue
            uint8_t msg = CENTER_BUTTON_PRESSED;
            xQueueSend(mailboxes[PODCAST_QUEUE], &msg, 0);
        }

        vTaskDelayUntil(&xLastWakeTime, 10 / portTICK_PERIOD_MS);
    }

}

