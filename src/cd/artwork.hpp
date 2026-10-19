// CD Player - Release-matched physical CD artwork.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cd
{
struct ArtworkImage { int width=0, height=0; std::vector<uint8_t> rgba; };
struct ArtworkSources
{
    std::string disc, back, spine;
    bool combined_spine=false, back_has_spines=false;
};
enum class ArtworkCrop { disc, back, back_with_spines, spine, spine_from_back };
ArtworkSources parse_artwork_sources(std::string_view json, const std::string &release,
                                     unsigned medium=1, unsigned medium_count=1);
std::shared_ptr<const ArtworkImage> decode_artwork(std::span<const uint8_t>, ArtworkCrop);
} // namespace cd
