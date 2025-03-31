#ifndef _PODCASTUTILS_H_
#define _PODCASTUTILS_H_

#include <string>

#define MAX_HTTP_RECV_BUFFER 512

std::string get_latest_episode(std::string url);
void fetch_and_print_url_content(std::string url);

#endif