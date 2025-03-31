#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_http_client.h"

#include <string>

#include "ui_control.h"

#include "message_queue.h"
#include "home_control.h"

#include "cJSON.h"

#define GOVEE_KEY "YOUR_GOVEE_KEY"

static const char *TAG = "HOME_CONTROL";

void home_task(void *pvParameters) {
    QueueHandle_t *mailboxes = (QueueHandle_t *) pvParameters;
    while(1){
        uint8_t queue_msg;
        if (xQueueReceive(mailboxes[UI_QUEUE], &queue_msg, 0) == pdPASS) {
            switch(queue_msg){
                case RIGHT_BUTTON_PRESSED:
                    ESP_LOGI(TAG, "Right button pressed");
                    make_request("YOUR_DEVICE_ID1", true);
                    make_request("YOUR_DEVICE_ID2", true);
                    break;
                case LEFT_BUTTON_PRESSED:
                    ESP_LOGI(TAG, "Left button pressed");
                    make_request("YOUR_DEVICE_ID1", false);
                    make_request("YOUR_DEVICE_ID2", false);
                    break;
                default:
                    break;
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void make_request(std::string device_address,bool switch_on) {
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return;
    }

    //Add Request ID
    cJSON_AddStringToObject(root, "requestId", "abcd");
    
    //Create payload object
    cJSON *payload = cJSON_CreateObject();
    if (payload == NULL) {
        ESP_LOGE(TAG, "Failed to create payload object");
        cJSON_Delete(root);
        return;
    }
    cJSON_AddStringToObject(payload, "sku", "YOUR_SKU");
    cJSON_AddStringToObject(payload, "device", device_address.c_str());
    //Create Capabilty object
    cJSON *capability = cJSON_CreateObject();
    if (capability == NULL) {
        ESP_LOGE(TAG, "Failed to create capability JSON object");
        cJSON_Delete(root);
        return;
    }
    cJSON_AddStringToObject(capability, "type", "devices.capabilities.on_off");
    cJSON_AddStringToObject(capability, "instance", "powerSwitch");
    cJSON_AddNumberToObject(capability, "value", (switch_on)?1:0);
    //Nest objects
    cJSON_AddItemToObject(payload, "capability", capability);
    cJSON_AddItemToObject(root, "payload", payload);

    // Convert JSON object to string
    char *json_string = cJSON_Print(root);
    if (json_string == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON object");
        cJSON_Delete(root);
        return;
    }
    ESP_LOGI(TAG, "Generated JSON: %s", json_string);
    send_json_to_url("https://openapi.api.govee.com/router/api/v1/device/control", json_string);
    // Release resources
    free(json_string); 
    cJSON_Delete(root);
}

// Function to send JSON data to a specified URL
esp_err_t send_json_to_url(const char *url, const char *json_data) {
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    // Set headers for JSON content
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Govee-API-Key", GOVEE_KEY);

    // Set the JSON payload
    esp_http_client_set_post_field(client, json_data, strlen(json_data));

    // Perform the HTTP request
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST Status = %d", status_code);
        if (status_code == 200 || status_code == 201) {
            ESP_LOGI(TAG, "JSON data sent successfully");
        } else {
            ESP_LOGW(TAG, "Unexpected HTTP status code: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    // Clean up
    esp_http_client_cleanup(client);
    return err;
}
