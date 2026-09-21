<p align="right">
  <a href="README.md">English</a> · <strong>简体中文</strong>
</p>

# FoloToy AI Passport — 端云一体化超级终端

本项目是基于官方 [`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) 的个人基线分支。
在完整继承官方硬件驱动与 BSP 能力的基础上，将这台小巧的 ESP32-C3 掌上设备定义为**端云一体化瘦客户端（Thin Client）超级终端**。通过与个人私有服务器或本地电脑协同，突破片上资源极限，赋予其大模型交互、长文本阅读、语音双向对讲与复古动效等多维能力。

---

## 核心架构与技术文档

- [**端云协同架构与通信协议手册**](docs/cloud-edge/architecture.zh_CN.md) ([English](docs/cloud-edge/architecture.md))：三级存储金字塔、2 KB 零 Flash 音频直通流、大文本虚拟视口滑动窗口、调色板单色/双色转码视频引擎以及全双工 WebSocket 载荷规范。
- [**端云方案概览**](docs/cloud-edge/README.zh_CN.md) ([English](docs/cloud-edge/README.md))：瘦客户端设计理念与文档导航。
- [**AI 开发技能清单**](skills/README.zh_CN.md) ([English](skills/README.md))：包含专属的 `passport-cloud-edge` 技能，让 AI 能够全自动理解并遵循端云开发规范。
- [**官方硬件与 BSP 文档**](docs/README.zh_CN.md) ([English](docs/README.md))：官方硬件引脚定义、外设总线与电路设计说明。

---

## 上游同步保证

本仓库的定制内容严格遵循模块化隔离设计：
- 架构规范与自定义技能独立存放在 `docs/cloud-edge/` 和 `skills/passport-cloud-edge/` 中。
- 可随时通过 GitHub 的 `Sync fork` 或 `git merge upstream/main` 同步官方最新代码，100% 保证零冲突自动合并。
