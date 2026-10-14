// CD Player - AudioOut PCM sink, using EVO Player's verified 7.1 channel order.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/ps5/cd_output.hpp"
#include "platform/ps5/system.hpp"
#include <algorithm>
extern "C"
{
    int sceAudioOutInit(void);
    int sceAudioOutOpen(int, int, int, unsigned, unsigned, unsigned);
    int sceAudioOutOutput(int, const void *);
    int sceAudioOutClose(int);
    int sceAudioOutSetVolume(int, int, const int *);
}
namespace cd
{
Output::~Output()
{
    stop();
}
bool Output::start()
{
    const int init = sceAudioOutInit();
    if (init != 0 && init != int(0x8026000e))
        return false;
    handle_ = sceAudioOutOpen(0xff, 0, 0, 512, 48000, 2);
    if (handle_ < 0)
    {
        channels_ = 2;
        handle_ = sceAudioOutOpen(0xff, 0, 0, 512, 48000, 1);
    }
    if (handle_ < 0)
        return false;
    const int volumes[8] = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};
    sceAudioOutSetVolume(handle_, channels_ == 8 ? 255 : 3, volumes);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 512 * 1024);
    started_ = pthread_create(&thread_, &attr, entry, this) == 0;
    pthread_attr_destroy(&attr);
    if (!started_)
    {
        sceAudioOutClose(handle_);
        handle_ = -1;
    }
    hui::sys::log("[CD] AudioOut: handle %d, %u channels at 48000 Hz", handle_, channels_);
    return started_;
}
void Output::stop()
{
    stop_.store(true);
    if (started_)
    {
        pthread_join(thread_, nullptr);
        started_ = false;
    }
    if (handle_ >= 0)
        sceAudioOutClose(handle_);
    handle_ = -1;
}
void *Output::entry(void *self)
{
    static_cast<Output *>(self)->loop();
    return nullptr;
}
void Output::loop()
{
    alignas(64) std::array<int16_t, 1024> pcm{}, ui{};
    alignas(64) std::array<int16_t, 4096> multichannel{};
    while (!stop_.load())
    {
        player_.render(pcm.data(), 512);
        cues_.render(ui.data(), 512);
        for (size_t i = 0; i < pcm.size(); ++i)
            pcm[i] = int16_t(std::clamp(int(pcm[i]) + int(ui[i]), -32768, 32767));
        dsp_.process(pcm, std::span(multichannel).first(512 * channels_), channels_);
        if (sceAudioOutOutput(handle_, multichannel.data()) < 0)
            hui::sys::sleep_us(10000);
    }
    sceAudioOutOutput(handle_, nullptr);
}
} // namespace cd
