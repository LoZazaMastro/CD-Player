// CD Player - Worker-owned optical I/O, gapless PCM stream and metadata cache.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cd/player.hpp"
#include "core/save_file.hpp"
#include "platform/ps5/system.hpp"
#include "third_party/stb/stb_image.h"
#define JSON_NOEXCEPTION 1
#include "third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sys/stat.h>

namespace cd
{
namespace
{
bool worker(pthread_t &thread, void *(*entry)(void *), void *self)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 2u << 20);
    const int result = pthread_create(&thread, &attr, entry, self);
    pthread_attr_destroy(&attr);
    return result == 0;
}
void wait(unsigned us = 10000)
{
    hui::sys::sleep_us(us);
}
bool get_cache(const std::string &path, std::vector<uint8_t> &out, size_t limit)
{
    std::string bytes;
    if (!hui::save::read_file(path, &bytes) || bytes.size() > limit)
        return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
}
void write_cache(const std::string &path, std::span<const uint8_t> bytes)
{
    const std::string temp = path + ".tmp";
    FILE *f = std::fopen(temp.c_str(), "wb");
    if (!f)
        return;
    const bool complete = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    const bool closed = std::fclose(f) == 0;
    if (complete && closed)
        std::rename(temp.c_str(), path.c_str());
    else
        std::remove(temp.c_str());
}
} // namespace
Player::~Player()
{
    stop();
}
bool Player::start(std::string cache)
{
    if (started_)
        return true;
    cache_ = std::move(cache);
    mkdir(cache_.c_str(), 0700);
    stop_.store(false);
    rotation_.seed(uint64_t(hui::sys::monotonic_us()));
    std::string preferences;
    if (dsp_ && hui::save::read_file(cache_ + "/settings.json", &preferences) &&
        preferences.size() < 16384)
    {
        hui::sys::log("[CD startup] restoring preferences (%zu bytes)", preferences.size());
        auto settings = nlohmann::json::parse(preferences, nullptr, false);
        hui::sys::log("[CD startup] preferences parsed (valid=%d)", settings.is_object());
        if (settings.is_object())
        {
            if (settings.contains("equalizer") && settings["equalizer"].is_boolean())
                dsp_->equalizer(settings["equalizer"].get<bool>());
            if (settings.contains("upmix") && settings["upmix"].is_boolean())
                dsp_->upmix(settings["upmix"].get<bool>());
            if (settings.contains("gapless") && settings["gapless"].is_boolean())
                gapless(settings["gapless"].get<bool>());
            if (settings.contains("volume") && settings["volume"].is_number())
                volume(std::clamp(settings["volume"].get<float>(), 0.f, 1.f));
            if (settings.contains("reduced_motion") && settings["reduced_motion"].is_boolean())
                reduced_motion(settings["reduced_motion"].get<bool>());
            if (settings.contains("visualization") &&
                settings["visualization"].is_number_unsigned())
                visualization(settings["visualization"].get<unsigned>());
            if (settings.contains("visual_rotation") && settings["visual_rotation"].is_boolean())
                visual_rotation(settings["visual_rotation"].get<bool>());
            if (settings.contains("show_eject_button") && settings["show_eject_button"].is_boolean())
                show_eject_button(settings["show_eject_button"].get<bool>());
            if (settings.contains("autoplay") && settings["autoplay"].is_boolean())
                autoplay(settings["autoplay"].get<bool>());
            if (settings.contains("oled_visualizer") && settings["oled_visualizer"].is_boolean())
                oled_visualizer(settings["oled_visualizer"].get<bool>());
            if (settings.contains("bands") && settings["bands"].is_array())
                for (size_t i = 0; i < 10 && i < settings["bands"].size(); ++i)
                    if (settings["bands"][i].is_number())
                    {
                        float v = settings["bands"][i].get<float>();
                        if (std::isfinite(v))
                            dsp_->band(i, v);
                    }
        }
    }
    started_ = worker(io_thread_, io_entry, this);
    if (started_)
        metadata_started_ = worker(metadata_thread_, metadata_entry, this);
    return started_;
}
void Player::stop()
{
    stop_.store(true);
    playing_.store(false);
    if (started_)
    {
        pthread_join(io_thread_, nullptr);
        started_ = false;
    }
    if (metadata_started_)
    {
        pthread_join(metadata_thread_, nullptr);
        metadata_started_ = false;
    }
}
void *Player::io_entry(void *p)
{
    static_cast<Player *>(p)->io_loop();
    return nullptr;
}
void *Player::metadata_entry(void *p)
{
    static_cast<Player *>(p)->metadata_loop();
    return nullptr;
}
void Player::set_state(State state, std::string message)
{
    std::lock_guard lock(mutex_);
    snapshot_.state = state;
    snapshot_.message = std::move(message);
    ++snapshot_.revision;
}
Snapshot Player::snapshot()
{
    std::lock_guard lock(mutex_);
    Snapshot s = snapshot_;
    s.playing = playing_.load() && finished_epoch_.load() != epoch_.load();
    s.underruns = underruns_.load();
    s.gapless = gapless();
    s.reduced_motion = reduced_motion_.load();
    s.visualization = visualization_.load();
    s.visual_rotation = visual_rotation_.load();
    s.autoplay = autoplay_.load();
    s.show_eject_button = show_eject_button_.load();
    s.oled_visualizer = oled_visualizer_.load();
    s.volume = volume();
    const uint64_t position = position_.load();
    s.track = 0;
    for (const auto &t : s.disc.tracks)
    {
        if (position >= uint64_t(t.start) * 640 && position < uint64_t(t.end) * 640)
        {
            s.track = t.number;
            s.seconds = double(position - uint64_t(t.start) * 640) / 48000.;
            break;
        }
    }
    if (s.state == State::ready)
    {
        const unsigned next = rotation_.observe(s.track, s.visualization, s.visual_rotation);
        if (next != s.visualization)
        {
            visualization_.store(next);
            hui::sys::log("[CD visualizer] track=%u shuffle %u -> %u", s.track, s.visualization,
                          next);
            s.visualization = next;
        }
    }
    return s;
}
void Player::play_track(size_t index)
{
    std::lock_guard lock(mutex_);
    if (snapshot_.state != State::ready || index >= snapshot_.disc.tracks.size() ||
        !snapshot_.disc.tracks[index].audio())
        return;
    seek_request_ = snapshot_.disc.tracks[index].start;
    seek_epoch_ = epoch_.fetch_add(1) + 1;
    position_.store(seek_request_ * 640);
    end_epoch_.store(0);
    finished_epoch_.store(0);
    playback_started_.store(true);
    playing_.store(true);
}
void Player::toggle_pause()
{
    const Snapshot s = snapshot();
    if (s.state != State::ready)
        return;
    if (!playback_started_.load() || s.track == 0 || finished_epoch_.load() == epoch_.load())
    {
        const auto first = std::find_if(s.disc.tracks.begin(), s.disc.tracks.end(),
                                        [](const Track &t) { return t.audio(); });
        if (first == s.disc.tracks.end())
            return;
        play_track(size_t(first - s.disc.tracks.begin()));
        return;
    }
    playing_.store(!playing_.load());
}
void Player::seek(double delta)
{
    std::lock_guard lock(mutex_);
    if (snapshot_.state != State::ready || snapshot_.disc.tracks.empty())
        return;
    const int64_t current = int64_t(position_.load() / 640);
    const int64_t next =
        std::clamp(current + int64_t(delta * 75), int64_t(snapshot_.disc.tracks.front().start),
                   int64_t(snapshot_.disc.leadout) - 1);
    auto it = std::find_if(snapshot_.disc.tracks.begin(), snapshot_.disc.tracks.end(),
                           [next](const auto &t)
                           { return t.audio() && next >= t.start && next < t.end; });
    if (it == snapshot_.disc.tracks.end())
        return;
    seek_request_ = uint64_t(next);
    seek_epoch_ = epoch_.fetch_add(1) + 1;
    position_.store(seek_request_ * 640);
    end_epoch_.store(0);
    finished_epoch_.store(0);
}
void Player::next(int direction)
{
    const auto s = snapshot();
    auto it = std::find_if(s.disc.tracks.begin(), s.disc.tracks.end(),
                           [&s](const auto &t) { return t.number == s.track; });
    int index = it == s.disc.tracks.end() ? 0 : int(it - s.disc.tracks.begin()) + direction;
    while (index >= 0 && index < int(s.disc.tracks.size()) && !s.disc.tracks[size_t(index)].audio())
        index += direction;
    if (index >= 0)
        play_track(size_t(index));
}
void Player::eject_disc()
{
    playing_.store(false);
    epoch_.fetch_add(1);
    std::lock_guard lock(mutex_);
    eject_request_ = true;
}
void Player::choose_edition(size_t index)
{
    std::lock_guard lock(mutex_);
    if (index >= snapshot_.editions.size())
        return;
    edition_request_ = index;
    metadata_id_ = snapshot_.disc.id;
    ++metadata_generation_;
}
void Player::render(int16_t *samples, unsigned frames)
{
    const uint64_t epoch = epoch_.load();
    if (have_held_frame_ && held_frame_.epoch != epoch)
    {
        have_held_frame_ = false;
        gap_frames_ = 0;
    }
    const bool play = playing_.load() && finished_epoch_.load() != epoch;
    const float gain = volume_.load();
    bool shortage = false;
    for (unsigned i = 0; i < frames; ++i)
    {
        AudioFrame frame;
        if (gapless_.load())
            gap_frames_ = 0;
        if (play && gap_frames_)
        {
            samples[i * 2] = samples[i * 2 + 1] = 0;
            analyzer_.feed(0, 0);
            --gap_frames_;
            continue;
        }
        const bool present = have_held_frame_ ? play : ring_.read(frame, epoch, play);
        if (have_held_frame_ && play)
        {
            frame = held_frame_;
            have_held_frame_ = false;
        }
        if (!present)
        {
            samples[i * 2] = samples[i * 2 + 1] = 0;
            analyzer_.feed(0, 0);
            shortage |= play && end_epoch_.load() != epoch;
            continue;
        }
        if (!gapless_.load() && frame.position != last_gap_ && previous_audio_epoch_ == epoch &&
            previous_audio_position_ + 1 == frame.position)
        {
            bool boundary = false;
            for (size_t n = 1; n < 100; ++n)
                if (track_starts_[n].load(std::memory_order_relaxed) == frame.position &&
                    frame.position != 0)
                    boundary = true;
            if (boundary)
            {
                last_gap_ = frame.position;
                held_frame_ = frame;
                have_held_frame_ = true;
                gap_frames_ = 47999;
                samples[i * 2] = samples[i * 2 + 1] = 0;
                analyzer_.feed(0, 0);
                continue;
            }
        }
        previous_audio_position_ = frame.position;
        previous_audio_epoch_ = epoch;
        position_.store(frame.position + 1, std::memory_order_relaxed);
        const float l = std::clamp(frame.left * gain, -1.f, 1.f),
                    r = std::clamp(frame.right * gain, -1.f, 1.f);
        samples[i * 2] = int16_t(std::clamp(std::lrint(l * 32768.f), -32768l, 32767l));
        samples[i * 2 + 1] = int16_t(std::clamp(std::lrint(r * 32768.f), -32768l, 32767l));
        analyzer_.feed(l, r);
    }
    if (shortage)
        underruns_.fetch_add(1, std::memory_order_relaxed);
    if (play && end_epoch_.load() == epoch && ring_.available() == 0 && !have_held_frame_ &&
        gap_frames_ == 0 && epoch == epoch_.load())
    {
        finished_epoch_.store(epoch);
    }
}
void Player::io_loop()
{
    std::unique_ptr<Drive> drive;
    std::vector<uint8_t> raw(20 * sector_bytes), toc_bytes(32768);
    Resampler resampler;
    uint64_t active_epoch = 0, output_position = 0;
    uint32_t read_at = 0, audio_end = 0;
    int64_t poll_at = 0;
    std::string disc_seen;
    unsigned piece_limit = 20;
    bool initialized = false;
    int64_t preferences_at = 0;
    std::string saved_preferences;
    auto forget = [&]
    {
        playback_started_.store(false);
        playing_.store(false);
        epoch_.fetch_add(1);
        active_epoch = 0;
        disc_seen.clear();
        std::lock_guard lock(mutex_);
        snapshot_.disc = {};
        rotation_.reset_track();
        snapshot_.cover.reset();
        snapshot_.editions.clear();
        snapshot_.album_info = {};
        snapshot_.album_info_ready = snapshot_.album_info_error = false;
        ++snapshot_.revision;
        ++snapshot_.cover_revision;
        ++metadata_generation_;
        metadata_id_.clear();
        seek_epoch_ = 0;
        for (auto &start : track_starts_)
            start.store(UINT64_MAX);
    };
    while (!stop_.load())
    {
        if (dsp_ && hui::sys::monotonic_us() >= preferences_at)
        {
            preferences_at = hui::sys::monotonic_us() + 1000000;
            nlohmann::json settings = {{"version", 1},
                                       {"equalizer", dsp_->equalizer()},
                                       {"upmix", dsp_->upmix()},
                                       {"gapless", gapless()},
                                       {"volume", volume()},
                                       {"reduced_motion", reduced_motion_.load()},
                                       {"visualization", visualization_.load()},
                                       {"visual_rotation", visual_rotation_.load()},
                                       {"autoplay", autoplay_.load()},
                                       {"oled_visualizer", oled_visualizer_.load()},
                                       {"show_eject_button", show_eject_button_.load()}};
            std::vector<float> bands;
            for (size_t i = 0; i < 10; ++i)
                bands.push_back(dsp_->band(i));
            settings["bands"] = bands;
            const std::string bytes = settings.dump(2) + "\n";
            if (bytes != saved_preferences)
            {
                write_cache(cache_ + "/settings.json",
                            {reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size()});
                saved_preferences = bytes;
            }
        }
        if (!drive)
        {
            std::string error;
            for (const auto &name : drive_names())
            {
                drive = open_drive(name, error);
                if (drive)
                {
                    std::lock_guard lock(mutex_);
                    snapshot_.drive = name;
                    break;
                }
            }
            if (!drive)
            {
                set_state(State::searching, error.empty() ? "Connect a USB CD drive" : error);
                wait(500000);
                continue;
            }
            initialized = false;
            poll_at = 0;
        }
        if (!initialized)
        {
            const auto response = drive->execute(inquiry(), toc_bytes);
            if (!response.ok || response.transferred < 36)
            {
                set_state(State::error, response.error);
                drive.reset();
                wait(500000);
                continue;
            }
            const std::string identity(reinterpret_cast<char *>(toc_bytes.data() + 8), 28);
            if (identity.find("Apple") != std::string::npos ||
                identity.find("APPLE") != std::string::npos)
            {
                const auto wake = drive->execute(apple_wake(), {});
                hui::sys::log("[CD] Apple wake: %s", wake.ok ? "accepted" : wake.error.c_str());
                if (!wake.ok)
                {
                    set_state(State::error, wake.error);
                    wait(500000);
                    continue;
                }
            }
            drive->execute(unlock(), {});
            {
                std::lock_guard lock(mutex_);
                snapshot_.drive = identity;
            }
            initialized = true;
        }
        bool eject_now = false;
        uint64_t target = 0, request_epoch = 0;
        {
            std::lock_guard lock(mutex_);
            eject_now = eject_request_;
            eject_request_ = false;
            target = seek_request_;
            request_epoch = seek_epoch_;
        }
        if (eject_now)
        {
            set_state(State::ejecting, "Ejecting disc");
            drive->execute(unlock(), {});
            const auto result = drive->execute(eject(), {});
            hui::sys::log("[CD] eject: %s", result.ok ? "accepted" : result.error.c_str());
            if (!result.ok)
                set_state(State::error, "Unable to eject: " + result.error);
            else
            {
                // Keep the disc visible until the drive reports that the medium has left.
                for (unsigned n = 0; n < 60 && !stop_.load(); ++n)
                {
                    wait(100000);
                    const auto status = drive->execute(ready(), {});
                    if (status.no_medium())
                    {
                        forget();
                        set_state(State::empty, "Insert an audio CD");
                        break;
                    }
                }
            }
            poll_at = 0;
            continue;
        }
        const int64_t now = hui::sys::monotonic_us();
        if (now >= poll_at)
        {
            poll_at = now + 1000000;
            const auto status = drive->execute(ready(), {});
            if (!status.ok)
            {
                if (status.no_medium())
                {
                    if (!disc_seen.empty())
                        forget();
                    set_state(State::empty, "Insert an audio CD");
                    wait(50000);
                    continue;
                }
                if (status.changed())
                {
                    forget();
                    poll_at = 0;
                    continue;
                }
                if (status.system_error)
                {
                    forget();
                    drive.reset();
                    set_state(State::searching, "Drive disconnected");
                    continue;
                }
                set_state(State::loading, "Preparing disc");
                wait(100000);
                continue;
            }
            if (disc_seen.empty())
            {
                set_state(State::loading, "Reading tracks");
                const auto reply = drive->execute(read_toc(), toc_bytes);
                Disc disc;
                std::string error;
                if (!reply.ok ||
                    !parse_toc(std::span(toc_bytes).first(reply.transferred), disc, error))
                {
                    set_state(State::error, reply.ok ? error : reply.error);
                    wait(500000);
                    continue;
                }
                const auto text_reply = drive->execute(read_toc(true), toc_bytes);
                if (text_reply.ok)
                    parse_cdtext(std::span(toc_bytes).first(text_reply.transferred), disc);
                size_t first_audio_index = 0;
                while (first_audio_index < disc.tracks.size() && !disc.tracks[first_audio_index].audio())
                    ++first_audio_index;
                const bool has_audio = first_audio_index < disc.tracks.size();
                disc_seen = disc.id;
                for (auto &start : track_starts_)
                    start.store(UINT64_MAX);
                for (const auto &track : disc.tracks)
                    track_starts_[track.number].store(uint64_t(track.start) * 640);
                hui::sys::log("[CD] disc %s; %zu tracks; leadout %u", disc.id.c_str(),
                              disc.tracks.size(), disc.leadout);
                {
                    std::lock_guard lock(mutex_);
                    snapshot_.disc = std::move(disc);
                    snapshot_.state = State::ready;
                    snapshot_.message = "CD Audio · 44.1 kHz / 16 bit";
                    ++snapshot_.revision;
                    metadata_id_ = disc_seen;
                    ++metadata_generation_;
                    edition_request_ = 0;
                }
                if (has_audio && autoplay_.load())
                {
                    play_track(first_audio_index);
                    hui::sys::log("[CD] autoplay: starting first audio track");
                }
            }
        }
        if (!disc_seen.empty() && request_epoch && request_epoch == epoch_.load() &&
            request_epoch != active_epoch)
        {
            active_epoch = request_epoch;
            read_at = uint32_t(target);
            output_position = target * 640;
            resampler.reset();
            end_epoch_.store(0);
            std::lock_guard lock(mutex_);
            audio_end = read_at;
            bool found = false;
            for (const auto &t : snapshot_.disc.tracks)
            {
                if (read_at >= t.start && read_at < t.end && t.audio())
                    found = true;
                if (found && !t.audio())
                    break;
                if (found)
                    audio_end = t.end;
            }
        }
        if (active_epoch && read_at < audio_end && ring_.space() >= 20 * 640 + 64)
        {
            const unsigned count = std::min(piece_limit, audio_end - read_at);
            Reply reply;
            for (unsigned attempt = 0; attempt < 3; ++attempt)
            {
                reply = drive->execute(read_audio(read_at, count), raw);
                if (reply.ok || stop_.load() || active_epoch != epoch_.load())
                    break;
                wait(30000);
            }
            if (!reply.ok)
            {
                if (count > 1)
                {
                    piece_limit = std::max(1u, count / 2);
                    continue;
                }
                playing_.store(false);
                epoch_.fetch_add(1);
                active_epoch = 0;
                set_state(State::error, "Unable to read disc: " + reply.error);
                continue;
            }
            read_at += count;
            auto samples =
                resampler.push(std::span(raw).first(count * sector_bytes), read_at == audio_end);
            if (active_epoch == epoch_.load())
            {
                if (!ring_.write(samples, active_epoch, output_position))
                {
                    set_state(State::error, "Audio buffer unavailable");
                    playing_.store(false);
                }
                output_position += samples.size() / 2;
                if (read_at == audio_end)
                    end_epoch_.store(active_epoch);
            }
        }
        else
            wait();
    }
    if (drive)
        drive->execute(unlock(), {});
}
void Player::metadata_loop()
{
    uint64_t completed = 0;
    int64_t next_request = 0;
    while (!stop_.load())
    {
        Disc disc;
        uint64_t generation = 0;
        size_t edition = 0;
        {
            std::lock_guard lock(mutex_);
            generation = metadata_generation_;
            edition = edition_request_;
            if (!metadata_id_.empty() && generation != completed)
                disc = snapshot_.disc;
        }
        if (disc.id.empty())
        {
            wait(100000);
            continue;
        }
        std::vector<uint8_t> json;
        const std::string json_path = cache_ + "/" + disc.id + ".json";
        if (!get_cache(json_path, json, 2u << 20))
        {
            while (hui::sys::monotonic_us() < next_request && !stop_.load())
                wait(50000);
            const std::string url = "https://musicbrainz.org/ws/2/discid/" + disc.id +
                                    "?fmt=json&inc=artist-credits+recordings";
            int status = -1;
            for (unsigned attempt = 0; attempt < 3 && !stop_.load(); ++attempt)
            {
                status = http_get(url, json, 2u << 20);
                if (status != -1 && status != 429 && status != 503)
                    break;
                wait(1500000 * (attempt + 1));
            }
            next_request = hui::sys::monotonic_us() + 1500000;
            if (status != 200)
            {
                completed = generation;
                hui::sys::log("[CD] metadata HTTP %d", status);
                continue;
            }
        }
        const auto editions =
            parse_releases({reinterpret_cast<const char *>(json.data()), json.size()}, disc);
        if (editions.empty())
        {
            completed = generation;
            continue;
        }
        write_cache(json_path, json);
        const Release &selected = editions[std::min(edition, editions.size() - 1)];
        if (!apply_release(selected, disc))
        {
            completed = generation;
            continue;
        }
        {
            std::lock_guard lock(mutex_);
            if (metadata_generation_ != generation || snapshot_.disc.id != disc.id)
                continue;
            snapshot_.disc = disc;
            snapshot_.editions = editions;
            snapshot_.album_info = {};
            snapshot_.album_info_ready = snapshot_.album_info_error = false;
            ++snapshot_.revision;
        }
        std::vector<uint8_t> image;
        const std::string cover_path = cache_ + "/" + selected.id + ".cover";
        if (get_cache(cover_path, image, 8u << 20) ||
            http_get("https://coverartarchive.org/release/" + selected.id + "/front-500", image,
                     8u << 20) == 200)
        {
            int width = 0, height = 0, channels = 0;
            if (stbi_info_from_memory(image.data(), int(image.size()), &width, &height,
                                      &channels) &&
                width > 0 && height > 0 && width <= 4096 && height <= 4096)
            {
                uint8_t *pixels = stbi_load_from_memory(image.data(), int(image.size()), &width,
                                                        &height, &channels, 4);
                if (pixels)
                {
                    auto cover = std::make_shared<std::vector<uint8_t>>(
                        pixels, pixels + size_t(width) * size_t(height) * 4);
                    stbi_image_free(pixels);
                    write_cache(cover_path, image);
                    std::lock_guard lock(mutex_);
                    if (metadata_generation_ == generation && snapshot_.disc.id == disc.id)
                    {
                        snapshot_.cover = std::move(cover);
                        snapshot_.cover_width = width;
                        snapshot_.cover_height = height;
                        ++snapshot_.cover_revision;
                    }
                }
            }
        }
        // Album notes are secondary to the cover and never block playback or
        // initial artwork. Cache each resource and respect MusicBrainz pacing.
        auto info_request = [&](const std::string &key, const std::string &url,
                                std::vector<uint8_t> &body)
        {
            const std::string path = cache_ + "/" + key + ".info.json";
            if (get_cache(path, body, 512u << 10)) return true;
            while (hui::sys::monotonic_us() < next_request && !stop_.load()) wait(50000);
            if (stop_.load()) return false;
            const int status = http_get(url, body, 512u << 10);
            next_request = hui::sys::monotonic_us() + 1500000;
            if (status != 200) { body.clear(); return false; }
            write_cache(path, body);
            return true;
        };
        std::vector<uint8_t> release_info, group_info;
        const bool release_ok = info_request(selected.id,
            "https://musicbrainz.org/ws/2/release/" + selected.id +
            "?fmt=json&inc=annotation+release-groups+labels", release_info);
        auto view = [](const std::vector<uint8_t> &bytes) -> std::string_view
        { return bytes.empty() ? std::string_view{} : std::string_view(reinterpret_cast<const char *>(bytes.data()),bytes.size()); };
        auto info = parse_album_info(view(release_info), {}, selected.id);
        bool group_ok = true;
        if (!info.group_id.empty() && info.annotation.empty())
        {
            group_ok = info_request(info.group_id,
                "https://musicbrainz.org/ws/2/release-group/" + info.group_id +
                "?fmt=json&inc=annotation+url-rels", group_info);
            info = parse_album_info(view(release_info), view(group_info), selected.id);
        }
        if (info.annotation.empty() && !info.wikidata_id.empty())
        {
            std::vector<uint8_t> entity, summary;
            if (info_request("wikidata-" + info.wikidata_id,
                "https://www.wikidata.org/wiki/Special:EntityData/" + info.wikidata_id + ".json", entity))
            {
                const std::string title = parse_wikipedia_title(view(entity), info.wikidata_id);
                if (!title.empty() && info_request("wikipedia-" + info.wikidata_id,
                    "https://en.wikipedia.org/api/rest_v1/page/summary/" + url_encode(title), summary))
                {
                    info.annotation = parse_wikipedia_description(view(summary));
                    if (!info.annotation.empty())
                    {
                        info.source = "Wikipedia";
                        info.source_url = "https://en.wikipedia.org/wiki/" + url_encode(title);
                    }
                }
            }
        }
        {
            std::lock_guard lock(mutex_);
            if (metadata_generation_ == generation && snapshot_.disc.id == disc.id)
            {
                snapshot_.album_info = std::move(info);
                snapshot_.album_info_ready = true;
                snapshot_.album_info_error = !release_ok || !group_ok;
                ++snapshot_.revision;
            }
        }
        completed = generation;
    }
}
} // namespace cd
