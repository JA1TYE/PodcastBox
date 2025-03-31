#ifndef _HOME_CONTROL_H_
#define _HOME_CONTROL_H_

#include <string>

// Function declarations
void home_task(void *pvParameters);
void make_request(std::string device_address, bool switch_on);
esp_err_t send_json_to_url(const char *url, const char *json_data);

#endif // HOME_CONTROL_H