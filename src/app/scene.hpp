// CD Player - Disc material and audio-reactive spatial scenes.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gfx/canvas.hpp"
#include <array>
#include <string>

namespace cd
{
class Scene
{
    GLuint program_ = 0, vao_ = 0, engraving_ = 0;
    hui::gfx::Canvas canvas_;

  public:
    ~Scene();
    bool init(const std::string &assets);
    GLuint render(float spin, int width, int height);
};
} // namespace cd
