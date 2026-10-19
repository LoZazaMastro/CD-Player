// CD Player - Reflective three-dimensional album case.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gfx/canvas.hpp"

namespace cd
{
class CoverScene
{
    GLuint program_ = 0, vao_ = 0;
    hui::gfx::Canvas canvas_;
    float last_angle_ = -100, last_back_fade_ = -1, last_spine_fade_ = -1;
    GLuint last_front_ = 0, last_back_ = 0, last_spine_ = 0;
  public:
    ~CoverScene();
    bool init();
    void invalidate() { last_angle_ = -100; }
    GLuint render(float angle, GLuint front, GLuint back, GLuint spine,
                  float back_fade = 1, float spine_fade = 1, int size = 1024);
};
}
