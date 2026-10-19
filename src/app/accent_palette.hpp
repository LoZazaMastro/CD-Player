// CD Player - Named interface accent colors.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>

namespace cd
{
struct AccentColor { const char *name; uint32_t rgb; };
inline constexpr std::array<AccentColor, 40> accent_colors = {{
    {"Crimson", 0xd71920}, {"Scarlet", 0xff3b30}, {"Ruby", 0xda3268},
    {"Coral", 0xff6b5c}, {"Salmon", 0xff9383}, {"Tangerine", 0xff713c},
    {"Orange", 0xff922b}, {"Amber", 0xffb229}, {"Gold", 0xeac144},
    {"Sunflower", 0xffd43b}, {"Lemon", 0xf5ed74}, {"Chartreuse", 0xc5e53d},
    {"Lime", 0xa4dc4b}, {"Apple", 0x78cb64}, {"Leaf", 0x52ba68},
    {"Emerald", 0x2abf88}, {"Mint", 0x8ce5b3}, {"Seafoam", 0x78d8bf},
    {"Teal", 0x36b8ab}, {"Turquoise", 0x42d4cb}, {"Cyan", 0x48d9ed},
    {"Ice", 0xade8f4}, {"Sky", 0x7ac7f1}, {"Azure", 0x44a8ef},
    {"Blue", 0x548bff}, {"Cobalt", 0x6880ec}, {"Indigo", 0x8779ed},
    {"Iris", 0xa391f4}, {"Violet", 0xb385f5}, {"Purple", 0xbe75df},
    {"Lavender", 0xc8b0ee}, {"Orchid", 0xd795e1}, {"Magenta", 0xe566cc},
    {"Fuchsia", 0xed64ae}, {"Rose", 0xef7fa6}, {"Pink", 0xf3a3c5},
    {"Peach", 0xf3b995}, {"Sand", 0xdcc8a1}, {"Copper", 0xc78f6b},
    {"Silver", 0xc9ced5}
}};
} // namespace cd
