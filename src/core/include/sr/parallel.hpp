// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#pragma once

#include <algorithm>
#include <cstddef>
#include <thread>
#include <vector>

namespace sr {

// Splits [0, count) into `threads` contiguous chunks and runs `body(begin, end)`
// on each in its own thread. `threads <= 1` runs inline. The work must be
// independent per index, which is true for everything in this codebase: the
// film model and the LUT are pure per-pixel functions.
template <class Body>
void parallelChunks(std::size_t count, int threads, Body&& body) {
    if (count == 0) return;
    const int workers = std::max(1, std::min<int>(threads, static_cast<int>(count)));
    if (workers == 1) {
        body(std::size_t{0}, count);
        return;
    }

    std::vector<std::thread> pool;
    pool.reserve(static_cast<std::size_t>(workers) - 1);
    const std::size_t chunk = (count + static_cast<std::size_t>(workers) - 1) /
                              static_cast<std::size_t>(workers);
    for (int i = 1; i < workers; ++i) {
        const std::size_t begin = chunk * static_cast<std::size_t>(i);
        const std::size_t end = std::min(count, begin + chunk);
        if (begin >= end) break;
        pool.emplace_back([&body, begin, end] { body(begin, end); });
    }
    body(std::size_t{0}, std::min(count, chunk));
    for (std::thread& worker : pool) worker.join();
}

// 0 or negative means "decide for me": all hardware threads minus one, so the
// encoder/decoder processes running alongside still get a core.
inline int resolveThreadCount(int requested) {
    if (requested > 0) return requested;
    const unsigned hw = std::thread::hardware_concurrency();
    return hw > 2 ? static_cast<int>(hw) - 1 : 1;
}

}  // namespace sr
