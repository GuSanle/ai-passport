<p align="right">
  <a href="README.md">English</a> · <strong>简体中文</strong>
</p>

# 端云协同架构与工程方案指南

> **端云协同工程指南**：指导如何将 FoloToy AI Passport（ESP32-C3）作为**端云协同瘦客户端（Thin Client）**接入私有服务器或云端中枢（BFF）的技术手册。

本目录汇集了端云协同应用的最佳工程实践与通信协议标准，指导如何在严格遵守片上无 PSRAM 内存红线的前提下，实现实时语音对讲、AI 交互卡片、流式音频与远程低代码流程自动化。

---

## 技术规范文档

| 文档 | 简介与核心要点 |
| :--- | :--- |
| [**端云协同架构与服务端 BFF 蓝图**](architecture.zh_CN.md) ([English](architecture.md)) | **主工程技术规范**：三级存储金字塔、全双工单隧道 WSS 多路复用、面向无限长文本的虚拟视口分页、30fps 1-bit/2-bit 调色板复古视频引擎（Pip-Boy/琥珀黄/黑客帝国/Game Boy）、NFC 动态云端中继以及接口标准载荷规范。 |

---

## 官方硬件参考

关于官方硬件事实、引脚分配与电气特性，请直接查阅官方权威文档：
- 引脚映射与外设总线：[`components/bsp/include/bsp_pins.h`](../../components/bsp/include/bsp_pins.h)
- 硬件事实与开发边界：[`docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md`](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md)
- 产品物理与电气规格：[`docs/hardware-design/specifications.zh_CN.md`](../hardware-design/specifications.zh_CN.md)
- 代码库 AI 交互总则：[`AGENTS.zh_CN.md`](../../AGENTS.zh_CN.md)
