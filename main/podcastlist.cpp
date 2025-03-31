#include "podcastlist.h"

#include <string>
#include <vector>

static std::vector<podcast_t> pd_list;
static int current_podcast_index = 0;

void init_podcast_list()
{
    pd_list.clear();
    register_podcast("https://www.nhk.or.jp/s-media/news/podcast/list/v1/all.xml", 0);
    register_podcast("ADD_YOUR_PODCAST_FEED_URL", -3);
}

void register_podcast(std::string url, int volume)
{
    podcast_t pd = {url, volume};
    pd_list.push_back(pd);
}

bool get_next_podcast(std::string *url, int *volume)
{
    printf("Current podcast index: %d/%d\n", current_podcast_index, pd_list.size());
    if (current_podcast_index >= pd_list.size()) {
        return false;
    }
    *url = pd_list[current_podcast_index].url;
    *volume = pd_list[current_podcast_index].volume;
    current_podcast_index++;
    return true;
}

void restart_podcast_list()
{
    current_podcast_index = 0;
}
