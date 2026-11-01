// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "sr/film_model.hpp"
#include "sr/image.hpp"
#include "sr/lut3d.hpp"
#include "sr/metrics.hpp"
#include "sr/profile.hpp"

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
        "                            [--title <text>]\n"
        "      Bake the M1 spectral film model (Kodak Portra 400 + Portra Endura\n"
        "      by default) into a .cube LUT. Output is display-encoded sRGB. With\n"
        "      --input apple-log the LUT expects Apple Log code values in BT.2020\n"
        "      primaries (e.g. a frame decoded from an iPhone ProRes/HEVC clip),\n"
        "      and --exposure shifts scene exposure in stops before the film model.\n"
        "      --scan negative scans the negative itself (inverted against the film\n"
        "      base, no paper): more highlight/shadow latitude, flatter image.\n"
        "      --contrast pivots at 18% grey (negative scans default to 1.15,\n"
        "      pass 1.0 for the flat film curve only).\n"
        "\n"
        "  spektra-roll-cli lut apply --lut <file.cube> --in <image> --out <image>\n"
        "                             [--interp tetra|tri] [--strength 0..1] [--bits 8|16]\n"
        "      Apply a 3D LUT to an image. Output bit depth defaults to the input's.\n"
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
        "  * The film model is a pure per-pixel function: no grain, halation,\n"
        "    diffusion or output gamut compression. This is milestone M1.\n",
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

    sr::FilmModel model;
    if (!model.configure(film, paper, filters, error)) return fail(error);

    sr::BakeOptions options;
    if (args.has("input")) {
        if (!sr::parseInputTransfer(args.get("input"), options.input)) {
            return fail("--input expects 'srgb' or 'apple-log'");
        }
    }
    options.exposureEv = args.getDouble("exposure", 0.0);

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

int cmdLutApply(const Args& args) {
    const std::string lutPath = args.get("lut");
    const std::string inPath = args.get("in");
    const std::string outPath = args.get("out");
    if (lutPath.empty()) return fail("lut apply requires --lut <file.cube>");
    if (inPath.empty()) return fail("lut apply requires --in <image>");
    if (outPath.empty()) return fail("lut apply requires --out <image>");

    sr::Lut3D lut;
    std::string error;
    if (!lut.loadCube(lutPath, error)) return fail(error);

    sr::Image image;
    int sourceBits = 8;
    if (!sr::loadImage(inPath, image, error, &sourceBits)) return fail(error);

    const std::string interpName = args.get("interp", "tetra");
    const sr::Interp mode =
        interpName == "tri" ? sr::Interp::Trilinear : sr::Interp::Tetrahedral;
    const float strength = static_cast<float>(std::atof(args.get("strength", "1.0").c_str()));
    const int bits = requestedBits(args, sourceBits);

    const auto started = std::chrono::steady_clock::now();
    const size_t count = static_cast<size_t>(image.width) * image.height;
    for (size_t i = 0; i < count; ++i) {
        float* px = &image.pixels[i * 3];
        const sr::Rgb src{px[0], px[1], px[2]};
        const sr::Rgb mapped = lut.apply(src, mode);
        const sr::Rgb result = sr::mix(src, mapped, strength);
        px[0] = result.r;
        px[1] = result.g;
        px[2] = result.b;
    }
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

int cmdConvert(const Args& args) {
    const std::string inPath = args.get("in");
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
