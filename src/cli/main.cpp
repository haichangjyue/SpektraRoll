// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 spektravideo contributors
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "sv/image.hpp"
#include "sv/lut3d.hpp"

namespace {

constexpr const char* kVersion = "0.1.0";

void printUsage() {
    std::printf(
        "spektravideo-cli %s\n"
        "\n"
        "Usage:\n"
        "  spektravideo-cli gen --out <file.png> [--width N] [--height N]\n"
        "      Write the built-in test chart (grey ramp + colour patches).\n"
        "\n"
        "  spektravideo-cli lut info --lut <file.cube>\n"
        "      Show table size, domain and title.\n"
        "\n"
        "  spektravideo-cli lut gen --out <file.cube> [--size 33] [--preset warm|invert|identity]\n"
        "      Write a synthetic LUT (demo and smoke test, not a film model).\n"
        "\n"
        "  spektravideo-cli lut apply --lut <file.cube> --in <image> --out <image>\n"
        "                             [--interp tetra|tri] [--strength 0..1]\n"
        "      Apply a 3D LUT to an image.\n"
        "\n"
        "  spektravideo-cli version\n"
        "\n"
        "Notes:\n"
        "  * Only .cube LUTs are supported for now.\n"
        "  * No colour management yet: pixels are treated as-is, 8-bit in and out.\n"
        "    This is milestone P0 scaffolding, not the final colour pipeline.\n",
        kVersion);
}

struct Args {
    std::vector<std::string> positional;
    std::map<std::string, std::string> flags;

    bool has(const std::string& key) const { return flags.count(key) != 0; }

    std::string get(const std::string& key, const std::string& fallback = {}) const {
        const auto it = flags.find(key);
        return it == flags.end() ? fallback : it->second;
    }
};

Args parse(int argc, char** argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        std::string token = argv[i];
        if (token.rfind("--", 0) == 0) {
            const auto eq = token.find('=');
            if (eq != std::string::npos) {
                args.flags[token.substr(2, eq - 2)] = token.substr(eq + 1);
            } else if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) {
                args.flags[token.substr(2)] = argv[++i];
            } else {
                args.flags[token.substr(2)] = "1";
            }
        } else {
            args.positional.push_back(token);
        }
    }
    return args;
}

int fail(const std::string& message) {
    std::fprintf(stderr, "error: %s\n", message.c_str());
    return 1;
}

int cmdGen(const Args& args) {
    const std::string out = args.get("out");
    if (out.empty()) return fail("gen requires --out <file.png>");

    const int width = std::atoi(args.get("width", "1920").c_str());
    const int height = std::atoi(args.get("height", "1080").c_str());

    const sv::Image image = sv::makeTestChart(width, height);
    std::string error;
    if (!sv::saveImage(out, image, error)) return fail(error);

    std::printf("wrote %s (%dx%d)\n", out.c_str(), image.width, image.height);
    return 0;
}

sv::Rgb warmPreset(float r, float g, float b) {
    // A deliberately simple, film-ish curve so the demo is not a no-op:
    // lifted shadows, slightly rolled highlights, warm crossover.
    const auto curve = [](float v, float lift, float gain) {
        v = std::min(1.0f, std::max(0.0f, v));
        const float s = v * v * (3.0f - 2.0f * v);  // smoothstep
        return std::min(1.0f, lift + (gain - lift) * (0.55f * v + 0.45f * s));
    };
    return sv::Rgb{curve(r, 0.03f, 0.97f), curve(g, 0.025f, 0.95f), curve(b, 0.05f, 0.90f)};
}

int cmdLutGen(const Args& args) {
    const std::string path = args.get("out");
    if (path.empty()) return fail("lut gen requires --out <file.cube>");

    const int size = std::max(2, std::atoi(args.get("size", "33").c_str()));
    const std::string preset = args.get("preset", "warm");

    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) return fail("cannot write LUT file: " + path);

    std::fprintf(file, "TITLE \"spektravideo %s preset\"\n", preset.c_str());
    std::fprintf(file, "LUT_3D_SIZE %d\n", size);
    std::fprintf(file, "DOMAIN_MIN 0.0 0.0 0.0\n");
    std::fprintf(file, "DOMAIN_MAX 1.0 1.0 1.0\n");

    const float scale = 1.0f / static_cast<float>(size - 1);
    for (int b = 0; b < size; ++b) {
        for (int g = 0; g < size; ++g) {
            for (int r = 0; r < size; ++r) {
                const float fr = r * scale;
                const float fg = g * scale;
                const float fb = b * scale;
                sv::Rgb v{fr, fg, fb};
                if (preset == "invert") {
                    v = {1.0f - fr, 1.0f - fg, 1.0f - fb};
                } else if (preset == "warm") {
                    v = warmPreset(fr, fg, fb);
                }
                std::fprintf(file, "%.6f %.6f %.6f\n", v.r, v.g, v.b);
            }
        }
    }
    std::fclose(file);

    std::printf("wrote %s (%d^3, preset %s)\n", path.c_str(), size, preset.c_str());
    return 0;
}

int cmdLutInfo(const Args& args) {    const std::string path = args.get("lut");
    if (path.empty()) return fail("lut info requires --lut <file.cube>");

    sv::Lut3D lut;
    std::string error;
    if (!lut.loadCube(path, error)) return fail(error);

    std::printf("file:    %s\n", path.c_str());
    std::printf("title:   %s\n", lut.title().empty() ? "(none)" : lut.title().c_str());
    std::printf("size:    %d^3 (%d entries)\n", lut.size(), lut.size() * lut.size() * lut.size());
    std::printf("domain:  [%g %g %g] .. [%g %g %g]\n", lut.domainMin().r, lut.domainMin().g,
                lut.domainMin().b, lut.domainMax().r, lut.domainMax().g, lut.domainMax().b);
    return 0;
}

int cmdLutApply(const Args& args) {
    const std::string lutPath = args.get("lut");
    const std::string inPath = args.get("in");
    const std::string outPath = args.get("out");
    if (lutPath.empty()) return fail("lut apply requires --lut <file.cube>");
    if (inPath.empty()) return fail("lut apply requires --in <image>");
    if (outPath.empty()) return fail("lut apply requires --out <image>");

    sv::Lut3D lut;
    std::string error;
    if (!lut.loadCube(lutPath, error)) return fail(error);

    sv::Image image;
    if (!sv::loadImage(inPath, image, error)) return fail(error);

    const std::string interpName = args.get("interp", "tetra");
    const sv::Interp mode =
        interpName == "tri" ? sv::Interp::Trilinear : sv::Interp::Tetrahedral;
    const float strength = static_cast<float>(std::atof(args.get("strength", "1.0").c_str()));

    const auto started = std::chrono::steady_clock::now();
    const size_t count = static_cast<size_t>(image.width) * image.height;
    for (size_t i = 0; i < count; ++i) {
        float* px = &image.pixels[i * 3];
        const sv::Rgb src{px[0], px[1], px[2]};
        const sv::Rgb mapped = lut.apply(src, mode);
        const sv::Rgb result = sv::mix(src, mapped, strength);
        px[0] = result.r;
        px[1] = result.g;
        px[2] = result.b;
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - started)
                             .count();

    if (!sv::saveImage(outPath, image, error)) return fail(error);

    std::printf("applied %s (%d^3) to %s\n", lutPath.c_str(), lut.size(), inPath.c_str());
    std::printf("wrote %s (%dx%d, %zu pixels, %.1f ms, %s)\n", outPath.c_str(), image.width,
                image.height, count, elapsed,
                mode == sv::Interp::Trilinear ? "trilinear" : "tetrahedral");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const Args args = parse(argc, argv);

    if (args.positional.empty()) {
        printUsage();
        return 1;
    }

    const std::string& command = args.positional[0];

    if (command == "version" || args.has("version")) {
        std::printf("spektravideo-cli %s\n", kVersion);
        return 0;
    }
    if (command == "help" || args.has("help")) {
        printUsage();
        return 0;
    }
    if (command == "gen") return cmdGen(args);

    if (command == "lut" && args.positional.size() >= 2) {
        const std::string& sub = args.positional[1];
        if (sub == "info") return cmdLutInfo(args);
        if (sub == "gen") return cmdLutGen(args);
        if (sub == "apply") return cmdLutApply(args);
        return fail("unknown lut subcommand: " + sub);
    }

    std::fprintf(stderr, "error: unknown command: %s\n\n", command.c_str());
    printUsage();
    return 1;
}
