<p align="right">
  <a href="README.md">English</a> · <strong>简体中文</strong>
</p>

# 端云协同架构与工程方案指南

> **突破单机硬件极限**：指导如何利用个人私有服务器或个人电脑（PC），将 FoloToy AI Passport 打造为**端云一体化超级瘦客户端（Thin Client）**的技术手册。

官方代码库主要关注单机离线菜单与独立的微型演示程序。本目录记录了**端云协同架构**的最佳工程实践，指导如何将 ESP32-C3 转化为云端 AI、大模型和自动化流程的高响应物理载体，同时绝对不超出片上无 PSRAM 的内存红线。

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
