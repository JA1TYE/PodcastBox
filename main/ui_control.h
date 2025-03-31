#ifndef UI_CONTROL_H
#define UI_CONTROL_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif // UI_CONTROL_H
