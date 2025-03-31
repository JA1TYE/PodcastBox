#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

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
#include "podcastlist.h"
#include "podcastutils.h"

#include "message_queue.h"

#include <string>

#define PODCAST_ERROR -1
#define PODCAST_SUCCESS 0
#define PODCAST_INTERRUPTED 1

#define DEFAULT_VOLUME -15

static const char *TAG = "PODCAST";

int playPodCast(std::string feed_url, int volume, QueueHandle_t *mailboxes) {
    std::string episode_url;
    episode_url = get_latest_episode(feed_url.c_str());
    if (episode_url == "") {
        ESP_LOGE(TAG, "No episode URL found");
        return PODCAST_ERROR;
    }

    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    audio_pipeline_handle_t pipeline;
    audio_element_handle_t http_stream_reader, i2s_stream_writer, mp3_decoder;

    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_stream_reader = http_stream_init(&http_cfg);

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.use_alc = true;
    i2s_cfg.volume = DEFAULT_VOLUME + volume;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_decoder = mp3_decoder_init(&mp3_cfg);

    audio_pipeline_register(pipeline, http_stream_reader, "http");
    audio_pipeline_register(pipeline, mp3_decoder,        "mp3");
    audio_pipeline_register(pipeline, i2s_stream_writer,  "i2s");

    const char *link_tag[3] = {"http", "mp3", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);

    if(episode_url == ""){
        ESP_LOGE(TAG, "No episode URL found");
        return PODCAST_ERROR;
    }
    audio_element_set_uri(http_stream_reader, episode_url.c_str());
    audio_pipeline_set_listener(pipeline, evt);
    audio_pipeline_run(pipeline);

    bool interrupt_flag = false;

    while (1) {
        audio_event_iface_msg_t msg;
        //Listen audio event (non-blocking)
        esp_err_t ret = audio_event_iface_listen(evt, &msg, 0);
        if (ret == ESP_OK) {
            if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
                && msg.source == (void *) mp3_decoder
                && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
                audio_element_info_t music_info = {0};
                audio_element_getinfo(mp3_decoder, &music_info);
    
                ESP_LOGI(TAG, "[ * ] Receive music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
                         music_info.sample_rates, music_info.bits, music_info.channels);
    
                i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
                continue;
            }
    
            // Stop when the last pipeline element (i2s_stream_writer in this case) receives stop event
            if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) i2s_stream_writer
                && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
                && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
                ESP_LOGW(TAG, "[ * ] Stop event received");
                break;
            }
        }
        // Listen button event (non-blocking)
        uint8_t queue_msg;
        if (xQueueReceive(mailboxes[PODCAST_QUEUE], &queue_msg, 0) == pdPASS) {
            interrupt_flag = true;
            break;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_unregister(pipeline, http_stream_reader);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);
    audio_pipeline_unregister(pipeline, mp3_decoder);

    audio_pipeline_remove_listener(pipeline);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(http_stream_reader);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(mp3_decoder);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    return interrupt_flag ? PODCAST_INTERRUPTED : PODCAST_SUCCESS;
}


void podcastTask(void *pvParameters) {
    
    QueueHandle_t *mailboxes = (QueueHandle_t *) pvParameters;

    uint8_t msg;

    //Initialize audio codec
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    while(1){
        if (xQueueReceive(mailboxes[PODCAST_QUEUE], &msg, 0) == pdPASS) {
            init_podcast_list();
            restart_podcast_list();
            std::string feed_url;
            int volume = 0;
            int ret = 0;
            while(1){
                if (get_next_podcast(&feed_url, &volume)) {
                    ESP_LOGI(TAG, "Playing podcast from URL: %s with volume %d", feed_url.c_str(), volume);
                    ret = playPodCast(feed_url,volume, mailboxes);
                    if(ret == PODCAST_INTERRUPTED) {
                        ESP_LOGI(TAG, "Podcast interrupted. Stopping playback.");
                        break;
                    } else if (ret == PODCAST_ERROR) {
                        ESP_LOGE(TAG, "Error playing podcast. Skipping to next.");
                        continue;
                    }
                } else {
                    ESP_LOGI(TAG, "No more podcasts to play.");
                    break;
                }
            }
        }
        vTaskDelay(10/portTICK_PERIOD_MS); 
    }
    vTaskDelete(NULL);
}