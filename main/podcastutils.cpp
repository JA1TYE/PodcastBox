#include "podcastutils.h"
#include "podcastlist.h"

#include "esp_log.h"
#include "esp_err.h"
#include <cstdlib>

#include <string>
#include <stdio.h>
#include <expat.h>
#include "esp_http_client.h"

#define TAG "PCASTUTIL"

/*RSS Parser for getting the latest episode in the feed*/
static std::string _latestTitle;
static std::string _latestUrl;

enum RssParserState {
    RSS_INIT,
    RSS_TOP,           // At the top level of RSS feed
    RSS_IN_CHANNEL,    // Inside a 'channel' tag
    RSS_IN_ITEM,       // Inside an 'item' tag within 'channel'
    RSS_IN_TITLE,      // Inside a 'title' tag within 'item'
    RSS_GOT_TITLE,   // Got the title
    RSS_GOT_URL,     // Got the URL
    RSS_IN_TITLE_AFTER_URL, // Inside a 'title' tag after getting the URL
    RSS_DONE,          // Finished parsing
};

typedef struct {
	int		depth; // XML depth
	char	tag[64]; // XML tag
} user_data_t;

static RssParserState _parserState = RSS_INIT;

//Parse the RSS feed and return the latest episode URL
std::string get_latest_episode(std::string url)
{
    fetch_and_print_url_content(url);
    ESP_LOGI(TAG, "Parser State: %d", _parserState);
    if(_parserState == RSS_DONE || _parserState == RSS_GOT_URL){
        return _latestUrl;
    }
    return "";
}

std::string get_latest_title()
{
    if(_parserState == RSS_GOT_TITLE || _parserState == RSS_DONE){
        return _latestTitle;
    }
    return "";
}

static XML_Parser parser;
user_data_t userData;

static void XMLCALL start_element(void *userData, const XML_Char *name, const XML_Char **atts)
{
	ESP_LOGD(TAG, "start_element name=%s", name);
    user_data_t *user_data = (user_data_t *)userData;
    strcpy(user_data->tag,name);
    switch(_parserState){
        case RSS_INIT:
            if(std::string(name) == "rss"){
                _parserState = RSS_TOP;
            }
            break;
        case RSS_TOP:
            if(std::string(name) == "channel"){
                _parserState = RSS_IN_CHANNEL;
            }
            break;
        case RSS_IN_CHANNEL:
            if(std::string(name) == "item"){
                _parserState = RSS_IN_ITEM;
            }
            break;
        case RSS_IN_ITEM:
            if(std::string(name) == "title"){
                _parserState = RSS_IN_TITLE;
            }
            else
            if(std::string(name) == "enclosure"){
                for(int i = 0; atts[i]; i += 2) {
                    if(std::string(atts[i]) == "url"){
                        _latestUrl = std::string(atts[i + 1]);
                        _parserState = RSS_GOT_URL;
                    }
                }
            }
            break;
        case RSS_GOT_TITLE:
            if(std::string(name) == "enclosure"){
                for(int i = 0; atts[i]; i += 2) {
                    if(std::string(atts[i]) == "url"){
                        _latestUrl = std::string(atts[i + 1]);
                        _parserState = RSS_DONE;
                    }
                }
            }
            break;
        case RSS_GOT_URL:
            if(std::string(name) == "title"){
                _parserState = RSS_IN_TITLE_AFTER_URL;
            }
            break;
        default:
            break;
    }
}

static void XMLCALL end_element(void *userData, const XML_Char *name)
{
	ESP_LOGD(TAG, "end_element name[%d]=%s", strlen(name), name);
    if(_parserState == RSS_IN_TITLE){
        _parserState = RSS_GOT_TITLE;
    }
    else if(_parserState == RSS_IN_TITLE_AFTER_URL){
        _parserState = RSS_DONE;
    }
}

static void data_handler(void *userData, const XML_Char *s, int len)
{
    if(_parserState == RSS_IN_TITLE || _parserState == RSS_IN_TITLE_AFTER_URL){
        if (len == 1 && s[0] == 0x0a){
            _latestTitle = "";
        }
        else{
            _latestTitle = std::string(s, len);
        }
    }
}

static void RSS_parser_init(void){
    _parserState = RSS_INIT;
    _latestTitle = "";
    _latestUrl = "";

    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    userData.depth = 0;
    memset(userData.tag, 0, sizeof(userData.tag));

    // Prepare XML parser
    parser = XML_ParserCreate(NULL);
    XML_SetUserData(parser, &userData);
    XML_SetElementHandler(parser, start_element, end_element);
    XML_SetCharacterDataHandler(parser, data_handler);

}

static bool is_RSS_done(){
    return _parserState == RSS_DONE;
}

static XML_Status RSS_parse(const char *data, int len, bool isFinal){
    return XML_Parse(parser, data, len, isFinal);
}                                   
static void RSS_parser_cleanup(void){
    XML_ParserFree(parser);
}

void fetch_and_print_url_content(std::string url)
{
    esp_log_level_set("PCASTUTIL", ESP_LOG_VERBOSE);
    
    //Initialize RSS parser
    RSS_parser_init();
    ESP_LOGI(TAG, "Fetching URL: %s", url.c_str());

    // Secure the buffer for retrieving the data
    char *buffer = (char*)malloc(MAX_HTTP_RECV_BUFFER + 1);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Cannot malloc http receive buffer");
        return;
    }
    esp_http_client_config_t config = {
        .url = url.c_str(),
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err;
    if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        free(buffer);
        return;
    }
    // Check the server uses chunked encoding
    int content_length =  esp_http_client_fetch_headers(client);
    ESP_LOGD(TAG, "Content Length: %d", content_length);
    bool is_chunked = esp_http_client_is_chunked_response(client);
    if(!is_chunked){
        ESP_LOGI(TAG, "Normal encoding");
        int total_read_len = 0, read_len,req_len;
        while(total_read_len < content_length){
            req_len = (content_length - total_read_len) > MAX_HTTP_RECV_BUFFER ? MAX_HTTP_RECV_BUFFER : (content_length - total_read_len);
            ESP_LOGD(TAG, "total_read_len = %d, req_len = %d", total_read_len, req_len);
            read_len = esp_http_client_read(client, buffer, req_len);
            if (read_len <= 0) {
                ESP_LOGE(TAG, "Error read data");
                break;
            }
            buffer[read_len] = 0;
            bool is_final = (content_length == total_read_len + read_len);
            if (RSS_parse(buffer, read_len, is_final) != XML_STATUS_OK) {
                ESP_LOGE(TAG, "XML_Parse fail");
            }
            if(is_RSS_done()){
                break;
            }
            total_read_len += read_len;
        }
    }
    else{
        ESP_LOGI(TAG, "Chunked encoding");
        int chunk_len;
        esp_http_client_get_chunk_length(client,&chunk_len);
        int total_read_len = 0, chunk_read_len = 0, read_len, req_len;
        while (chunk_len > 0) {
            while(chunk_read_len < chunk_len){
                req_len = chunk_len > MAX_HTTP_RECV_BUFFER ? MAX_HTTP_RECV_BUFFER : chunk_len;
                read_len = esp_http_client_read(client, buffer, req_len);
                buffer[read_len] = 0;
                if (RSS_parse(buffer, read_len, false) != XML_STATUS_OK) {
                    ESP_LOGE(TAG, "XML_Parse fail");
                }
                chunk_read_len += read_len;
                if(is_RSS_done()){
                    break;
                }
            }
            if(is_RSS_done()){
                break;
            }
            esp_http_client_get_chunk_length(client,&chunk_len);
            total_read_len += chunk_read_len;
            chunk_read_len = 0;
        }
    }


    ESP_LOGI(TAG, "HTTP Stream reader Status = %d, content_length = %"PRId64,
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
                    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    RSS_parser_cleanup();
    free(buffer);
    ESP_LOGI(TAG, "RSS Parser done.");
    ESP_LOGI(TAG, "Latest Episode Title: %s", _latestTitle.c_str());
    ESP_LOGI(TAG, "Latest Episode URL: %s", _latestUrl.c_str());
}