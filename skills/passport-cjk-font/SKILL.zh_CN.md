---
name: passport-cjk-font
description: 针对 FoloToy AI Passport（ESP32-C3，LVGL 9，无 PSRAM）使用或集成预编译工业级中日英多语言字库资产（noto_cjk_16_compact_4bpp.bin，837 KB，约 8,000 字符）。当需求涉及显示中文、日文、英文多语言文本、看板工作流状态图标或划定字库 Flash 分区时激活。
---

<p align="right"><a href="SKILL.md">English</a> · <strong>简体中文</strong></p>

# 多语言 CJK 字库资产与集成技能

## 触发时机与适用场景

当任务涉及以下场景时触发本技能：
- 在 AI Passport 屏幕上添加、渲染或排版中文、日文、英文文本；
- 在界面中添加业务状态图标与序号（`✓`, `⚠`, `•`, `·`, `①-⑩`, `■`, `□`, `●`, `○`, `★`, `☆`）；
- 诊断排查文字方块缺字、乱码、白块、或者加载大字体引发的内存崩溃（OOM）；
- 为字库配置自定义 Flash 分区表（`partitions.csv`）或通过 `esp_partition_mmap` 进行内存映射。

---

## 1. 什么样的应用适合使用这套字库？

| 业务应用类型 | 是否适配 | 适用理由与优势 |
| :--- | :---: | :--- |
| **智能工牌 / 工业仓储巡检看板** | **核心推荐** | 全量 GB2312（6,763 汉字）彻底覆盖机械五金配件（轴、阀、泵、螺、栓）、化工材料（钛、锂、镍、硅）及所有员工中文姓名，**99.99% 免维护不缺字**。 |
| **双语 / 多语言交互助手** | **核心推荐** | 日本官方常用汉字（2,135）+ 全套平假名/片假名 + ASCII 英文，字形笔画与灰度由思源黑体母版统一定标，**中日混排粗细绝不割裂**。 |
| **低代码工具 / 状态卡片 / 仪表盘** | **核心推荐** | 原生内嵌常用矢量状态图标（`✓`, `⚠`, `①-⑩`, `■`, `□`, `℃`, `%`, `±`），直接受 LVGL 文本颜色控制（变绿变黄变红），无需贴图。 |
| **极简纯离线英文计时器** | *可选* | 若仅仅显示几个数字 "00:00" 或少量固定英文单词，系统自带 Montserrat 即可。但凡涉及任何中日文显示，本字库为统一标准。 |

---

## 2. 规范字库核心参数

* **字库资产路径**：[`assets/fonts/noto_cjk_16_compact_4bpp.bin`](../../assets/fonts/README.zh_CN.md)
* **物理体积**：**837 KB**（仅占 8 MB Flash 的约 10%，剩余空间超 5.6 MB）。
* **渲染规格**：16 px 高度，4bpp 抗锯齿（16 级平滑灰度，手机视网膜级显示质感）。
* **字体母版与授权**：Google / Adobe 思源黑体（Noto Sans CJK SC）Regular，遵循 **SIL Open Font License 1.1**（永久免费商用，支持固件二次分发）。
* **字符覆盖范围（约 8,000 字符）**：
  - **中文**：全量 GB2312 国标汉字（6,763 字：一级常用 3,755 + 二级次常用 3,008）。
  - **日文**：日本官方常用汉字（常用漢字 2,135）+ 平假名（`0x3040-0x309F`）+ 片假名（`0x30A0-0x30FF`）。
  - **英文 / 数字**：标准 ASCII 可打印字符（`0x20-0x7E`）。
  - **工作流图标**：`✓ • · ① ② ③ ④ ⑤ ⑥ ⑦ ⑧ ⑨ ⑩ ⚠ ☎`
  - **状态形状与单位**：`■ □ ▲ △ ▼ ▽ ◆ ◇ ○ ◎ ● ★ ☆ ℃ % ° ± × ÷ ≠ ≤ ≥ ← ↑ → ↓ ¥ $ € £`
  - **韩语**：**0 字符**（彻底剔除全部韩文音节以节省宝贵 Flash）。

---

## 3. 硬件不可逾越红线 (ESP32-C3, 无 PSRAM)

> [!IMPORTANT]
> - **严禁在 ESP32-C3 上使用 FreeType / TTF 动态矢量解析**：FreeType 加载 CJK 映射表需瞬时消耗 60~120 KB 堆内存，在可用堆仅 ~100 KB 的 ESP32-C3 上必崩（OOM 看门狗复位）。
> - **严禁将 8,000 字字模写成 C 语言数组编译进代码段**：数十万行 C 数组会导致 GCC 编译宿主机内存爆满，且严重膨胀 OTA 固件包。
> - **必须采用 Flash 分区总线内存映射 (`esp_partition_mmap`)**：
>   - 在 `partitions.csv` 划分独立的 `font` 数据分区（建议大小 `0x0f0000` / 约 960 KB）。
>   - 将 `noto_cjk_16_compact_4bpp.bin` 烧录进该分区。
>   - 开机时调用 `esp_partition_mmap()` 将分区虚拟映射到 CPU 数据总线（DROM）。
>   - **运行时片内 SRAM 堆内存开销：绝对 0 字节！** 字模由 ESP32-C3 的硬件 Flash Cache 自动按需微秒级直读。

---

## 4. 固件工程集成范式

### 步骤一：配置分区表 (`partitions.csv`)
```csv
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   0x6000,
phy_init, data, phy,     0xf000,   0x1000,
factory,  app,  factory, 0x10000,  0x600000,
font,     data, 0x40,    0x610000, 0x0f0000,
```

### 步骤二：系统启动初始化映射并注册至 LVGL 9
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
        // 从映射内存直接创建 LVGL 9 binfont
        s_cjk_font = lv_binfont_create_from_buffer(s_font_mmap_ptr, part->size);
    }
}
```

---

## 5. 自动化构建工具

若后续有自定义字号或字符集变更需求，直接运行：
```bash
# 默认生成：assets/fonts/noto_cjk_16_compact_4bpp.bin (837 KB)
./tools/generate_cjk_font.sh
```
