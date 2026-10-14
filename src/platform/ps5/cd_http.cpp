// CD Player - Bounded, certificate-verified metadata HTTPS on a worker thread.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cd/disc.hpp"
#include "platform/ps5/system.hpp"
#include "platform/ps5/console_curl.h"
#include <mutex>

extern "C" int sceNetInit(void);
namespace cd
{
namespace
{
struct Body
{
    std::vector<uint8_t> &bytes;
    size_t limit;
};
size_t receive(char *data, size_t size, size_t count, void *opaque)
{
    auto &body = *static_cast<Body *>(opaque);
    if (count > SIZE_MAX / std::max(size, size_t(1)))
        return 0;
    const size_t bytes = size * count;
    if (bytes > body.limit - body.bytes.size())
        return 0;
    body.bytes.insert(body.bytes.end(), data, data + bytes);
    return bytes;
}
} // namespace
int http_get(const std::string &url, std::vector<uint8_t> &out, size_t limit)
{
    static std::mutex mutex;
    static bool initialized = false;
    std::lock_guard lock(mutex);
    out.clear();
    if (url.rfind("https://musicbrainz.org/", 0) != 0 &&
        url.rfind("https://coverartarchive.org/", 0) != 0 &&
        url.rfind("https://www.wikidata.org/", 0) != 0 &&
        url.rfind("https://en.wikipedia.org/", 0) != 0)
        return -1;
    if (!initialized)
    {
        const int net = sceNetInit();
        const auto result = curl_global_init(CURL_GLOBAL_DEFAULT);
        hui::sys::log("[CD HTTPS] net=%08x curl_init=%d", unsigned(net), int(result));
        if (result != CURLE_OK)
            return -1;
        initialized = true;
    }
    CURL *request = curl_easy_init();
    if (!request)
        return -1;
    console_curl_setup(request);
    Body body{out, limit};
    char error[CURL_ERROR_SIZE]{};
    curl_easy_setopt(request, CURLOPT_URL, url.c_str());
    curl_easy_setopt(request, CURLOPT_USERAGENT, "CDPlayer/0.1 (private PS5 homebrew)");
    curl_easy_setopt(request, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
    curl_easy_setopt(request, CURLOPT_TIMEOUT_MS, 15000L);
    curl_easy_setopt(request, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(request, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(request, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(request, CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(request, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(request, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(request, CURLOPT_WRITEFUNCTION, receive);
    curl_easy_setopt(request, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(request, CURLOPT_ERRORBUFFER, error);
    const auto result = curl_easy_perform(request);
    long status = 0;
    curl_easy_getinfo(request, CURLINFO_RESPONSE_CODE, &status);
    hui::sys::log("[CD HTTPS] %s result=%d status=%ld bytes=%zu CA=%s detail=%s",
                  url.substr(0, url.find('?')).c_str(), int(result), status, out.size(),
                  console_curl_ca_file(), error);
    curl_easy_cleanup(request);
    if (result != CURLE_OK)
    {
        out.clear();
        return -1;
    }
    return int(status);
}
} // namespace cd
