// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <sys/wait.h>
#endif

#include "sr/film_model.hpp"
#include "sr/image.hpp"
#include "sr/lut3d.hpp"
#include "sr/metrics.hpp"
#include "sr/parallel.hpp"
#include "sr/profile.hpp"
#include "sr/texture.hpp"

#include "gpu_film.hpp"
#include "gpu_texture.hpp"

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
        "                         [--engine lut|spectral|spectral-gpu] [--input srgb|apple-log]\n"
        "                         [--film <json>] [--paper <json>]\n"
        "                         [--trc srgb|pq|hlg] [--in-matrix bt2020|bt709]\n"
        "                         [--start 0] [--frames N | --duration T]\n"
        "                         [--codec hevc|h264|prores]\n"
        "                         [--crf 16] [--preset medium] [--encoder sw|qsv]\n"
        "                         [--interp tetra|tri] [--strength 0..1] [--threads N]\n"
        "                         [--grain 0..1] [--halation 0..1] [--seed N]\n"
        "                         [--grain-rate 3] [--glare 0..1] [--diffusion 0..1]\n"
        "                         [--segment SEC] [--resume] [--keep-parts]\n"
        "                         [--timecode HH:MM:SS:FF] [--no-timecode]\n"
        "                         [--texture-engine auto|cpu|gpu] [--ffmpeg <path>]\n"
        "      Decode a clip with ffmpeg, apply the LUT frame by frame and re-encode\n"
        "      with the original audio. --start/--frames pick a range (seconds or\n"
        "      HH:MM:SS). HDR output is HEVC Main10 (or ProRes 422 HQ) with PQ/HLG\n"
        "      tags. --segment renders in chunks under <out>.parts and --resume\n"
        "      skips the chunks that already finished, so an interrupted long\n"
        "      render continues where it stopped. The source timecode is copied\n"
        "      to the output unless --no-timecode is given. With\n"
        "      --engine spectral the LUT is bypassed and every pixel runs the\n"
        "      physical film/paper chain (slow but physical; --film/--paper and\n"
        "      --input choose the materials). --engine spectral-gpu runs the same\n"
        "      chain on the D3D11 GPU and falls back to the CPU engine when no\n"
        "      device is available. --texture-engine gpu moves grain, halation,\n"
        "      glare and diffusion onto the same GPU (D047); the default 'auto'\n"
        "      does this whenever the colour engine is spectral-gpu.\n"
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
        "                             [--engine lut|spectral|spectral-gpu] [--input srgb|apple-log]\n"
        "                             [--film <json>] [--paper <json>]\n"
        "                             [--interp tetra|tri] [--strength 0..1] [--bits 8|16]\n"
        "                             [--grain 0..1] [--halation 0..1]\n"
        "                             [--grain-rate 3] [--glare 0..1] [--diffusion 0..1]\n"
        "                             [--texture-engine auto|cpu|gpu]\n"
        "      Apply a 3D LUT to an image. Output bit depth defaults to the input's.\n"
        "      --engine spectral runs the per-pixel physical chain instead of a LUT\n"
        "      (no --lut needed; --film/--paper/--input choose the materials).\n"
        "\n"
        "  spektra-roll-cli lut apply --lut <file.cube> --in-dir <dir> --out-dir <dir>\n"
        "                             [--interp tetra|tri] [--strength 0..1] [--bits 8|16]\n"
        "                             [--texture-engine auto|cpu|gpu]\n"
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

struct FilmRecipe {
    sr::Profile film;
    sr::Profile paper;
    sr::FilmModel model;
    sr::BakeOptions options;
    sr::Triple filters{};
    bool couplers = false;
};

// Shared by `lut bake`, the spectral image path and the spectral video path:
// resolves profiles, neutral filters and every BakeOptions flag from the CLI.
bool configureFilmRecipe(const Args& args, FilmRecipe& recipe, std::string& error) {
    const std::string filmPath = args.get("film", kDefaultFilm);
    const std::string paperPath = args.get("paper", kDefaultPaper);
    const std::string filtersPath = args.get("filters", kDefaultFilters);

    if (!sr::loadProfile(filmPath, recipe.film, error)) return false;
    if (!sr::loadProfile(paperPath, recipe.paper, error)) return false;

    if (args.has("cc")) {
        if (!parseTriple(args.get("cc"), recipe.filters)) {
            error = "--cc expects three numbers, e.g. --cc 0,51.57,52.53";
            return false;
        }
    } else if (!sr::loadNeutralFilters(filtersPath, recipe.paper.info.stock,
                                       recipe.film.info.stock, "TH-KG3", recipe.filters,
                                       error)) {
        error += " (pass --cc C,M,Y to override)";
        return false;
    }

    if (args.has("input")) {
        if (!sr::parseInputTransfer(args.get("input"), recipe.options.input)) {
            error = "--input expects 'srgb' or 'apple-log'";
            return false;
        }
    }
    recipe.options.exposureEv = args.getDouble("exposure", 0.0);
    recipe.options.threads = args.getInt("threads", 0);

    if (args.has("scan")) {
        const std::string mode = args.get("scan");
        if (mode == "negative" || mode == "film") {
            recipe.options.scan = sr::ScanMode::Negative;
        } else if (mode != "print") {
            error = "--scan expects 'print' or 'negative'";
            return false;
        }
    }
    recipe.options.contrast = args.getDouble(
        "contrast", recipe.options.scan == sr::ScanMode::Negative ? 1.15 : 1.0);

    recipe.options.gamutCompress = recipe.options.input == sr::InputTransfer::AppleLog;
    if (args.has("gamut")) {
        const std::string mode = args.get("gamut");
        if (mode == "oklch") {
            recipe.options.gamutCompress = true;
        } else if (mode == "off") {
            recipe.options.gamutCompress = false;
        } else {
            error = "--gamut expects 'off' or 'oklch'";
            return false;
        }
    }

    bool couplers = recipe.options.input == sr::InputTransfer::AppleLog;
    if (args.has("couplers")) {
        const std::string value = args.get("couplers");
        if (value == "on") {
            couplers = true;
        } else if (value == "off") {
            couplers = false;
        } else {
            error = "--couplers expects 'on' or 'off'";
            return false;
        }
    }

    sr::FilmModelOptions modelOptions;
    modelOptions.couplers = couplers;
    recipe.couplers = couplers;
    if (!recipe.model.configure(recipe.film, recipe.paper, recipe.filters, error,
                                modelOptions)) {
        return false;
    }

    if (args.has("hdr")) {
        const std::string value = args.get("hdr");
        if (value == "pq") {
            recipe.options.hdr = sr::HdrMode::Pq;
        } else if (value == "hlg") {
            recipe.options.hdr = sr::HdrMode::Hlg;
        } else if (value != "off") {
            error = "--hdr expects 'pq', 'hlg' or 'off'";
            return false;
        }
    }
    recipe.options.peakNits = args.getDouble("peak", 1000.0);
    return true;
}

int cmdLutBake(const Args& args) {
    const std::string out = args.get("out", "out/portra400_endura_33.cube");
    const int size = std::max(2, args.getInt("size", 33));

    FilmRecipe recipe;
    std::string error;
    if (!configureFilmRecipe(args, recipe, error)) return fail(error);
    const sr::Profile& film = recipe.film;
    const sr::Profile& paper = recipe.paper;
    const sr::BakeOptions& options = recipe.options;

    std::printf("film:              %s (%s)\n", film.info.name.c_str(), film.info.stock.c_str());
    std::printf("paper:             %s (%s)\n", paper.info.name.c_str(), paper.info.stock.c_str());
    std::printf("neutral filters:   C=%.3f M=%.3f Y=%.3f\n", recipe.filters[0],
                recipe.filters[1], recipe.filters[2]);
    std::printf("print exposure:    %.12f\n", recipe.model.printExposureFactor());
    std::printf("input:             %s", sr::inputTransferName(options.input));
    if (options.exposureEv != 0.0) std::printf(" (%+.2f EV)", options.exposureEv);
    std::printf("\n");
    std::printf("scan:              %s\n",
                options.scan == sr::ScanMode::Negative ? "negative (no paper)" : "print");
    if (options.contrast != 1.0) std::printf("contrast:          %.2f\n", options.contrast);
    std::printf("gamut:             %s\n", options.gamutCompress ? "oklch" : "off");
    std::printf("couplers:          %s\n", recipe.couplers ? "on" : "off");
    if (options.hdr != sr::HdrMode::Off) {
        std::printf("hdr:               %s, white at %.0f nits\n",
                    options.hdr == sr::HdrMode::Pq ? "pq" : "hlg", options.peakNits);
    }

    const auto started = std::chrono::steady_clock::now();
    const std::string title =
        args.get("title", "SpektraRoll " + film.info.stock + " + " + paper.info.stock);
    const sr::Lut3D lut = recipe.model.bake(size, title, options);
    if (lut.empty()) return fail("failed to bake the LUT");

    if (!lut.saveCube(out, error)) return fail(error);
    const auto elapsed =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();

    const sr::Rgb mid = recipe.model.transformFrom(options, {0.5f, 0.5f, 0.5f});
    const sr::Rgb black = recipe.model.transformFrom(options, {0.0f, 0.0f, 0.0f});
    const sr::Rgb white = recipe.model.transformFrom(options, {1.0f, 1.0f, 1.0f});

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
sr::TextureParams parseTexture(const Args& args) {
    sr::TextureParams texture;
    texture.grain.amount = static_cast<float>(args.getDouble("grain", 0.0));
    texture.grain.size = static_cast<float>(args.getDouble("grain-size", 1.4));
    texture.grain.seed = static_cast<unsigned>(args.getInt("seed", 1));
    texture.grain.rate = std::max(1, args.getInt("grain-rate", 3));
    texture.halation.amount = static_cast<float>(args.getDouble("halation", 0.0));
    texture.halation.radius = static_cast<float>(args.getDouble("halation-radius", 0.006));
    texture.halation.threshold = static_cast<float>(args.getDouble("halation-threshold", 0.72));
    texture.glare.amount = static_cast<float>(args.getDouble("glare", 0.0));
    texture.glare.radius = static_cast<float>(args.getDouble("glare-radius", 0.02));
    texture.diffusion.amount = static_cast<float>(args.getDouble("diffusion", 0.0));
    texture.diffusion.radius = static_cast<float>(args.getDouble("diffusion-radius", 0.004));
    return texture;
}

// `auto` uses the GPU whenever the colour engine is already on the GPU (D047);
// `cpu` keeps the reference chain, `gpu` asks for it explicitly. A device
// failure is never fatal: the caller falls back and says so once.
sr::GpuTexture* prepareTextureGpu(const Args& args, const std::string& engine,
                                  sr::GpuTexture& storage) {
    std::string mode = args.get("texture-engine", "auto");
    if (mode != "auto" && mode != "cpu" && mode != "gpu") {
        std::fprintf(stderr, "warning: --texture-engine expects 'auto', 'cpu' or 'gpu'\n");
        mode = "auto";
    }
    const bool wanted = mode == "gpu" || (mode == "auto" && engine == "spectral-gpu");
    if (!wanted) return nullptr;
    std::string error;
    if (!storage.prepare(error)) {
        std::fprintf(stderr, "warning: %s; keeping the CPU texture chain\n", error.c_str());
        return nullptr;
    }
    return &storage;
}

// Applies the chain on the GPU when one is set up; on any device failure it
// warns once, drops the backend and finishes the sequence on the CPU.
void applyTextureFrame(float* pixels, int width, int height, const sr::TextureParams& texture,
                       int frame, sr::GrainFieldCache* grainCache, sr::GpuTexture** gpu) {
    if (*gpu != nullptr) {
        std::string error;
        if ((*gpu)->process(pixels, width, height, texture, frame, pixels, error)) return;
        std::fprintf(stderr, "warning: %s; falling back to the CPU texture chain\n",
                     error.c_str());
        *gpu = nullptr;
    }
    sr::applyTexture(pixels, width, height, texture, frame, grainCache);
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
    const sr::TextureParams texture = parseTexture(args);
    sr::GrainFieldCache grainCache;
    sr::GpuTexture textureGpuStorage;
    sr::GpuTexture* textureGpu = prepareTextureGpu(args, "lut", textureGpuStorage);
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
        applyTextureFrame(image.pixels.data(), image.width, image.height, texture,
                          static_cast<int>(frames), &grainCache, &textureGpu);
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
    const std::string engine = args.get("engine", "lut");
    if (engine != "lut" && engine != "spectral" && engine != "spectral-gpu") {
        return fail("--engine expects 'lut', 'spectral' or 'spectral-gpu'");
    }
    bool gpu = engine == "spectral-gpu";
    const bool spectral = engine == "spectral" || gpu;

    const std::string lutPath = args.get("lut");
    sr::Lut3D lut;
    FilmRecipe recipe;
    sr::GpuFilm gpuFilm;
    std::string error;
    if (!spectral) {
        if (lutPath.empty()) return fail("lut apply requires --lut <file.cube>");
        if (!lut.loadCube(lutPath, error)) return fail(error);
    } else {
        if (!configureFilmRecipe(args, recipe, error)) return fail(error);
        if (gpu) gpu = gpuFilm.configure(recipe.model, recipe.options, error);
        if (!gpu && engine == "spectral-gpu") {
            std::fprintf(stderr, "warning: %s; falling back to the CPU spectral engine\n",
                         error.c_str());
        }
    }

    const std::string interpName = args.get("interp", "tetra");
    const sr::Interp mode =
        interpName == "tri" ? sr::Interp::Trilinear : sr::Interp::Tetrahedral;
    const float strength = static_cast<float>(std::atof(args.get("strength", "1.0").c_str()));

    if (args.has("in-dir") || args.has("out-dir")) {
        if (spectral) return fail("--engine spectral currently handles a single image only");
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
    const auto applyCpu = [&]() {
        sr::parallelChunks(count, threads, [&](size_t begin, size_t end) {
            for (size_t i = begin; i < end; ++i) {
                float* px = &image.pixels[i * 3];
                const sr::Rgb src{px[0], px[1], px[2]};
                const sr::Rgb mapped =
                    spectral ? recipe.model.transformFrom(recipe.options, src)
                             : lut.apply(src, mode);
                const sr::Rgb result = sr::mix(src, mapped, strength);
                px[0] = result.r;
                px[1] = result.g;
                px[2] = result.b;
            }
        });
    };
    if (gpu && !gpuFilm.process(image.pixels.data(), count, image.pixels.data(), strength,
                               error)) {
        std::fprintf(stderr, "warning: %s; falling back to the CPU spectral engine\n",
                     error.c_str());
        gpu = false;
    }
    if (!gpu) applyCpu();
    sr::GpuTexture textureGpuStorage;
    sr::GpuTexture* textureGpu = prepareTextureGpu(args, engine, textureGpuStorage);
    applyTextureFrame(image.pixels.data(), image.width, image.height, parseTexture(args), 0,
                      nullptr, &textureGpu);
    const auto elapsed = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - started)
                             .count();

    if (!sr::saveImage(outPath, image, error, bits)) return fail(error);

    if (spectral) {
        std::printf("engine: %s %s + %s\n", gpu ? "spectral-gpu" : "spectral",
                    recipe.film.info.name.c_str(), recipe.paper.info.name.c_str());
        std::printf("wrote %s (%dx%d, %zu pixels, %d-bit out from %d-bit in, %.1f ms)\n",
                    outPath.c_str(), image.width, image.height, count, bits, sourceBits,
                    elapsed);
    } else {
        std::printf("applied %s (%d^3) to %s\n", lutPath.c_str(), lut.size(), inPath.c_str());
        std::printf("wrote %s (%dx%d, %zu pixels, %d-bit out from %d-bit in, %.1f ms, %s)\n",
                    outPath.c_str(), image.width, image.height, count, bits, sourceBits,
                    elapsed,
                    mode == sr::Interp::Trilinear ? "trilinear" : "tetrahedral");
    }
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

std::string formatNumber(double value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%g", value);
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

// ffprobe 7+ prints a stream twice when a timecode track creates a stream group
// (once under "stream_groups", once under "streams"). The text output is
// therefore duplicated; all our probes only need the first line.
std::string firstLine(const std::string& text) {
    const auto end = text.find('\n');
    std::string line = end == std::string::npos ? text : text.substr(0, end);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    return line;
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

// ---- timecode ------------------------------------------------------------
// Cameras write a start timecode into the clip. Copying it to the render keeps
// the result lined up with the camera report and with proxies made elsewhere.
struct Timecode {
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    int frames = 0;
    bool dropFrame = false;
    bool valid = false;
};

Timecode parseTimecode(const std::string& text) {
    Timecode tc;
    if (text.empty()) return tc;
    const char separator = text.find(';') != std::string::npos ? ';' : ':';
    std::istringstream stream(text);
    std::string part;
    int values[4] = {0, 0, 0, 0};
    int count = 0;
    while (count < 4 && std::getline(stream, part, separator)) {
        values[count++] = std::atoi(part.c_str());
    }
    if (count != 4) return tc;
    tc.hours = values[0];
    tc.minutes = values[1];
    tc.seconds = values[2];
    tc.frames = values[3];
    if (tc.hours < 0 || tc.minutes < 0 || tc.minutes > 59 || tc.seconds < 0 ||
        tc.seconds > 59 || tc.frames < 0 || tc.frames > 59 || tc.hours > 99) {
        return tc;
    }
    tc.dropFrame = separator == ';';
    tc.valid = true;
    return tc;
}

std::string formatTimecode(const Timecode& tc) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d%c%02d", tc.hours, tc.minutes,
                  tc.seconds, tc.dropFrame ? ';' : ':', tc.frames);
    return buffer;
}

// Non-drop counting. Drop-frame sources keep their separator and are shipped as
// the offset in nominal frames; the drift is a few frames per hour.
Timecode addFrames(Timecode tc, long offset, double fps) {
    if (!tc.valid || offset == 0) return tc;
    const int nominal = std::max(1, static_cast<int>(std::lround(fps)));
    long total = (static_cast<long>(tc.hours) * 3600 + tc.minutes * 60 + tc.seconds) * nominal +
                 tc.frames + offset;
    const long perDay = 24L * 3600 * nominal;
    total %= perDay;
    if (total < 0) total += perDay;
    tc.hours = static_cast<int>(total / (3600L * nominal));
    total %= 3600L * nominal;
    tc.minutes = static_cast<int>(total / (60L * nominal));
    total %= 60L * nominal;
    tc.seconds = static_cast<int>(total / nominal);
    tc.frames = static_cast<int>(total % nominal);
    return tc;
}

std::string probeTimecode(const std::string& ffprobe, const std::string& input) {
    const std::string text =
        runCapture(shellQuote(ffprobe) +
                   " -v error -show_entries format_tags=timecode:stream_tags=timecode"
                   " -of default=noprint_wrappers=1:nokey=1 " +
                   shellQuote(input));
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.find(':') != std::string::npos || line.find(';') != std::string::npos) return line;
    }
    return {};
}

// Frame count of the video stream; falls back to duration x fps when the
// container does not count frames.
long probeFrameCount(const std::string& ffprobe, const std::string& input, double fps) {
    const std::string text = firstLine(runCapture(
        shellQuote(ffprobe) +
        " -v error -select_streams v:0 -show_entries stream=nb_frames,duration"
        " -of csv=p=0 " +
        shellQuote(input)));
    std::istringstream stream(text);
    std::string token;
    std::vector<std::string> parts;
    while (std::getline(stream, token, ',')) parts.push_back(token);
    // ffprobe prints fields in declaration order, not request order: a value
    // with a dot is the duration, a plain integer is nb_frames.
    long frames = 0;
    double duration = 0.0;
    for (const std::string& part : parts) {
        if (part.find('.') != std::string::npos) {
            duration = std::atof(part.c_str());
        } else {
            const long value = std::atol(part.c_str());
            if (value > 0) frames = value;
        }
    }
    if (frames <= 0 && duration > 0.0) frames = static_cast<long>(std::lround(duration * fps));
    return frames;
}

int runCommand(const std::string& command) {
    FILE* pipe = openPipe(command, "r");
    if (!pipe) return -1;
    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), pipe)) std::fputs(buffer, stderr);
#ifdef _WIN32
    return _pclose(pipe);
#else
    const int status = pclose(pipe);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

std::string zeroPad(long value, int digits) {
    std::string text = std::to_string(value);
    if (static_cast<int>(text.size()) < digits) {
        text.insert(0, static_cast<size_t>(digits) - text.size(), '0');
    }
    return text;
}

// ---- video rendering -----------------------------------------------------

struct VideoRender {
    std::string ffmpeg;
    std::string inPath;
    int width = 0;
    int height = 0;
    double fps = 24.0;
    const sr::Lut3D* lut = nullptr;
    // Set when `--engine spectral`: the per-pixel physical chain replaces the
    // LUT. Kept as a pointer because FilmModel is not copyable and lives in
    // cmdVideo for the whole render.
    const sr::FilmModel* model = nullptr;
    sr::BakeOptions bake;
    bool spectral = false;
    sr::GpuFilm* gpu = nullptr;
    sr::Interp interp = sr::Interp::Tetrahedral;
    float strength = 1.0f;
    int threads = 1;
    sr::TextureParams texture;
    // D047: optional D3D11 texture chain. Set to nullptr to force the CPU path.
    sr::GpuTexture* textureGpu = nullptr;
    std::string codecArgs;
    std::string outFormat;
    std::string inMatrix;
    std::string matrix;
    std::string transfer;
    std::string primaries;
};

// Content hash of a small file (LUTs are ~130 KB), used to notice that the
// recipe changed since the parts were rendered.
std::string fileHash(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return "missing";
    std::uint64_t hash = 1469598103934665603ULL;
    char byte = 0;
    while (input.get(byte)) {
        hash ^= static_cast<unsigned char>(byte);
        hash *= 1099511628211ULL;
    }
    std::ostringstream text;
    text << std::hex << std::setw(16) << std::setfill('0') << hash;
    return text.str();
}

// Everything that would make an old part incompatible with the new render.
// Resuming across a codec, LUT or texture change would silently mix looks.
std::string renderSignature(const VideoRender& job, const std::string& lutPath, long startFrame,
                            long totalFrames, long segmentFrames) {
    std::ostringstream signature;
    signature << std::setprecision(9) << "spektra-roll video v1\n"
              << "input " << job.inPath << "\n"
              << "size " << job.width << "x" << job.height << " fps " << job.fps << "\n"
              << "lut " << fileHash(lutPath) << "\n"
              << "start " << startFrame << " total " << totalFrames << " segment "
              << segmentFrames << "\n"
              << "encode " << job.codecArgs << "\n"
              << "format " << job.outFormat << " " << job.matrix << " " << job.transfer << " "
              << job.primaries << "\n"
              << "texture " << job.texture.grain.amount << " " << job.texture.grain.size << " "
              << job.texture.grain.seed << " " << job.texture.grain.rate << " "
              << job.texture.halation.amount << " " << job.texture.halation.radius << " "
              << job.texture.halation.threshold << " " << job.texture.glare.amount << " "
              << job.texture.glare.radius << " " << job.texture.diffusion.amount << " "
              << job.texture.diffusion.radius << "\n"
              << "texture-engine " << (job.textureGpu != nullptr ? "gpu" : "cpu") << "\n";
    return signature.str();
}

// Renders `frameCount` frames from source frame `startFrame` into one video-only
// file. Audio is muxed once from the source at the end, so a segment never
// re-encodes audio and A/V sync cannot drift between parts. `rangeFrameBase` is
// the frame index within the requested range and keeps grain continuous across
// segment boundaries.
bool renderRange(VideoRender& job, long startFrame, long frameCount, long rangeFrameBase,
                 const std::string& outPath, const std::string& timecode,
                 sr::GrainFieldCache* grainCache, long rangeTotal, size_t* rendered,
                 std::string& error) {
    const double start = static_cast<double>(startFrame) / job.fps;
    const std::string seek = start > 0.0 ? " -ss " + formatSeconds(start) : "";
    const std::string limit = frameCount > 0 ? " -frames:v " + std::to_string(frameCount) : "";
    const std::string decodeCmd =
        shellQuote(job.ffmpeg) + " -v error" + seek + " -i " + shellQuote(job.inPath) + limit +
        " -vf " +
        shellQuote("scale=in_color_matrix=" + job.inMatrix + ":in_range=tv:out_range=full") +
        " -pix_fmt rgb48le -f rawvideo -";

    const std::string encodeCmd =
        shellQuote(job.ffmpeg) + " -v error -y -f rawvideo -pix_fmt rgb48le -s " +
        std::to_string(job.width) + "x" + std::to_string(job.height) + " -r " +
        std::to_string(job.fps) + " -i - -an -vf " +
        shellQuote("scale=out_color_matrix=" + job.matrix + ":out_range=tv,format=" +
                   job.outFormat) +
        job.codecArgs + " -color_primaries " + job.primaries + " -color_trc " + job.transfer +
        " -colorspace " + job.matrix +
        (timecode.empty() ? "" : " -timecode " + timecode) + " " + shellQuote(outPath);

    FILE* decoder = openPipe(decodeCmd, "rb");
    if (!decoder) {
        error = "failed to start ffmpeg decoder";
        return false;
    }
    FILE* encoder = openPipe(encodeCmd, "wb");
    if (!encoder) {
        closePipe(decoder);
        error = "failed to start ffmpeg encoder";
        return false;
    }

    const size_t pixels = static_cast<size_t>(job.width) * job.height;
    std::vector<unsigned short> raw(pixels * 3);
    std::vector<float> frame(pixels * 3);
    size_t frames = 0;
    bool writeError = false;
    while (frameCount <= 0 || frames < static_cast<size_t>(frameCount)) {
        const size_t got = std::fread(raw.data(), sizeof(unsigned short), raw.size(), decoder);
        if (got < raw.size()) break;
        for (size_t i = 0; i < raw.size(); ++i) frame[i] = raw[i] / 65535.0f;
        if (job.gpu != nullptr &&
            !job.gpu->process(frame.data(), pixels, frame.data(), job.strength, error)) {
            std::fprintf(stderr, "warning: %s; falling back to the CPU spectral engine\n",
                         error.c_str());
            job.gpu = nullptr;
        }
        if (job.gpu == nullptr) {
            sr::parallelChunks(pixels, job.threads, [&](size_t begin, size_t end) {
                for (size_t i = begin; i < end; ++i) {
                    float* px = &frame[i * 3];
                    const sr::Rgb src{px[0], px[1], px[2]};
                    const sr::Rgb mapped = job.spectral
                                               ? job.model->transformFrom(job.bake, src)
                                               : job.lut->apply(src, job.interp);
                    const sr::Rgb result = sr::mix(src, mapped, job.strength);
                    px[0] = result.r;
                    px[1] = result.g;
                    px[2] = result.b;
                }
            });
        }
        applyTextureFrame(frame.data(), job.width, job.height, job.texture,
                          static_cast<int>(rangeFrameBase + static_cast<long>(frames)), grainCache,
                          &job.textureGpu);
        for (size_t i = 0; i < raw.size(); ++i) {
            const float value = std::min(1.0f, std::max(0.0f, frame[i]));
            raw[i] = static_cast<unsigned short>(std::lround(value * 65535.0f));
        }
        if (std::fwrite(raw.data(), sizeof(unsigned short), raw.size(), encoder) != raw.size()) {
            writeError = true;
            break;
        }
        ++frames;
        if (frames % 24 == 0) {
            if (rangeTotal > 0) {
                std::printf("  %ld/%ld frames...\n",
                            rangeFrameBase + static_cast<long>(frames), rangeTotal);
            } else {
                std::printf("  %zu frames...\n", frames);
            }
            std::fflush(stdout);
        }
    }
    closePipe(decoder);
    closePipe(encoder);

    if (rendered != nullptr) *rendered = frames;
    if (writeError) {
        error = "encoder closed the pipe early";
        return false;
    }
    if (frames == 0) {
        error = "no frames were decoded";
        return false;
    }
    return true;
}

// Copies the audio once into the rendered video. The timecode was already
// stamped while encoding, so a stream copy carries it through (this also keeps
// new ffmpeg builds happy: -timecode is an encoder option, not a copy option).
bool finishVideo(const std::string& ffmpeg, const std::string& videoInput,
                 const std::string& inPath, long startFrame, long totalFrames, double fps,
                 const std::string& outPath, std::string& error) {
    const double start = static_cast<double>(startFrame) / fps;
    std::string command = shellQuote(ffmpeg) + " -v error -y" + videoInput;
    if (start > 0.0) command += " -ss " + formatSeconds(start);
    if (totalFrames > 0) command += " -t " + formatSeconds(static_cast<double>(totalFrames) / fps);
    command += " -i " + shellQuote(inPath) +
               " -map 0:v:0 -map 1:a:0? -c copy -movflags +faststart " + shellQuote(outPath);
    if (runCommand(command) != 0) {
        error = "ffmpeg could not mux the final file";
        return false;
    }
    return true;
}

int cmdVideo(const Args& args) {
    const std::string inPath = args.get("in");
    const std::string outPath = args.get("out");
    const std::string lutPath = args.get("lut");
    const std::string engine = args.get("engine", "lut");
    if (engine != "lut" && engine != "spectral" && engine != "spectral-gpu") {
        return fail("--engine expects 'lut', 'spectral' or 'spectral-gpu'");
    }
    bool gpu = engine == "spectral-gpu";
    const bool spectral = engine == "spectral" || gpu;
    if (!spectral && lutPath.empty()) return fail("video requires --lut <file.cube>");
    if (inPath.empty()) return fail("video requires --in <clip>");
    if (outPath.empty()) return fail("video requires --out <clip>");

    sr::Lut3D lut;
    FilmRecipe recipe;
    sr::GpuFilm gpuFilm;
    std::string error;
    if (spectral) {
        if (!configureFilmRecipe(args, recipe, error)) return fail(error);
        if (gpu) gpu = gpuFilm.configure(recipe.model, recipe.options, error);
        if (!gpu && engine == "spectral-gpu") {
            std::fprintf(stderr, "warning: %s; falling back to the CPU spectral engine\n",
                         error.c_str());
        }
    } else if (!lut.loadCube(lutPath, error)) {
        return fail(error);
    }

    const std::string ffmpeg = ffmpegPath(args);
    const std::string ffprobe = siblingTool(ffmpeg);
    const std::string probe = firstLine(
        runCapture(shellQuote(ffprobe) +
                   " -v error -select_streams v:0 -show_entries stream=width,height,r_frame_rate"
                   " -of csv=p=0 " +
                   shellQuote(inPath)));
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
    const std::string rotation = firstLine(runCapture(
        shellQuote(ffprobe) +
        " -v error -select_streams v:0 -show_entries side_data=rotation"
        " -of default=noprint_wrappers=1:nokey=1 " +
        shellQuote(inPath)));
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
    if (spectral) {
        // The LUT path bakes PQ/HLG into the table; spectral output encodes it
        // at the end of the chain, using the same --trc.
        recipe.options.hdr = trc == "pq" ? sr::HdrMode::Pq
                             : trc == "hlg" ? sr::HdrMode::Hlg
                                            : sr::HdrMode::Off;
        recipe.options.peakNits = args.getDouble("peak", 1000.0);
    }

    // Time range (D031) and segmentation (D040).
    const double start = parseSeconds(args.get("start", "0"));
    const long startFrame = std::max(0L, static_cast<long>(std::lround(start * fps)));
    double framesWanted = 0.0;
    if (args.has("frames")) {
        framesWanted = std::atof(args.get("frames").c_str());
    } else if (args.has("duration")) {
        framesWanted = std::floor(parseSeconds(args.get("duration")) * fps + 0.5);
    }
    const long probedFrames = probeFrameCount(ffprobe, inPath, fps);
    long totalFrames = 0;
    if (framesWanted >= 1.0) {
        totalFrames = static_cast<long>(std::llround(framesWanted));
    } else if (probedFrames > 0) {
        totalFrames = std::max(0L, probedFrames - startFrame);
    }

    // Timecode (D040): copy the source start unless overridden or disabled.
    std::string timecode;
    if (!args.has("no-timecode")) {
        timecode = args.get("timecode");
        if (timecode.empty()) timecode = probeTimecode(ffprobe, inPath);
    }
    Timecode base = parseTimecode(timecode);
    if (!timecode.empty() && !base.valid) {
        std::printf("warning: ignoring unreadable timecode '%s'\n", timecode.c_str());
    }

    // Codec: HEVC for HDR, H.264 for SDR, or ProRes 422 HQ on request.
    std::string codec = args.get("codec", hdr ? "hevc" : "h264");
    if (args.has("prores")) codec = "prores";
    if (codec != "hevc" && codec != "h264" && codec != "prores") {
        return fail("--codec expects 'hevc', 'h264' or 'prores'");
    }

    // Encoder: software (x264/x265) or Intel Quick Sync (D038).
    const std::string encoderMode = args.get("encoder", "sw");
    if (encoderMode != "sw" && encoderMode != "qsv") {
        return fail("--encoder expects 'sw' or 'qsv'");
    }
    const bool qsv = encoderMode == "qsv" && codec != "prores";

    const bool prores = codec == "prores";
    const std::string outFormat = prores ? "yuv422p10le"
                                         : (qsv ? (hdr ? "p010le" : "nv12")
                                                : (hdr ? "yuv420p10le" : "yuv420p"));
    const std::string matrix = hdr ? "bt2020nc" : "bt709";
    const std::string transfer = hdr ? (trc == "pq" ? "smpte2084" : "arib-std-b67") : "bt709";
    const std::string primaries = hdr ? "bt2020" : "bt709";

    std::string codecArgs;
    const double crf = args.getDouble("crf", 16.0);
    const std::string preset = args.get("preset", "medium");
    if (prores) {
        codecArgs = " -c:v prores_ks -profile:v 3 -vendor apl0";
    } else if (qsv) {
        // Intel Quick Sync: global_quality is QSV's quality knob (lower = better).
        codecArgs = std::string(" -c:v ") + (hdr ? "hevc_qsv" : "h264_qsv") +
                    " -global_quality " + formatNumber(crf + 4.0) + " -preset " + preset;
        if (hdr) codecArgs += " -profile:v main10";
    } else if (hdr) {
        codecArgs = " -c:v libx265 -crf " + formatNumber(crf) + " -preset " + preset +
                    " -x265-params " +
                    shellQuote("colorprim=bt2020:transfer=" + transfer +
                               ":colormatrix=bt2020nc:master-display=G(13250,34500)B(7500,3000)"
                               "R(34000,16000)WP(15635,16450)L(10000000,1):max-cll=1000,400");
    } else {
        codecArgs = " -c:v libx264 -crf " + formatNumber(crf) + " -preset " + preset;
    }

    const std::string interpName = args.get("interp", "tetra");
    const sr::Interp mode =
        interpName == "tri" ? sr::Interp::Trilinear : sr::Interp::Tetrahedral;
    const float strength = static_cast<float>(std::atof(args.get("strength", "1.0").c_str()));
    const int threads = sr::resolveThreadCount(args.getInt("threads", 0));
    const sr::TextureParams texture = parseTexture(args);
    // Lives for the whole render, including every segment.
    sr::GpuTexture textureGpuStorage;

    VideoRender job;
    job.ffmpeg = ffmpeg;
    job.inPath = inPath;
    job.width = width;
    job.height = height;
    job.fps = fps;
    job.spectral = spectral;
    job.lut = spectral ? nullptr : &lut;
    job.model = spectral ? &recipe.model : nullptr;
    job.gpu = gpu ? &gpuFilm : nullptr;
    job.bake = recipe.options;
    job.interp = mode;
    job.strength = strength;
    job.threads = threads;
    job.texture = texture;
    job.textureGpu = prepareTextureGpu(args, engine, textureGpuStorage);
    job.codecArgs = codecArgs;
    job.outFormat = outFormat;
    job.inMatrix = inMatrix;
    job.matrix = matrix;
    job.transfer = transfer;
    job.primaries = primaries;

    // Segments keep finished parts on disk so an interrupted render resumes
    // (D040). --resume alone picks a 10 s chunk.
    const bool resume = args.has("resume");
    double segmentSeconds = args.getDouble("segment", 0.0);
    if (segmentSeconds <= 0.0 && resume) segmentSeconds = 10.0;
    const bool segmented = segmentSeconds > 0.0;
    if (segmented && totalFrames <= 0) {
        return fail("--segment needs a known length: add --frames or --duration");
    }

    std::printf("input:  %s (%dx%d @ %.3f fps)\n", inPath.c_str(), width, height, fps);
    if (startFrame > 0 || totalFrames > 0) {
        std::printf("range:  start %.3f s, ", static_cast<double>(startFrame) / fps);
        if (totalFrames > 0) {
            std::printf("%ld frames\n", totalFrames);
        } else {
            std::printf("to the end\n");
        }
    }
    if (base.valid) {
        std::printf("timecode: %s -> %s\n", formatTimecode(base).c_str(),
                    formatTimecode(addFrames(base, startFrame, fps)).c_str());
    }
    const char* codecName = prores ? "ProRes 422 HQ"
                                   : (codec == "hevc" ? "HEVC Main10" : "H.264");
    std::printf("output: %s (%s, %s)\n", outPath.c_str(), codecName,
                hdr ? (trc == "pq" ? "PQ" : "HLG") : "SDR");
    if (spectral) {
        std::printf("engine: %s %s + %s\n", gpu ? "spectral-gpu" : "spectral",
                    recipe.film.info.name.c_str(), recipe.paper.info.name.c_str());
    } else {
        std::printf("engine: lut %s\n", lutPath.c_str());
    }
    if (job.textureGpu != nullptr) std::printf("texture: gpu (D3D11)\n");

    const std::filesystem::path outFile(outPath);
    std::string extension = outFile.extension().string();
    if (extension.empty()) extension = ".mov";
    const std::filesystem::path workDir(outPath + ".parts");

    sr::GrainFieldCache grainCache;
    const auto started = std::chrono::steady_clock::now();
    size_t renderedFrames = 0;
    size_t partCount = 1;
    std::string videoInput;
    std::string tempVideo;

    if (segmented) {
        const long segmentFrames =
            std::max(1L, static_cast<long>(std::llround(segmentSeconds * fps)));
        const long partTotal = (totalFrames + segmentFrames - 1) / segmentFrames;
        std::error_code ec;
        if (!resume) std::filesystem::remove_all(workDir, ec);
        ec.clear();
        std::filesystem::create_directories(workDir, ec);
        if (ec) return fail("cannot create " + workDir.string() + ": " + ec.message());

        // Never mix parts from a different codec, LUT or texture setting.
        const std::string signature =
            renderSignature(job, lutPath, startFrame, totalFrames, segmentFrames);
        const std::filesystem::path configPath = workDir / "config.txt";
        std::string existingSignature;
        {
            std::ifstream config(configPath);
            std::ostringstream text;
            text << config.rdbuf();
            existingSignature = text.str();
        }
        if (resume && !existingSignature.empty() && existingSignature != signature) {
            std::printf("settings changed since the last run: starting the parts over\n");
            std::filesystem::remove_all(workDir, ec);
            ec.clear();
            std::filesystem::create_directories(workDir, ec);
            if (ec) return fail("cannot create " + workDir.string() + ": " + ec.message());
        }
        {
            std::ofstream config(configPath, std::ios::trunc);
            config << signature;
        }

        std::printf("segments: %ld frames in %ld parts of up to %ld frames%s\n", totalFrames,
                    partTotal, segmentFrames, resume ? ", resuming" : "");
        std::vector<std::filesystem::path> parts;
        parts.reserve(static_cast<size_t>(partTotal));
        for (long index = 0; index < partTotal; ++index) {
            const long offset = index * segmentFrames;
            const long count = std::min(segmentFrames, totalFrames - offset);
            const std::filesystem::path part =
                workDir / ("part-" + zeroPad(index, 5) + extension);
            const std::filesystem::path marker(part.string() + ".done");

            bool done = false;
            if (resume) {
                std::ifstream state(marker);
                long savedStart = -1;
                long savedCount = -1;
                if (state >> savedStart >> savedCount) {
                    done = savedStart == startFrame + offset && savedCount == count &&
                           std::filesystem::exists(part);
                }
            }
            if (done) {
                std::printf("part %ld/%ld: already done\n", index + 1, partTotal);
                parts.push_back(part);
                continue;
            }

            std::printf("part %ld/%ld: frames %ld..%ld\n", index + 1, partTotal,
                        startFrame + offset, startFrame + offset + count - 1);
            size_t partFrames = 0;
            std::string partError;
            const std::string partTimecode =
                base.valid ? formatTimecode(addFrames(base, startFrame + offset, fps)) : "";
            if (!renderRange(job, startFrame + offset, count, offset, part.string(), partTimecode,
                             &grainCache, totalFrames, &partFrames, partError)) {
                return fail(partError + " (finished parts are kept for --resume)");
            }
            renderedFrames += partFrames;
            std::ofstream state(marker, std::ios::trunc);
            state << (startFrame + offset) << " " << count << "\n";
            parts.push_back(part);
        }

        // One concat list; the audio is copied once into the final file. Paths
        // are relative to the list, which sits in the parts directory.
        const std::filesystem::path listPath = workDir / "parts.txt";
        std::ofstream list(listPath, std::ios::trunc);
        for (const auto& part : parts) {
            list << "file '" << part.filename().string() << "'\n";
        }
        list.close();
        videoInput = " -f concat -safe 0 -i " + shellQuote(listPath.string());
        partCount = parts.size();
    } else {
        tempVideo = outPath + ".video" + extension;
        std::string renderError;
        const std::string rangeTimecode =
            base.valid ? formatTimecode(addFrames(base, startFrame, fps)) : "";
        if (!renderRange(job, startFrame, totalFrames, 0, tempVideo, rangeTimecode, &grainCache,
                         totalFrames, &renderedFrames, renderError)) {
            return fail(renderError);
        }
        videoInput = " -i " + shellQuote(tempVideo);
    }

    std::string finishError;
    if (!finishVideo(ffmpeg, videoInput, inPath, startFrame, totalFrames, fps, outPath,
                     finishError)) {
        return fail(finishError);
    }

    std::error_code cleanupError;
    if (!tempVideo.empty()) std::filesystem::remove(tempVideo, cleanupError);
    if (segmented && !args.has("keep-parts")) {
        std::filesystem::remove_all(workDir, cleanupError);
    }

    if (texture.grain.amount > 0.0f && renderedFrames > 0) {
        if (job.textureGpu != nullptr) {
            std::printf("grain fields: generated on the GPU (D3D11)\n");
        } else {
            std::printf("grain fields: %zu generated for %zu frames (rate %d)\n",
                        grainCache.builds(), renderedFrames, texture.grain.rate);
        }
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - started)
                             .count();
    if (renderedFrames == 0) {
        std::printf("resumed %s: all %zu parts were already done, %.2f s total\n",
                    outPath.c_str(), partCount, elapsed / 1000.0);
    } else {
        std::printf("wrote %s: %zu frames in %zu part%s, %.1f ms/frame, %.2f s total\n",
                    outPath.c_str(), renderedFrames, partCount, partCount == 1 ? "" : "s",
                    elapsed / static_cast<double>(renderedFrames), elapsed / 1000.0);
    }
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
