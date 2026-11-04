// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Contract for the optional RED R3D reader (D068). The default build compiles
// the stub, so this test asserts that `.r3d` is still recognized and that every
// entry point explains how to get a BYO-SDK build instead of failing silently.
#include <cstdio>
#include <string>

#include "sr/red.hpp"

namespace {

int g_failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

}  // namespace

int main() {
    check(sr::redIsRedFile("clip.r3d"), "lowercase .r3d is recognized");
    check(sr::redIsRedFile("CLIP.R3D"), "uppercase .R3D is recognized");
    check(sr::redIsRedFile("A001_C001.R3d"), "mixed-case .R3d is recognized");
    check(!sr::redIsRedFile("clip.mov"), "non-r3d is rejected");
    check(!sr::redIsRedFile("r3d"), "an extension-less name is rejected");
    check(!sr::redIsRedFile("clip.r3d.tmp"), "a later extension wins");

    const sr::RedRuntime runtime = sr::redRuntime();
    check(!runtime.message.empty(), "runtime always has a message");
    if (!runtime.built) {
        check(!runtime.available, "stub is never available");
        check(runtime.sdkVersion.empty(), "stub has no SDK version");
        sr::RedClip clip;
        std::string error;
        check(!sr::redProbeClip("missing.r3d", clip, error), "stub cannot probe a clip");
        check(!error.empty(), "stub probe explains why");
        std::printf("RED stub: %s\n", runtime.message.c_str());
    } else {
        std::printf("RED SDK build: available=%d, version=%s, note=%s\n",
                    runtime.available ? 1 : 0, runtime.sdkVersion.c_str(),
                    runtime.message.c_str());
    }

    if (g_failures == 0) {
        std::printf("all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
