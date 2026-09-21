---
name: passport-cjk-font
description: Use or integrate the pre-compiled industrial CJK (Chinese, Japanese, English) font asset (noto_cjk_16_compact_4bpp.bin, 837 KB, ~8,000 characters) for FoloToy AI Passport (ESP32-C3, LVGL 9, no PSRAM). Triggers whenever displaying Chinese, Japanese, multilingual text, dashboard status icons, or allocating font Flash partitions.
---

<p align="right"><a href="SKILL.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Multilingual CJK Font Asset & Integration Skill

## When to Use & Triggers

Activate this skill when the task involves:
- Adding or rendering Chinese, Japanese, or English text on the AI Passport display;
- Adding business workflow status icons (`✓`, `⚠`, `•`, `·`, `①-⑩`, `■`, `□`, `●`, `○`, `★`, `☆`);
- Resolving missing font glyphs, white rectangles, blank labels, or font memory crashes;
- Configuring custom partition tables (`partitions.csv`) for font assets or using `esp_partition_mmap`.

---

## 1. What Applications Should Use This Font?

| Application Category | Suitable? | Why? |
| :--- | :---: | :--- |
| **Smart Badges / Industrial Workflows** | **YES (Primary)** | Full GB2312 (6,763 Hanzi) covers factory hardware components (shaft, valve, pump, screw, bolt), chemical elements (titanium, lithium, nickel, silicon), and employee Chinese names with zero missing glyphs. |
| **Bilingual / Multilingual Assistants** | **YES** | Native Japanese Joyo Kanji (2,135 characters) + Hiragana + Katakana + Latin ASCII with perfectly harmonized stroke weight and optical density. |
| **Low-Code Tools / Dashboards / HUDs** | **YES** | Built-in monochrome vector status icons (`✓`, `⚠`, `①-⑩`, `■`, `□`, `℃`, `%`, `±`) controllable by LVGL text color. |
| **Minimal Offline Single-Screen Demos** | *Optional* | If an app only shows a fixed timer ("00:00") or 5 English words, Montserrat 14/20 is sufficient. But for any Chinese/Japanese UI, this asset is the standard. |

---

## 2. Canonical Font Asset Specifications

* **Asset File**: [`assets/fonts/noto_cjk_16_compact_4bpp.bin`](../../assets/fonts/README.md)
* **Physical Size**: **837 KB** (occupies only ~10% of 8 MB Flash, leaving > 5.6 MB free).
* **Display Metrics**: 16 px height, 4bpp (16-level grayscale smooth antialiasing).
* **Base Font & License**: Google / Adobe Noto Sans CJK SC Regular, licensed under **SIL Open Font License 1.1** (100% free for commercial use and firmware redistribution).
* **Character Coverage (~8,000 characters)**:
  - **Chinese**: Full GB2312 (6,763 Hanzi: Level 1 common 3,755 + Level 2 secondary 3,008). 99.99% coverage.
  - **Japanese**: Official Joyo Kanji (2,135 characters) + Hiragana (`0x3040-0x309F`) + Katakana (`0x30A0-0x30FF`).
  - **English / ASCII**: Alphanumerics and punctuation (`0x20-0x7E`).
  - **Workflow Icons**: `✓ • · ① ② ③ ④ ⑤ ⑥ ⑦ ⑧ ⑨ ⑩ ⚠ ☎`
  - **Status Shapes & Units**: `■ □ ▲ △ ▼ ▽ ◆ ◇ ○ ◎ ● ★ ☆ ℃ % ° ± × ÷ ≠ ≤ ≥ ← ↑ → ↓ ¥ $ € £`
  - **Korean**: **0 characters** (Hangul blocks completely excluded to save flash).

---

## 3. Mandatory Hardware Invariant (ESP32-C3, No PSRAM)

> [!IMPORTANT]
> - **NEVER use FreeType / TTF rendering on ESP32-C3**: FreeType requires 60–120 KB heap to parse CJK tables, causing immediate Out-of-Memory (OOM) crashes on ESP32-C3 (~100 KB free heap).
> - **NEVER compile ~8,000 glyphs as a C source array**: A 8,000-glyph C array bloats compile time by minutes and inflates the OTA application binary.
> - **ALWAYS use Flash Partition Memory-Mapping (`esp_partition_mmap`)**:
>   - Define a `font` data partition in `partitions.csv` (size `0xe0000` / ~900 KB).
>   - Flash `noto_cjk_16_compact_4bpp.bin` directly into this partition.
>   - Call `esp_partition_mmap()` during startup. The CPU directly reads glyphs via the hardware Flash MMU XIP bus cache.
>   - **Runtime SRAM heap consumption: Exactly 0 bytes!**

---

## 4. Firmware Integration Steps

### Step 1: Partition Table (`partitions.csv`)
```csv
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   0x6000,
phy_init, data, phy,     0xf000,   0x1000,
factory,  app,  factory, 0x10000,  0x600000,
font,     data, 0x40,    0x610000, 0x0f0000,
```

### Step 2: Mount and Register in LVGL 9
```c
#include "esp_partition.h"
#include "lvgl.h"

static const void *s_font_mmap_ptr = NULL;
static esp_partition_mmap_handle_t s_font_mmap_handle;
static lv_font_t *s_cjk_font = NULL;

void bsp_font_init(void)
{
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, 0x40, "font");
    if (!part) return;

    if (esp_partition_mmap(part, 0, part->size, SPI_FLASH_MMAP_DATA,
                           &s_font_mmap_ptr, &s_font_mmap_handle) == ESP_OK) {
        // Create LVGL 9 binfont from mapped pointer
        s_cjk_font = lv_binfont_create_from_buffer(s_font_mmap_ptr, part->size);
    }
}
```

---

## 5. Automation Tool

If custom sizes or modifications are requested, run:
```bash
# Default output: assets/fonts/noto_cjk_16_compact_4bpp.bin (837 KB)
./tools/generate_cjk_font.sh
```
