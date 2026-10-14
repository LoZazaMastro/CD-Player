// CD Player - Reentrant calendar adapter for the embedded archive interpreter.
// Copyright (C) 2026 Andrea.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <ctime>
#include <mutex>
extern "C" struct tm *localtime_r(const time_t *value,struct tm *out)
{
    static std::mutex mutex;
    std::lock_guard lock(mutex);
    const struct tm *result=std::localtime(value);
    if(!result)return nullptr;
    *out=*result;
    return out;
}
