# Film and paper profile data

The JSON files in this directory are **data**, not code. They come from the
upstream [spektrafilm](https://github.com/andreavolpato/spektrafilm) project by
**Andrea Volpato** and are licensed under
[CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/).

| File | What it is |
|---|---|
| `kodak_portra_400.json` | Kodak Portra 400 film profile (sensitivity, characteristic curves, dye densities) |
| `kodak_portra_endura.json` | Kodak Portra Endura paper profile |
| `neutral_print_filters.json` | Neutral enlarger CC values, keyed by paper / illuminant / film |
| `fujifilm_*.json`, `kodak_*.json` | 26 further upstream film / paper profiles, imported unchanged |

The UI lists only the pairs the C++ model has verified:

- **11 colour negative films** (D55): Fujifilm C200 / Pro 400H / X-Tra 400, Kodak Ektar 100,
  Gold 200, Portra 160 / 400 / 800 / 800 Push 1 / 800 Push 2, Ultramax 400.
- **6 papers**: Fujifilm Crystal Archive Type II; Kodak Ektacolor Edge,
  Endura Premier, Portra Endura, Supra Endura, Ultra Endura.

Not selectable yet (profiles still ship, for the CLI and future work):

- 4 positive/slide films (need the direct-scan path);
- Kodak Vision3 200T / 500T (tungsten illuminant, M1 is D55-only);
- Kodak Vision3 250D / 50D and Verita 200D (their `target_print` is the 2383 print
  film, which the engine cannot use as a paper yet);
- Kodak 2383 / 2393 print films themselves.

If you redistribute these files, or anything derived from them (including the
baked `.cube` LUTs in `tests/reference/`), you must:

1. credit Andrea Volpato and link <https://github.com/andreavolpato/spektrafilm>;
2. keep the CC BY-SA 4.0 licence notice;
3. licence modified versions under CC BY-SA 4.0 as well;
4. note any modifications.

The C++ code that reads these files is GPL-3.0-or-later, as described in the
repository `LICENSE` and `docs/licensing.md`.

Stock names (Kodak, Portra, Endura) are trademarks of Eastman Kodak Company.
They are used only to identify the measured material. SpektraRoll is not
affiliated with or endorsed by Kodak.
