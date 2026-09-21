<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 字库资源目录（Fonts）

本目录存放 FoloToy AI Passport 的可复用字库资源，主要为针对 LVGL 9 与 Flash 独立分区内存映射优化的预编译二进制字库。

## 维护的字库资产

| 文件 | 规格与格式 | 字符覆盖范围 | 来源与许可 |
| --- | --- | --- | --- |
| [`noto_cjk_16_compact_4bpp.bin`](noto_cjk_16_compact_4bpp.bin) | 16 px，**4bpp** 抗锯齿，**837 KB** 二进制 | **约 8,000 字符**：全量 GB2312 国标汉字（6,763）+ 日本常用汉字（2,135）+ 日文假名 + ASCII + 业务流状态符号（`✓ ⚠ • · ①-⑩ ■ □ ● ○ ★ ℃ % ±`）。彻底剔除韩语。 | [Google / Adobe 思源黑体 (Noto Sans CJK SC)](https://github.com/googlefonts/noto-cjk) ([SIL OFL 1.1](https://scripts.sil.org/OFL)) |

## 生成与自定义

如需重新生成字库，请使用自动化构建脚本：

```bash
# 默认生成：assets/fonts/noto_cjk_16_compact_4bpp.bin（837 KB）
./tools/generate_cjk_font.sh
```

环境前提：
- 通过 `npm install -g lv_font_conv` 安装官方转换工具。
- 网络畅通以下载上游母字体 `NotoSansCJKsc-Regular.otf`（若本地已存在则自动复用）。

## 与 ESP32-C3 固件集成

鉴于 ESP32-C3 拥有 8 MB Flash 且无 PSRAM，本二进制字库推荐零 SRAM 内存开销的加载方式：

1. **Flash 分区隔离**：在 `partitions.csv` 声明独立数据分区（如 `font, data, 0x40, , 2500K`）。
2. **内存映射（XIP）**：系统启动时调用 `esp_partition_mmap()` 将字库分区映射到 CPU 虚拟地址空间，或通过 LVGL 9 的 `lv_binfont_create()` 配合文件系统驱动挂载。
3. **零 SRAM 堆开销**：CPU 在绘制时由硬件 Flash Cache 自动加速直读字模，完全不占用宝贵的片内 SRAM 堆内存。
