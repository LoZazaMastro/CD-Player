// CD Player - Chinese dot-matrix font and fallback rendering regression tests.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gfx/font.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

std::string read_file(const char *path)
{
    std::ifstream file(path, std::ios::binary);
    assert(file.is_open());
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

} // namespace

int main()
{
    hui::gfx::Font doto;
    const std::string doto_bytes = read_file("assets/fonts/doto-semibold.huifont");
    assert(doto.load(doto_bytes));

    hui::gfx::Font chinese;
    const std::string zh_bytes = read_file("assets/fonts/dotted-songti.huifont");
    assert(chinese.load(zh_bytes));

    // Verify key glyphs exist in the dot-matrix Chinese font
    // Simplified Chinese
    for (char32_t cp : {U'周', U'杰', U'伦', U'范', U'特', U'西', U'晴', U'天', U'双', U'截', U'棍'})
        assert(chinese.has_glyph(cp));

    // Traditional Chinese
    for (char32_t cp : {U'鄧', U'麗', U'君', U'葉', U'惠', U'美', U'國', U'體', U'學', U'樂'})
        assert(chinese.has_glyph(cp));

    // CJK Punctuation & symbols
    for (char32_t cp : {U'《', U'》', U'【', U'】', U'「', U'」', U'、', U'。', U'…', U'·'})
        assert(chinese.has_glyph(cp));

    // Doto alone does not have Chinese characters
    assert(!doto.has_glyph(U'周'));
    assert(!doto.has_glyph(U'杰'));

    // Attach Chinese font as fallback to Doto
    constexpr std::uint32_t kZhTextureHandle = 0xf0000005;
    doto.add_fallback(&chinese, kZhTextureHandle);

    // After fallback attachment, Doto can resolve Chinese glyphs
    assert(doto.has_glyph(U'周'));
    assert(doto.has_glyph(U'杰'));
    assert(doto.has_glyph(U'鄧'));

    // Measure CD track metadata containing mixed English and Chinese
    const std::string album_title = "周杰伦 - 范特西 (Fantasy)";
    const float size = 32.0f;
    const float measured_width = doto.measure(album_title, size);
    assert(measured_width > 0.0f);

    // Layout quads
    std::vector<hui::gfx::GlyphQuad> quads;
    const float advance = doto.layout(album_title, 100.0f, 200.0f, size, hui::gfx::Align::left, quads);
    assert(advance == measured_width);

    // Check that Chinese glyphs use the fallback texture while Latin glyphs use 0 (primary)
    int primary_count = 0;
    int fallback_count = 0;
    for (const auto &q : quads)
    {
        if (q.texture == kZhTextureHandle)
            fallback_count++;
        else if (q.texture == 0)
            primary_count++;
    }
    assert(fallback_count == 6); // 周, 杰, 伦, 范, 特, 西
    assert(primary_count > 0);   // ' ', '-', '(', 'F', 'a', 'n', 't', 'a', 's', 'y', ')'

    // Test text truncation with ellipsis on Chinese text
    const std::string long_title = "周杰伦 · 范特西 · 01. 爱在西元前 (03:54)";
    const std::string fitted = doto.fit(long_title, size, 250.0f);
    assert(!fitted.empty());
    assert(doto.measure(fitted, size) <= 250.0f);

    // Test Traditional Chinese CD info
    const std::string tc_track = "01. 鄧麗君 - 月亮代表我的心 (03:29)";
    quads.clear();
    doto.layout(tc_track, 0.0f, 0.0f, size, hui::gfx::Align::left, quads);
    int tc_count = 0;
    for (const auto &q : quads)
    {
        if (q.texture == kZhTextureHandle)
            tc_count++;
    }
    assert(tc_count == 10); // 鄧, 麗, 君, 月, 亮, 代, 表, 我, 的, 心

    // Test word wrapping with continuous Chinese characters (no spaces)
    const std::string lyrics = "快使用双截棍哼哼哈兮快使用双截棍哼哼哈兮习武之人切记仁者无敌是谁在练太极风生水起";
    const auto wrapped = doto.wrap(lyrics, size, 200.0f);
    assert(wrapped.size() > 1);
    for (const auto &line : wrapped)
    {
        assert(!line.empty());
        assert(doto.measure(line, size) <= 200.0f);
    }

    std::cout << "Chinese dot-matrix font tests passed: Simplified/Traditional glyphs, fallback resolution, mixed layout, fit, wrap\n";
    return 0;
}

