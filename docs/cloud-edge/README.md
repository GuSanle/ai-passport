<p align="right">
  <strong>English</strong> · <a href="README.zh_CN.md">简体中文</a>
</p>

# Cloud-Edge Architecture & Engineering Blueprint

> **Beyond Standalone Hardware**: A definitive technical guide for transforming the FoloToy AI Passport into a **Server-Supercharged Thin Client** powered by a self-hosted server or personal computer.

Official repository documentation focuses on standalone firmware, local offline menus, and isolated demo features. This directory documents the **cloud-edge collaborative architecture** that turns the ESP32-C3 into an ultra-low-latency, physical avatar for cloud AI and automated services, without violating no-PSRAM memory boundaries.

---

## Technical Documentation

| Document | Description |
| :--- | :--- |
| [**Cloud-Edge Architecture & Server BFF Blueprint**](architecture.md) ([简体中文](architecture.zh_CN.md)) | **The Master Engineering Manual**: 3-Tier storage hierarchy, single-tunnel WSS multiplexing, Virtual Viewport pagination for infinite text, 1-bit/2-bit palette video engine (Pip-Boy/Amber/Matrix), Dynamic Cloud Relay for passive NFC, and complete wire protocol schemas. |

---

## Upstream & Hardware References

For official hardware invariants, pin maps, and board specifications, refer directly to:
- Pin assignments and peripheral buses: [`components/bsp/include/bsp_pins.h`](../../components/bsp/include/bsp_pins.h)
- Hardware facts and electrical limits: [`docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md)
- Product physical specifications: [`docs/hardware-design/specifications.md`](../hardware-design/specifications.md)
- Repository agent rules: [`AGENTS.md`](../../AGENTS.md)
