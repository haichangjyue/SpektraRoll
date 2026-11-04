# Fonts

The macOS UI uses **思源黑体** (Source Han Sans) throughout. The build script
bundles any `.otf` / `.ttf` / `.ttc` files from this directory into
`SpektraRoll.app/Contents/Resources/Fonts` and registers them at launch.

Because font binaries are large (about 16 MB per weight) they are **not**
committed. Run:

```bash
scripts/fetch-macos-fonts.sh
```

This downloads **Noto Sans CJK SC Regular/Bold**, the Google OFL release of the
same Source Han Sans design. If no bundled font is present, the app falls back
to `Noto Sans SC` / `PingFang SC` / the system font.

## Licence

Noto Sans CJK is licensed under the **SIL Open Font License 1.1**. Keep the
OFL text with any redistributed app that contains the font. Source Han Sans is
also OFL 1.1. Do not commit fonts unless you also commit their licence text.
