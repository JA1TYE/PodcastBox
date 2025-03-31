#ifndef __PODCASTLIST_H__
#define __PODCASTLIST_H__

#include <string>
#include <vector>

typedef struct {
    std::string url;
    int  volume;
} podcast_t;

void init_podcast_list();
void register_podcast(std::string url, int volume);
bool get_next_podcast(std::string *url, int *volume);
void restart_podcast_list();
#endif