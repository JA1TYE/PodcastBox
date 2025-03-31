/* Play an MP3 file from HTTP

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"

#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "board.h"

#include "podcasttask.h"
#include "ui_control.h"
#include "home_control.h"
#include "message_queue.h"

#include "esp_netif.h"

static const char *TAG = "HTTP_MP3_EXAMPLE";

extern "C" void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(esp_netif_init());

    esp_log_level_set("*", ESP_LOG_DEBUG);
    ESP_LOGI(TAG, "Start and wait for Wi-Fi network");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    periph_wifi_cfg_t wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);
    
    // Create two mailboxes (queues)
    QueueHandle_t mailboxes[2];
    mailboxes[PODCAST_QUEUE] = xQueueCreate(1, sizeof(uint8_t));
    mailboxes[UI_QUEUE] = xQueueCreate(10, sizeof(uint8_t));

    if (mailboxes[PODCAST_QUEUE] == NULL || mailboxes[UI_QUEUE] == NULL) {
        ESP_LOGE(TAG, "Failed to create mailboxes");
        return;
    }

    //Create a task to manage the UI
    xTaskCreatePinnedToCore(ui_task, "ui_task", 8192, (void*)mailboxes, 5, NULL, 1);
    //Create a task to control appliances
    xTaskCreatePinnedToCore(home_task, "homeTask", 8192, (void*)mailboxes, 5, NULL, 1);
    //Create a task to play the podcast
    xTaskCreatePinnedToCore(podcastTask, "podcastTask", 8192, (void*)mailboxes, 5, NULL, 0);

    while(1){
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    /* Stop all peripherals before removing the listener */
    esp_periph_set_stop_all(set);
    esp_periph_set_destroy(set);

}

