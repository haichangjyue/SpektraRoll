// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#pragma once

#include <string>

namespace sr {

// How the input side of a bake is encoded.
//
//   Srgb           display-encoded sRGB, the M1 default (D013)
//   AppleLog       Apple Log code values in BT.2020 primaries (D021)
//   SLog3          Sony S-Log3 in S-Gamut3.Cine
//   CLog3          Canon Log 3 (v1.2) in Cinema Gamut
//   VLog           Panasonic V-Log in V-Gamut
//   SLog3SGamut3   Sony S-Log3 in S-Gamut3 (the wider, non-Cine gamut)
//   ArriLogC3      ARRI LogC3 (SUP 3.x, EI 800) in ARRI Wide Gamut 3
//   ArriLogC4      ARRI LogC4 in ARRI Wide Gamut 4
//   BmdFilmGen5    Blackmagic Film Generation 5 in Blackmagic Wide Gamut
//   DaVinciIntermediate  DaVinci Intermediate in DaVinci Wide Gamut
//
// New values are appended so the integers used by the GPU constant buffers
// keep their meaning (0 sRGB ... 4 V-Log).
enum class InputTransfer {
    Srgb,
    AppleLog,
    SLog3,
    CLog3,
    VLog,
    SLog3SGamut3,
    ArriLogC3,
    ArriLogC4,
    BmdFilmGen5,
    DaVinciIntermediate
};

// "srgb" / "apple-log" (also accepts "applelog", "apple_log") / "s-log3" /
// "s-log3-sgamut3" (also "s-gamut3") / "c-log3" / "v-log" / "logc3" (also
// "arri-logc3") / "logc4" / "bmd-film" (also "bmd-film-gen5") / "davinci"
// (also "davinci-intermediate"); known spellings are case-insensitive.
bool parseInputTransfer(const std::string& name, InputTransfer& out);

const char* inputTransferName(InputTransfer value);

// Comma separated list of every accepted --input spelling, for CLI errors.
const char* inputTransferList();

// sRGB transfer function, no clamping on encode (the caller decides).
double srgbDecode(double encoded);
double srgbEncode(double linear);

// Apple Log OETF and its inverse, normalised to [0, 1] code values.
// Constants from Apple's "Apple Log Profile" white paper, as implemented by
// colour-science 0.4.7 (log_encoding_AppleLogProfile /
// log_decoding_AppleLogProfile). 18% reflectance encodes to 0.4882724585.
//
// The curve has headroom below black: code 0 decodes to -0.0564, and true
// black (linear 0) sits at code 0.1504764523.
double appleLogEncode(double linear);
double appleLogDecode(double encoded);

// Camera log decodes, normalised code values in, scene-linear camera RGB out.
// Verified against colour-science 0.4.7 over a 101-point sweep (D060, D061).
double slog3Decode(double encoded);   // Sony S-Log3
double cLog3Decode(double encoded);   // Canon Log 3 (v1.2)
double vLogDecode(double encoded);    // Panasonic V-Log
double arriLogC3Decode(double encoded);          // ARRI LogC3, SUP 3.x, EI 800
double arriLogC4Decode(double encoded);          // ARRI LogC4
double bmdFilmGen5Decode(double encoded);        // Blackmagic Film Generation 5
double daVinciIntermediateDecode(double encoded);  // DaVinci Intermediate

// Decodes one input triple to linear sRGB (Rec.709 primaries). Log formats
// also apply their gamut matrix (BT.2020 for Apple Log, S-Gamut3.Cine or
// S-Gamut3, Cinema Gamut, V-Gamut, ARRI Wide Gamut 3 / 4, Blackmagic Wide
// Gamut, DaVinci Wide Gamut) and clamp negatives to zero, like the film model
// always did for Apple Log.
void decodeInputToLinearSrgb(InputTransfer input, const double encoded[3],
                             double linear[3]);

// Linear BT.2020 (D65) -> linear sRGB (D65). Out-of-gamut colours come back
// negative; the caller applies gamut mapping or clipping (D014).
void bt2020ToSrgb(const double rgb2020[3], double srgb[3]);

// SMPTE ST 2084 (PQ) and ITU-R BT.2100 HLG transfer functions, D029.
// PQ takes absolute luminance in cd/m2 and returns a [0, 1] code value;
// HLG takes a scene-linear value in [0, 1] (1.0 = peak).
double pqEncode(double nits);
double pqDecode(double code);
double hlgEncode(double linear);
double hlgDecode(double code);

}  // namespace sr
