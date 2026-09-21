<p align="right">
  <strong>English</strong> · <a href="README.zh_CN.md">简体中文</a>
</p>

# FoloToy AI Passport — Cloud-Edge Supercharged Terminal

This repository is a personalized, upstream-aligned fork of [`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport).
It extends the baseline ESP32-C3 hardware into a **Cloud-Edge Thin-Client Supercharged Terminal**, pairing the compact pocket device with a private server or personal computer to unleash AI capabilities while strictly honoring hardware memory and flash boundaries.

---

## Key Architecture & Documentation

- [**Cloud-Edge Architecture Blueprint**](docs/cloud-edge/architecture.md) ([简体中文](docs/cloud-edge/architecture.zh_CN.md)): The master engineering manual covering the 3-tier storage hierarchy, zero-flash 2 KB audio streaming, virtual viewport text pagination, 1-bit/2-bit retro palette animations, and duplex WebSocket protocol schemas.
- [**Cloud-Edge Overview**](docs/cloud-edge/README.md) ([简体中文](docs/cloud-edge/README.zh_CN.md)): Overview of the thin-client philosophy and documentation navigation.
- [**AI Skills Catalog**](skills/README.md) ([简体中文](skills/README.zh_CN.md)): Specialized AI skills, including `passport-cloud-edge` for automated cloud-edge firmware development.
- [**Official Hardware Specifications**](docs/README.md) ([简体中文](docs/README.zh_CN.md)): Official hardware facts, pin assignments, schematics, and BSP documentation from upstream.

---

## Upstream Synchronization

This fork maintains clean modular isolation:
- Personal architecture blueprints and custom skills live in `docs/cloud-edge/` and `skills/passport-cloud-edge/`.
- Official updates from `upstream` (`FoloToy/ai-passport`) can be synchronized at any time with zero merge conflicts.
