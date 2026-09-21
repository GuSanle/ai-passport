<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Fonts Asset Directory

This directory stores reusable font assets for FoloToy AI Passport, specifically pre-compiled binary fonts for LVGL 9 and direct Flash partition mapping.

## Maintained Font Asset

| File | Size & Format | Character Coverage | Source & License |
| --- | --- | --- | --- |
| [`noto_cjk_16_compact_4bpp.bin`](noto_cjk_16_compact_4bpp.bin) | 16 px, **4bpp** antialiased, **837 KB** binary | **~8,000 characters**: Full GB2312 (6,763 Hanzi), Japanese Joyo Kanji (2,135), Hiragana, Katakana, ASCII, and workflow status symbols (`✓ ⚠ • · ①-⑩ ■ □ ● ○ ★ ℃ % ±`). Zero Korean. | [Google / Adobe Noto Sans CJK SC](https://github.com/googlefonts/noto-cjk) ([SIL OFL 1.1](https://scripts.sil.org/OFL)) |

## Generation and Customization

To regenerate or customize the font, run the automated build tool:

```bash
# Default: generates assets/fonts/noto_cjk_16_compact_4bpp.bin (837 KB)
./tools/generate_cjk_font.sh
```

Prerequisites:
- `lv_font_conv` installed via `npm install -g lv_font_conv`
- Internet access for downloading the upstream source font `NotoSansCJKsc-Regular.otf` (if not already cached locally).

## Integration with ESP32-C3 Firmware

Since the ESP32-C3 has 8 MB Flash and no PSRAM, the binary font is loaded without consuming internal SRAM:

1. **Flash Partition**: Define a data partition in `partitions.csv` (e.g. `font, data, 0x40, , 2500K`).
2. **Memory-Mapped Access**: Call `esp_partition_mmap()` during startup to map the font partition into CPU address space, or load it via LVGL 9's `lv_binfont_create()` with a filesystem driver.
3. **Zero RAM Heap Overhead**: The CPU directly reads glyph bit patterns from Flash MMU XIP cache on demand.
