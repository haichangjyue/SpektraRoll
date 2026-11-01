// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "sr/film_model.hpp"
#include "sr/image.hpp"
#include "sr/lut3d.hpp"
#include "sr/metrics.hpp"
#include "sr/parallel.hpp"
#include "sr/profile.hpp"
#include "sr/texture.hpp"

namespace {

constexpr const char* kVersion = "0.1.0";

constexpr const char* kDefaultFilm = "data/profiles/kodak_portra_400.json";
constexpr const char* kDefaultPaper = "data/profiles/kodak_portra_endura.json";
constexpr const char* kDefaultFilters = "data/profiles/neutral_print_filters.json";

void printUsage() {
    std::printf(
        "spektra-roll-cli %s\n"
        "\n"
        "Usage:\n"
        "  spektra-roll-cli gen --out <file.png> [--width N] [--height N] [--bits 8|16]\n"
        "      Write the built-in test chart (grey ramp + colour patches).\n"
        "\n"
        "  spektra-roll-cli convert --in <image> --out <image> [--from srgb|apple-log]\n"
        "                           [--to srgb] [--exposure EV] [--bits 8|16]\n"
        "      Decode an image to display-encoded sRGB without the film model. Used\n"
        "      to render an Apple Log frame as a plain SDR reference.\n"
        "\n"
        "  spektra-roll-cli video --lut <file.cube> --in <clip> --out <clip>\n"
        "                         [--trc srgb|pq|hlg] [--in-matrix bt2020|bt709]\n"
        "                         [--start 0] [--frames N | --duration T]\n"
        "                         [--codec hevc|h264|prores]\n"
        "                         [--interp tetra|tri] [--strength 0..1] [--threads N]\n"
        "                         [--grain 0..1] [--halation 0..1] [--seed N]\n"
        "                         [--glare 0..1] [--diffusion 0..1]\n"
        "                         [--ffmpeg <path>]\n"
        "      Decode a clip with ffmpeg, apply the LUT frame by frame and re-encode\n"
        "      with the original audio. --start/--frames pick a range (seconds or\n"
        "      HH:MM:SS). HDR output is HEVC Main10 (or ProRes 422 HQ) with PQ/HLG\n"
        "      tags.\n"
        "\n"
        "  spektra-roll-cli lut info --lut <file.cube>\n"
        "      Show table size, domain and title.\n"
        "\n"
        "  spektra-roll-cli lut gen --out <file.cube> [--size 33] [--preset warm|invert|identity]\n"
        "      Write a synthetic LUT (demo and smoke test, not a film model).\n"
        "\n"
        "  spektra-roll-cli lut bake --out <file.cube> [--film <json>] [--paper <json>]\n"
        "                            [--filters <json>] [--cc C,M,Y] [--size 33]\n"
        "                            [--input srgb|apple-log] [--exposure EV]\n"
        "                            [--scan print|negative] [--contrast 1.15]\n"
        "                            [--gamut oklch|off] [--couplers on|off]\n"
        "                            [--hdr pq|hlg] [--peak 1000]\n"
        "                            [--threads N] [--title <text>]\n"
        "      Bake the M1 spectral film model (Kodak Portra 400 + Portra Endura\n"
        "      by default) into a .cube LUT. Output is display-encoded sRGB. With\n"
        "      --input apple-log the LUT expects Apple Log code values in BT.2020\n"
        "      primaries (e.g. a frame decoded from an iPhone ProRes/HEVC clip),\n"
        "      and --exposure shifts scene exposure in stops before the film model.\n"
        "      --scan negative scans the negative itself (inverted against the film\n"
        "      base, no paper): more highlight/shadow latitude, flatter image.\n"
        "      --contrast pivots at 18% grey (negative scans default to 1.15,\n"
        "      pass 1.0 for the flat film curve only).\n"
        "      --gamut oklch maps out-of-gamut colour back into sRGB by reducing\n"
        "      OkLCh chroma; negative scans default to it, print keeps it off.\n"
        "      --couplers on adds DIR interlayer inhibition (Apple Log defaults\n"
        "      to on, the sRGB reference keeps it off).\n"
        "      --hdr pq writes ST 2084 code values with the image's white at\n"
        "      --peak nits; --hdr hlg writes BT.2100 HLG instead.\n"
        "\n"
        "  spektra-roll-cli lut apply --lut <file.cube> --in <image> --out <image>\n"
        "                             [--interp tetra|tri] [--strength 0..1] [--bits 8|16]\n"
        "                             [--grain 0..1] [--halation 0..1]\n"
        "                             [--glare 0..1] [--diffusion 0..1]\n"
        "      Apply a 3D LUT to an image. Output bit depth defaults to the input's.\n"
        "\n"
        "  spektra-roll-cli lut apply --lut <file.cube> --in-dir <dir> --out-dir <dir>\n"
        "                             [--interp tetra|tri] [--strength 0..1] [--bits 8|16]\n"
        "      Apply the LUT to every PNG in --in-dir (sorted by name). Used to grade\n"
        "      image sequences without starting the process once per frame.\n"
        "\n"
        "  spektra-roll-cli lut compare --a <reference.cube> --b <test.cube>\n"
        "                               [--min-psnr 60]\n"
        "      Compare two LUTs entry by entry (PSNR and CIE76 delta E).\n"
        "      Exits non-zero when PSNR is below --min-psnr.\n"
        "\n"
        "  spektra-roll-cli version\n"
        "\n"
        "Notes:\n"
        "  * Only .cube LUTs are supported for now.\n"
        "  * The film model is a pure per-pixel function (no grain, halation,\n"
        "    glare or diffusion); those run per frame in `video` and `lut apply`.\n",
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

    int getInt(const std::string& key, int fallback) const {
        const auto it = flags.find(key);
        return it == flags.end() ? fallback : std::atoi(it->second.c_str());
    }

    double getDouble(const std::string& key, double fallback) const {
        const auto it = flags.find(key);
        return it == flags.end() ? fallback : std::atof(it->second.c_str());
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

int requestedBits(const Args& args, int fallback) {
    if (!args.has("bits")) return fallback;
    const int bits = args.getInt("bits", fallback);
    if (bits != 8 && bits != 16) {
        std::fprintf(stderr, "warning: --bits %d is not supported, using %d\n", bits, fallback);
        return fallback;
    }
    return bits;
}

int cmdGen(const Args& args) {
    const std::string out = args.get("out");
    if (out.empty()) return fail("gen requires --out <file.png>");

    const int width = std::atoi(args.get("width", "1920").c_str());
    const int height = std::atoi(args.get("height", "1080").c_str());
    const int bits = requestedBits(args, 8);

    const sr::Image image = sr::makeTestChart(width, height);
    std::string error;
    if (!sr::saveImage(out, image, error, bits)) return fail(error);

    std::printf("wrote %s (%dx%d, %d-bit)\n", out.c_str(), image.width, image.height, bits);
    return 0;
}

sr::Rgb warmPreset(float r, float g, float b) {
    // A deliberately simple, film-ish curve so the demo is not a no-op:
    // lifted shadows, slightly rolled highlights, warm crossover.
    const auto curve = [](float v, float lift, float gain) {
        v = std::min(1.0f, std::max(0.0f, v));
        const float s = v * v * (3.0f - 2.0f * v);  // smoothstep
        return std::min(1.0f, lift + (gain - lift) * (0.55f * v + 0.45f * s));
    };
    return sr::Rgb{curve(r, 0.03f, 0.97f), curve(g, 0.025f, 0.95f), curve(b, 0.05f, 0.90f)};
}

int cmdLutGen(const Args& args) {
    const std::string path = args.get("out");
    if (path.empty()) return fail("lut gen requires --out <file.cube>");

    const int size = std::max(2, std::atoi(args.get("size", "33").c_str()));
    const std::string preset = args.get("preset", "warm");

    std::vector<sr::Rgb> values(static_cast<size_t>(size) * size * size);
    const float scale = 1.0f / static_cast<float>(size - 1);
    for (int b = 0; b < size; ++b) {
        for (int g = 0; g < size; ++g) {
            for (int r = 0; r < size; ++r) {
                sr::Rgb v{r * scale, g * scale, b * scale};
                if (preset == "invert") {
                    v = {1.0f - v.r, 1.0f - v.g, 1.0f - v.b};
                } else if (preset == "warm") {
                    v = warmPreset(v.r, v.g, v.b);
                }
                values[(static_cast<size_t>(b) * size + g) * size + r] = v;
            }
        }
    }

    const sr::Lut3D lut =
        sr::Lut3D::fromData(size, std::move(values), "SpektraRoll " + preset + " preset");
    std::string error;
    if (!lut.saveCube(path, error)) return fail(error);

    std::printf("wrote %s (%d^3, preset %s)\n", path.c_str(), size, preset.c_str());
    return 0;
}

int cmdLutInfo(const Args& args) {
    const std::string path = args.get("lut");
    if (path.empty()) return fail("lut info requires --lut <file.cube>");

    sr::Lut3D lut;
    std::string error;
    if (!lut.loadCube(path, error)) return fail(error);

    std::printf("file:    %s\n", path.c_str());
    std::printf("title:   %s\n", lut.title().empty() ? "(none)" : lut.title().c_str());
    std::printf("size:    %d^3 (%d entries)\n", lut.size(), lut.size() * lut.size() * lut.size());
    std::printf("domain:  [%g %g %g] .. [%g %g %g]\n", lut.domainMin().r, lut.domainMin().g,
                lut.domainMin().b, lut.domainMax().r, lut.domainMax().g, lut.domainMax().b);
    return 0;
}

bool parseTriple(const std::string& text, sr::Triple& out) {
    std::string normalized = text;
    std::replace(normalized.begin(), normalized.end(), ',', ' ');
    std::istringstream stream(normalized);
    return static_cast<bool>(stream >> out[0] >> out[1] >> out[2]);
}

int cmdLutBake(const Args& args) {
    const std::string out = args.get("out", "out/portra400_endura_33.cube");
    const std::string filmPath = args.get("film", kDefaultFilm);
    const std::string paperPath = args.get("paper", kDefaultPaper);
    const std::string filtersPath = args.get("filters", kDefaultFilters);
    const int size = std::max(2, args.getInt("size", 33));

    sr::Profile film;
    sr::Profile paper;
    std::string error;
    if (!sr::loadProfile(filmPath, film, error)) return fail(error);
    if (!sr::loadProfile(paperPath, paper, error)) return fail(error);

    sr::Triple filters{0.0, 0.0, 0.0};
    if (args.has("cc")) {
        if (!parseTriple(args.get("cc"), filters)) {
            return fail("--cc expects three numbers, e.g. --cc 0,51.57,52.53");
        }
    } else if (!sr::loadNeutralFilters(filtersPath, paper.info.stock, film.info.stock, "TH-KG3",
                                       filters, error)) {
        return fail(error + " (pass --cc C,M,Y to override)");
    }

    sr::BakeOptions options;
    if (args.has("input")) {
        if (!sr::parseInputTransfer(args.get("input"), options.input)) {
            return fail("--input expects 'srgb' or 'apple-log'");
        }
    }
    options.exposureEv = args.getDouble("exposure", 0.0);
    options.threads = args.getInt("threads", 0);

    if (args.has("scan")) {
        const std::string mode = args.get("scan");
        if (mode == "negative" || mode == "film") {
            options.scan = sr::ScanMode::Negative;
        } else if (mode != "print") {
            return fail("--scan expects 'print' or 'negative'");
        }
    }
    // Negative scans default to a little contrast (D026); the print path ignores
    // this value entirely.
    options.contrast =
        args.getDouble("contrast", options.scan == sr::ScanMode::Negative ? 1.15 : 1.0);

    // Gamut compression defaults on only for Apple Log input (D027): the print    // path from Apple Log can produce negative linear values, while the sRGB
    // path must stay byte-identical to the M1 reference.
    options.gamutCompress = options.input == sr::InputTransfer::AppleLog;
    if (args.has("gamut")) {
        const std::string mode = args.get("gamut");
        if (mode == "oklch") {
            options.gamutCompress = true;
        } else if (mode == "off") {
            options.gamutCompress = false;
        } else {
            return fail("--gamut expects 'off' or 'oklch'");
        }
    }

    // DIR couplers default on for Apple Log and off for the sRGB reference
    // (D028); `--couplers off` turns them back off.
    bool couplers = options.input == sr::InputTransfer::AppleLog;
    if (args.has("couplers")) {
        const std::string value = args.get("couplers");
        if (value == "on") {
            couplers = true;
        } else if (value == "off") {
            couplers = false;
        } else {
            return fail("--couplers expects 'on' or 'off'");
        }
    }

    sr::FilmModel model;
    sr::FilmModelOptions modelOptions;
    modelOptions.couplers = couplers;
    if (!model.configure(film, paper, filters, error, modelOptions)) return fail(error);

    if (args.has("hdr")) {
        const std::string value = args.get("hdr");
        if (value == "pq") {
            options.hdr = sr::HdrMode::Pq;
        } else if (value == "hlg") {
            options.hdr = sr::HdrMode::Hlg;
        } else if (value != "off") {
            return fail("--hdr expects 'pq', 'hlg' or 'off'");
        }
    }
    options.peakNits = args.getDouble("peak", 1000.0);

    std::printf("film:              %s (%s)\n", film.info.name.c_str(), film.info.stock.c_str());
    std::printf("paper:             %s (%s)\n", paper.info.name.c_str(), paper.info.stock.c_str());
    std::printf("neutral filters:   C=%.3f M=%.3f Y=%.3f\n", filters[0], filters[1], filters[2]);
    std::printf("print exposure:    %.12f\n", model.printExposureFactor());
    std::printf("input:             %s", sr::inputTransferName(options.input));
    if (options.exposureEv != 0.0) std::printf(" (%+.2f EV)", options.exposureEv);
    std::printf("\n");
    std::printf("scan:              %s\n",
                options.scan == sr::ScanMode::Negative ? "negative (no paper)" : "print");
    if (options.contrast != 1.0) std::printf("contrast:          %.2f\n", options.contrast);
    std::printf("gamut:             %s\n", options.gamutCompress ? "oklch" : "off");
    std::printf("couplers:          %s\n", couplers ? "on" : "off");
    if (options.hdr != sr::HdrMode::Off) {
        std::printf("hdr:               %s, white at %.0f nits\n",
                    options.hdr == sr::HdrMode::Pq ? "pq" : "hlg", options.peakNits);
    }

    const auto started = std::chrono::steady_clock::now();
    const std::string title =
        args.get("title", "SpektraRoll " + film.info.stock + " + " + paper.info.stock);
    const sr::Lut3D lut = model.bake(size, title, options);
    if (lut.empty()) return fail("failed to bake the LUT");

    if (!lut.saveCube(out, error)) return fail(error);
    const auto elapsed =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();

    const sr::Rgb mid = model.transformFrom(options, {0.5f, 0.5f, 0.5f});
    const sr::Rgb black = model.transformFrom(options, {0.0f, 0.0f, 0.0f});
    const sr::Rgb white = model.transformFrom(options, {1.0f, 1.0f, 1.0f});

    std::printf("wrote %s (%d^3, %d entries, %.1f ms)\n", out.c_str(), size,
                size * size * size, elapsed);
    std::printf("probe 0.0 -> %.5f %.5f %.5f\n", black.r, black.g, black.b);
    std::printf("probe 0.5 -> %.5f %.5f %.5f\n", mid.r, mid.g, mid.b);
    std::printf("probe 1.0 -> %.5f %.5f %.5f\n", white.r, white.g, white.b);
    return 0;
}

int cmdLutCompare(const Args& args) {
    const std::string pathA = args.get("a");
    const std::string pathB = args.get("b");
    if (pathA.empty()) return fail("lut compare requires --a <reference.cube>");
    if (pathB.empty()) return fail("lut compare requires --b <test.cube>");

    sr::Lut3D a;
    sr::Lut3D b;
    std::string error;
    if (!a.loadCube(pathA, error)) return fail(error);
    if (!b.loadCube(pathB, error)) return fail(error);

    sr::LutComparison result;
    if (!sr::compareLuts(a, b, result, error)) return fail(error);

    std::printf("reference:  %s\n", pathA.c_str());
    std::printf("test:       %s\n", pathB.c_str());
    std::printf("samples:    %zu (%d^3)\n", result.samples, a.size());
    std::printf("PSNR:       %.2f dB\n", result.psnr);
    std::printf("max |diff|: %.3e  (at input %.3f %.3f %.3f)\n", result.maxAbs,
                result.worstInput.r, result.worstInput.g, result.worstInput.b);
    std::printf("mean|diff|: %.3e\n", result.meanAbs);
    std::printf("deltaE76:   mean %.4f, p95 %.4f, max %.4f\n", result.meanDeltaE,
                result.p95DeltaE, result.maxDeltaE);

    const double minPsnr = std::atof(args.get("min-psnr", "60").c_str());
    if (result.psnr < minPsnr) {
        std::printf("FAIL: PSNR is below %.1f dB\n", minPsnr);
        return 1;
    }
    std::printf("PASS: PSNR >= %.1f dB\n", minPsnr);
    return 0;
}

// Spatial effects (D033): parsed once, applied per frame after the LUT.
struct TextureOptions {
    sr::GrainParams grain;
    sr::HalationParams halation;
    sr::GlareParams glare;
    sr::DiffusionParams diffusion;
};

TextureOptions parseTexture(const Args& args) {
    TextureOptions texture;
    texture.grain.amount = static_cast<float>(args.getDouble("grain", 0.0));
    texture.grain.size = static_cast<float>(args.getDouble("grain-size", 1.4));
    texture.grain.seed = static_cast<unsigned>(args.getInt("seed", 1));
    texture.halation.amount = static_cast<float>(args.getDouble("halation", 0.0));
    texture.halation.radius = static_cast<float>(args.getDouble("halation-radius", 0.006));
    texture.halation.threshold = static_cast<float>(args.getDouble("halation-threshold", 0.72));
    texture.glare.amount = static_cast<float>(args.getDouble("glare", 0.0));
    texture.glare.radius = static_cast<float>(args.getDouble("glare-radius", 0.02));
    texture.diffusion.amount = static_cast<float>(args.getDouble("diffusion", 0.0));
    texture.diffusion.radius = static_cast<float>(args.getDouble("diffusion-radius", 0.004));
    return texture;
}

void applyTexture(float* pixels, int width, int height, const TextureOptions& texture, int frame) {
    if (texture.halation.amount > 0.0f) {
        sr::applyHalation(pixels, width, height, texture.halation);
    }
    if (texture.glare.amount > 0.0f) {
        sr::applyGlare(pixels, width, height, texture.glare);
    }
    if (texture.diffusion.amount > 0.0f) {
        sr::applyDiffusion(pixels, width, height, texture.diffusion);
    }
    if (texture.grain.amount > 0.0f) {
        sr::GrainParams grain = texture.grain;
        grain.frame = frame;
        sr::applyGrain(pixels, width, height, grain);
    }
}

int applyDirectory(const sr::Lut3D& lut, sr::Interp mode, float strength,
                   const std::string& inDir, const std::string& outDir, const Args& args) {
    if (inDir.empty() || outDir.empty()) return fail("--in-dir and --out-dir go together");

    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(inDir, ec)) {
        if (!entry.is_regular_file()) continue;
        const std::string ext = entry.path().extension().string();
        if (ext == ".png" || ext == ".PNG") files.push_back(entry.path());
    }
    if (files.empty()) return fail("no PNG files in " + inDir);
    std::sort(files.begin(), files.end());
    std::filesystem::create_directories(outDir, ec);

    const auto started = std::chrono::steady_clock::now();
    size_t frames = 0;
    size_t pixels = 0;
    std::string error;
    int bits = 16;
    const int threads = sr::resolveThreadCount(args.getInt("threads", 0));
    const TextureOptions texture = parseTexture(args);
    for (const auto& path : files) {
        sr::Image image;
        int sourceBits = 8;
        if (!sr::loadImage(path.string(), image, error, &sourceBits)) return fail(error);
        const size_t count = static_cast<size_t>(image.width) * image.height;
        sr::parallelChunks(count, threads, [&](size_t begin, size_t end) {
            for (size_t i = begin; i < end; ++i) {
                float* px = &image.pixels[i * 3];
                const sr::Rgb src{px[0], px[1], px[2]};
                const sr::Rgb mapped = lut.apply(src, mode);
                const sr::Rgb result = sr::mix(src, mapped, strength);
                px[0] = result.r;
                px[1] = result.g;
                px[2] = result.b;
            }
        });
        applyTexture(image.pixels.data(), image.width, image.height, texture,
                     static_cast<int>(frames));
        bits = requestedBits(args, sourceBits);
        const auto outPath = std::filesystem::path(outDir) / path.filename();
        if (!sr::saveImage(outPath.string(), image, error, bits)) return fail(error);
        ++frames;
        pixels += count;
        if (frames % 10 == 0 || frames == files.size()) {
            std::printf("  frame %zu/%zu (%.0f%%)\n", frames, files.size(),
                        100.0 * frames / files.size());
            std::fflush(stdout);
        }
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - started)
                             .count();
    std::printf("applied %d^3 LUT to %zu frames, %zu pixels, %d-bit, %.2f s (%.1f ms/frame)\n",
                lut.size(), frames, pixels, bits, elapsed / 1000.0, elapsed / frames);
    return 0;
}

int cmdLutApply(const Args& args) {
    const std::string lutPath = args.get("lut");
    if (lutPath.empty()) return fail("lut apply requires --lut <file.cube>");

    sr::Lut3D lut;
    std::string error;
    if (!lut.loadCube(lutPath, error)) return fail(error);

    const std::string interpName = args.get("interp", "tetra");
    const sr::Interp mode =
        interpName == "tri" ? sr::Interp::Trilinear : sr::Interp::Tetrahedral;
    const float strength = static_cast<float>(std::atof(args.get("strength", "1.0").c_str()));

    if (args.has("in-dir") || args.has("out-dir")) {
        return applyDirectory(lut, mode, strength, args.get("in-dir"), args.get("out-dir"), args);
    }

    const std::string inPath = args.get("in");
    const std::string outPath = args.get("out");
    if (inPath.empty()) return fail("lut apply requires --in <image>");
    if (outPath.empty()) return fail("lut apply requires --out <image>");

    sr::Image image;
    int sourceBits = 8;
    if (!sr::loadImage(inPath, image, error, &sourceBits)) return fail(error);

    const int bits = requestedBits(args, sourceBits);

    const auto started = std::chrono::steady_clock::now();
    const size_t count = static_cast<size_t>(image.width) * image.height;
    const int threads = sr::resolveThreadCount(args.getInt("threads", 0));
    sr::parallelChunks(count, threads, [&](size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            float* px = &image.pixels[i * 3];
            const sr::Rgb src{px[0], px[1], px[2]};
            const sr::Rgb mapped = lut.apply(src, mode);
            const sr::Rgb result = sr::mix(src, mapped, strength);
            px[0] = result.r;
            px[1] = result.g;
            px[2] = result.b;
        }
    });
    applyTexture(image.pixels.data(), image.width, image.height, parseTexture(args), 0);
    const auto elapsed = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - started)
                             .count();

    if (!sr::saveImage(outPath, image, error, bits)) return fail(error);

    std::printf("applied %s (%d^3) to %s\n", lutPath.c_str(), lut.size(), inPath.c_str());
    std::printf("wrote %s (%dx%d, %zu pixels, %d-bit out from %d-bit in, %.1f ms, %s)\n",
                outPath.c_str(), image.width, image.height, count, bits, sourceBits, elapsed,
                mode == sr::Interp::Trilinear ? "trilinear" : "tetrahedral");
    return 0;
}

#ifdef _WIN32
FILE* openPipe(const std::string& command, const char* mode) {
    // cmd.exe needs the whole line wrapped when the program path is quoted.
    const std::string full = command.rfind("\"", 0) == 0 ? "\"" + command + "\"" : command;
    FILE* pipe = _popen(full.c_str(), mode);
    if (pipe) _setmode(_fileno(pipe), _O_BINARY);
    return pipe;
}
void closePipe(FILE* pipe) {
    if (pipe) _pclose(pipe);
}
#else
FILE* openPipe(const std::string& command, const char* mode) {
    // POSIX popen() only knows "r" and "w"; glibc tolerates a trailing 'b' as a
    // no-op, but the BSD libc on macOS rejects the whole string with EINVAL, so
    // "rb"/"wb" returned NULL and the video pipes never started (issue #1).
    // There is no text/binary distinction on POSIX, so pass the bare mode.
    return popen(command.c_str(), mode[0] == 'w' ? "w" : "r");
}
void closePipe(FILE* pipe) {
    if (pipe) pclose(pipe);
}
#endif

std::string shellQuote(const std::string& value) { return "\"" + value + "\""; }

// "12.5" or "00:01:12.5" -> seconds.
double parseSeconds(const std::string& text) {
    if (text.find(':') == std::string::npos) return std::atof(text.c_str());
    std::istringstream stream(text);
    std::string part;
    double total = 0.0;
    while (std::getline(stream, part, ':')) total = total * 60.0 + std::atof(part.c_str());
    return total;
}

std::string formatSeconds(double value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.6f", value);
    return buffer;
}

std::string runCapture(const std::string& command) {
    std::string out;
    FILE* pipe = openPipe(command, "r");
    if (!pipe) return out;
    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), pipe)) out += buffer;
    closePipe(pipe);
    return out;
}

std::string ffmpegPath(const Args& args) {    if (args.has("ffmpeg")) return args.get("ffmpeg");
    if (const char* env = std::getenv("SPEKTRA_FFMPEG")) return env;
    return "ffmpeg";
}

std::string siblingTool(const std::string& ffmpeg) {
#ifdef _WIN32
    const char* name = "ffprobe.exe";
#else
    const char* name = "ffprobe";
#endif
    const auto slash = ffmpeg.find_last_of("/\\");
    if (slash == std::string::npos) return name;
    return ffmpeg.substr(0, slash + 1) + name;
}

int cmdVideo(const Args& args) {
    const std::string inPath = args.get("in");
    const std::string outPath = args.get("out");
    const std::string lutPath = args.get("lut");
    if (lutPath.empty()) return fail("video requires --lut <file.cube>");
    if (inPath.empty()) return fail("video requires --in <clip>");
    if (outPath.empty()) return fail("video requires --out <clip>");

    sr::Lut3D lut;
    std::string error;
    if (!lut.loadCube(lutPath, error)) return fail(error);

    const std::string ffmpeg = ffmpegPath(args);
    const std::string ffprobe = siblingTool(ffmpeg);
    const std::string probe =
        runCapture(shellQuote(ffprobe) +
                   " -v error -select_streams v:0 -show_entries stream=width,height,r_frame_rate"
                   " -of csv=p=0 " +
                   shellQuote(inPath));
    int width = 0;
    int height = 0;
    double fps = 0.0;
    {
        std::istringstream stream(probe);
        std::string token;
        std::vector<std::string> parts;
        while (std::getline(stream, token, ',')) parts.push_back(token);
        if (parts.size() < 3) {
            return fail("could not probe " + inPath + " (is ffprobe next to ffmpeg?)");
        }
        width = std::atoi(parts[0].c_str());
        height = std::atoi(parts[1].c_str());
        const auto slash = parts[2].find('/');
        if (slash != std::string::npos) {
            const double num = std::atof(parts[2].substr(0, slash).c_str());
            const double den = std::atof(parts[2].substr(slash + 1).c_str());
            fps = den != 0.0 ? num / den : 0.0;
        }
    }
    if (width <= 0 || height <= 0 || fps <= 0.0) return fail("bad video geometry");

    // ffmpeg autorotates while decoding, so a 90/270 display matrix means the
    // frames arrive with width and height swapped.
    const std::string rotation = runCapture(
        shellQuote(ffprobe) +
        " -v error -select_streams v:0 -show_entries side_data=rotation"
        " -of default=noprint_wrappers=1:nokey=1 " +
        shellQuote(inPath));
    const int degrees = std::atoi(rotation.c_str());
    if (degrees == 90 || degrees == -90 || degrees == 270 || degrees == -270) {
        std::swap(width, height);
    }

    if (width % 2 != 0 || height % 2 != 0) {
        return fail("video needs even width and height for 4:2:0 output");
    }

    const std::string trc = args.get("trc", "srgb");
    const bool hdr = trc == "pq" || trc == "hlg";
    const std::string inMatrix = args.get("in-matrix", "bt2020");

    // Time range (D031): seconds or HH:MM:SS.mmm, plus an optional frame count.
    const double start = parseSeconds(args.get("start", "0"));
    double framesWanted = 0.0;
    if (args.has("frames")) {
        framesWanted = std::atof(args.get("frames").c_str());
    } else if (args.has("duration")) {
        framesWanted = std::floor(parseSeconds(args.get("duration")) * fps + 0.5);
    }

    std::string seekDecode;
    std::string seekAudio;
    if (start > 0.0) {
        const std::string s = formatSeconds(start);
        seekDecode = " -ss " + s;
        seekAudio = " -ss " + s;
    }
    std::string limitDecode;
    std::string limitAudio;
    if (framesWanted >= 1.0) {
        const std::string dur = formatSeconds(framesWanted / fps);
        limitDecode = " -frames:v " + std::to_string(static_cast<long>(framesWanted));
        limitAudio = " -t " + dur;
    }

    const std::string decodeCmd =
        shellQuote(ffmpeg) + " -v error" + seekDecode + " -i " + shellQuote(inPath) +
        limitDecode + " -vf " +
        shellQuote("scale=in_color_matrix=" + inMatrix + ":in_range=tv:out_range=full") +
        " -pix_fmt rgb48le -f rawvideo -";

    // Codec: HEVC for HDR, H.264 for SDR, or ProRes 422 HQ on request.
    std::string codec = args.get("codec", hdr ? "hevc" : "h264");
    if (args.has("prores")) codec = "prores";
    if (codec != "hevc" && codec != "h264" && codec != "prores") {
        return fail("--codec expects 'hevc', 'h264' or 'prores'");
    }

    const bool prores = codec == "prores";
    const std::string outFormat =
        prores ? "yuv422p10le" : (hdr ? "yuv420p10le" : "yuv420p");
    const std::string matrix = hdr ? "bt2020nc" : "bt709";
    const std::string transfer = hdr ? (trc == "pq" ? "smpte2084" : "arib-std-b67") : "bt709";
    const std::string primaries = hdr ? "bt2020" : "bt709";

    std::string codecArgs;
    if (prores) {
        codecArgs = " -c:v prores_ks -profile:v 3 -vendor apl0";
    } else if (hdr) {
        codecArgs = " -c:v libx265 -crf 16 -x265-params " +
                    shellQuote("colorprim=bt2020:transfer=" + transfer +
                               ":colormatrix=bt2020nc:master-display=G(13250,34500)B(7500,3000)"
                               "R(34000,16000)WP(15635,16450)L(10000000,1):max-cll=1000,400");
    } else {
        codecArgs = " -c:v libx264 -crf 16 -preset medium";
    }

    const std::string encodeCmd =
        shellQuote(ffmpeg) + " -v error -y -f rawvideo -pix_fmt rgb48le -s " +
        std::to_string(width) + "x" + std::to_string(height) + " -r " +
        std::to_string(fps) + " -i -" + seekAudio + limitAudio + " -i " + shellQuote(inPath) +
        " -map 0:v:0 -map 1:a:0? -c:a copy -vf " +
        shellQuote("scale=out_color_matrix=" + matrix + ":out_range=tv,format=" + outFormat) +
        codecArgs + " -color_primaries " + primaries + " -color_trc " + transfer +
        " -colorspace " + matrix + " -movflags +faststart " + shellQuote(outPath);

    std::printf("input:  %s (%dx%d @ %.3f fps)\n", inPath.c_str(), width, height, fps);
    if (start > 0.0 || framesWanted >= 1.0) {
        std::printf("range:  start %.3f s, %s\n", start,
                    framesWanted >= 1.0
                        ? (std::to_string(static_cast<long>(framesWanted)) + " frames").c_str()
                        : "to the end");
    }
    const char* codecName = prores ? "ProRes 422 HQ"
                                   : (codec == "hevc" ? "HEVC Main10" : "H.264");
    std::printf("output: %s (%s, %s)\n", outPath.c_str(), codecName,
                hdr ? (trc == "pq" ? "PQ" : "HLG") : "SDR");

    FILE* decoder = openPipe(decodeCmd, "rb");
    if (!decoder) return fail("failed to start ffmpeg decoder");
    FILE* encoder = openPipe(encodeCmd, "wb");
    if (!encoder) {
        closePipe(decoder);
        return fail("failed to start ffmpeg encoder");
    }

    const std::string interpName = args.get("interp", "tetra");
    const sr::Interp mode =
        interpName == "tri" ? sr::Interp::Trilinear : sr::Interp::Tetrahedral;
    const float strength = static_cast<float>(std::atof(args.get("strength", "1.0").c_str()));

    const size_t pixels = static_cast<size_t>(width) * height;
    const int threads = sr::resolveThreadCount(args.getInt("threads", 0));
    const TextureOptions texture = parseTexture(args);
    std::vector<unsigned short> raw(pixels * 3);
    std::vector<float> frame(pixels * 3);
    const auto started = std::chrono::steady_clock::now();
    size_t frames = 0;
    while (true) {
        const size_t got = std::fread(raw.data(), sizeof(unsigned short), raw.size(), decoder);
        if (got < raw.size()) break;
        for (size_t i = 0; i < raw.size(); ++i) frame[i] = raw[i] / 65535.0f;
        sr::parallelChunks(pixels, threads, [&](size_t begin, size_t end) {
            for (size_t i = begin; i < end; ++i) {
                float* px = &frame[i * 3];
                const sr::Rgb src{px[0], px[1], px[2]};
                const sr::Rgb mapped = lut.apply(src, mode);
                const sr::Rgb result = sr::mix(src, mapped, strength);
                px[0] = result.r;
                px[1] = result.g;
                px[2] = result.b;
            }
        });
        applyTexture(frame.data(), width, height, texture, static_cast<int>(frames));
        for (size_t i = 0; i < raw.size(); ++i) {
            const float value = std::min(1.0f, std::max(0.0f, frame[i]));
            raw[i] = static_cast<unsigned short>(std::lround(value * 65535.0f));
        }
        if (std::fwrite(raw.data(), sizeof(unsigned short), raw.size(), encoder) != raw.size()) {
            closePipe(decoder);
            closePipe(encoder);
            return fail("encoder closed the pipe early");
        }
        ++frames;
        if (frames % 24 == 0) {
            std::printf("  %zu frames...\n", frames);
            std::fflush(stdout);
        }
    }
    closePipe(decoder);
    closePipe(encoder);

    if (frames == 0) return fail("no frames were decoded");
    const auto elapsed = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - started)
                             .count();
    std::printf("wrote %s: %zu frames, %.1f ms/frame, %.2f s total\n", outPath.c_str(), frames,
                elapsed / frames, elapsed / 1000.0);
    return 0;
}

int cmdConvert(const Args& args) {    const std::string inPath = args.get("in");
    const std::string outPath = args.get("out");
    if (inPath.empty()) return fail("convert requires --in <image>");
    if (outPath.empty()) return fail("convert requires --out <image>");

    sr::InputTransfer from = sr::InputTransfer::Srgb;
    if (args.has("from") && !sr::parseInputTransfer(args.get("from"), from)) {
        return fail("--from expects 'srgb' or 'apple-log'");
    }
    if (args.has("to") && args.get("to") != "srgb") {
        return fail("--to currently supports 'srgb' only (HDR output is M2 work)");
    }
    const double exposureEv = args.getDouble("exposure", 0.0);

    sr::Image image;
    int sourceBits = 8;
    std::string error;
    if (!sr::loadImage(inPath, image, error, &sourceBits)) return fail(error);
    const int bits = requestedBits(args, sourceBits);

    const auto started = std::chrono::steady_clock::now();
    const size_t count = static_cast<size_t>(image.width) * image.height;
    const double gain = std::exp2(exposureEv);
    for (size_t i = 0; i < count; ++i) {
        float* px = &image.pixels[i * 3];
        double linear[3];
        if (from == sr::InputTransfer::AppleLog) {
            const double code[3] = {px[0], px[1], px[2]};
            double lin2020[3];
            for (int c = 0; c < 3; ++c) lin2020[c] = sr::appleLogDecode(code[c]);
            sr::bt2020ToSrgb(lin2020, linear);
            for (int c = 0; c < 3; ++c) linear[c] = std::max(0.0, linear[c]);
        } else {
            for (int c = 0; c < 3; ++c) linear[c] = sr::srgbDecode(px[c]);
        }
        for (int c = 0; c < 3; ++c) {
            px[c] = static_cast<float>(sr::srgbEncode(std::min(1.0, std::max(0.0, linear[c] * gain))));
        }
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - started)
                             .count();

    if (!sr::saveImage(outPath, image, error, bits)) return fail(error);

    std::printf("converted %s (%s -> srgb, %+.2f EV) to %s\n", inPath.c_str(),
                sr::inputTransferName(from), exposureEv, outPath.c_str());
    std::printf("wrote %s (%dx%d, %zu pixels, %d-bit out from %d-bit in, %.1f ms)\n",
                outPath.c_str(), image.width, image.height, count, bits, sourceBits, elapsed);
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
        std::printf("spektra-roll-cli %s\n", kVersion);
        return 0;
    }
    if (command == "help" || args.has("help")) {
        printUsage();
        return 0;
    }
    if (command == "gen") return cmdGen(args);
    if (command == "convert") return cmdConvert(args);
    if (command == "video") return cmdVideo(args);

    if (command == "lut" && args.positional.size() >= 2) {
        const std::string& sub = args.positional[1];
        if (sub == "info") return cmdLutInfo(args);
        if (sub == "gen") return cmdLutGen(args);
        if (sub == "bake") return cmdLutBake(args);
        if (sub == "apply") return cmdLutApply(args);
        if (sub == "compare") return cmdLutCompare(args);
        return fail("unknown lut subcommand: " + sub);
    }

    std::fprintf(stderr, "error: unknown command: %s\n\n", command.c_str());
    printUsage();
    return 1;
}
