/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <string.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "board.h"

#include "nau8315.h"

static const char *TAG = "nau8315";

static bool codec_init_flag;

audio_hal_func_t AUDIO_NAU8315_DEFAULT_HANDLE = {
    .audio_codec_initialize = nau8315_init,
    .audio_codec_deinitialize = nau8315_deinit,
    .audio_codec_ctrl = nau8315_ctrl_state,
    .audio_codec_config_iface = nau8315_config_i2s,
    .audio_codec_set_mute = nau8315_set_voice_mute,
    .audio_codec_set_volume = nau8315_set_voice_volume,
    .audio_codec_get_volume = nau8315_get_voice_volume,
    .audio_codec_enable_pa = nau8315_enable_pa,
};

bool nau8315_initialized()
{
    return codec_init_flag;
}

esp_err_t nau8315_init(audio_hal_codec_config_t *cfg)
{
    ESP_LOGI(TAG, "nau8315 init");
    /* NAU8315 PA gpio_config */
    gpio_config_t  io_conf;
    memset(&io_conf, 0, sizeof(io_conf));
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT64(get_pa_enable_gpio());
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    /* enable NAU8315 PA */
    nau8315_enable_pa(true);
    codec_init_flag = true;
    return ESP_OK;
}

esp_err_t nau8315_deinit(void)
{
    nau8315_enable_pa(false);
    codec_init_flag = false;
    return ESP_OK;
}

esp_err_t nau8315_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state)
{
    return ESP_OK;
}

esp_err_t nau8315_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface)
{
    return ESP_OK;
}

esp_err_t nau8315_set_voice_mute(bool mute)
{
    return ESP_OK;
}

esp_err_t nau8315_set_voice_volume(int volume)
{
    return ESP_OK;
}

esp_err_t nau8315_get_voice_volume(int *volume)
{
    return ESP_OK;
}

esp_err_t nau8315_enable_pa(bool enable)
{
    esp_err_t res = ESP_OK;
    if (enable) {
        res = gpio_set_level(get_pa_enable_gpio(), 1);

    } else {
        res = gpio_set_level(get_pa_enable_gpio(), 0);
    }
    return res;
}

