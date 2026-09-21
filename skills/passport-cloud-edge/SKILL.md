---
name: passport-cloud-edge
description: Design and implement cloud-edge collaborative applications for FoloToy AI Passport, pairing the ESP32-C3 thin client with a backend server/BFF via WebSocket and REST APIs while strictly respecting hardware memory, audio streaming, and display constraints.
---

<p align="right"><a href="SKILL.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Cloud-Edge Collaborative Development Skill

Use this workflow when designing or implementing networked, server-augmented, or AI-powered applications on the FoloToy AI Passport.
The device functions as a **thin client / physical avatar** (sensors, display viewport, audio I/O), while the user's private server or PC acts as the **super-brain and source of truth**.

Refer to [`docs/cloud-edge/architecture.md`](../../docs/cloud-edge/architecture.md) for full architectural guidelines and API payload schemas.

## Invariant Hardware & Resource Rules (ESP32-C3)

1. **Memory Budget**:
   - ESP32-C3 has ~100 KB usable heap and **NO PSRAM**.
   - Maximum network receive buffer is 4 KB.
   - Never load full JSON payloads, long texts, or uncompressed images into heap memory.

2. **Zero-Flash Audio Streaming**:
   - Audio input (PTT 16kHz 16-bit PCM, ~32 KB/s): stream directly from the I2S microphone to WebSocket using a 2 KB ping-pong double buffer.
   - Audio output (MP3/PCM stream): stream directly from network to the I2S audio codec via a circular buffer.
   - **CRITICAL**: Never write streaming audio chunks to NOR Flash (SPI bus lock stalls I2S audio and burns flash endurance).

3. **Virtual Viewport for Large Texts**:
   - For articles, books, or lengthy LLM outputs, never buffer the full text on device.
   - The device requests and holds only the **active viewport window** (current screen + 1 screen buffer, < 1 KB RAM).
   - Use server-side line-wrapping and pagination indexes (`page_idx`, `total_pages`).

4. **Retro Palette & Animation Engine**:
   - Do not attempt native on-device MP4/H.264 video decoding (uncompressed 240×320 frame is 150 KB, exceeding available RAM).
   - Video and animations must be transcoded on the server into 1-bit or 2-bit RLE/bitmap streams (Pip-Boy Green, Amber, Matrix, Game Boy palettes).
   - Device draws pre-indexed palettes or bitmaps to ST7789 via LVGL canvas with hardware scanline effects.

5. **NFC Dynamic Cloud Relay**:
   - The on-board NTAG213 tag is passive and fixed.
   - Never treat the NFC tag as dynamic memory; use its UID / static URL as an immutable pointer that maps to dynamic user metadata on the cloud server.

## Dual Protocol Strategy

- **REST API (HTTP/HTTPS)**:
  - Used strictly for **cold-path operations**: device registration, Wi-Fi provisioning, profile synchronization, static asset discovery.
  - JSON responses must be concise (< 512 bytes).
- **WebSocket (WSS)**:
  - Used for **hot-path duplex operations**: real-time push-to-talk voice streaming, LLM token streaming, sensor telemetry, and RPC controls.
  - Frame multiplexing:
    - Text frame: Structured JSON commands (`type: "cmd"`, `"stream_start"`, `"status"`, etc.).
    - Binary frame: Prefixed audio chunks (`[0x01][Audio Payload]`) or image slices (`[0x02][Bitmap Payload]`).

## AI Workflow for Cloud-Edge Apps

1. **Understand Role Boundaries**:
   - Device logic belongs in `main/` (UI, LVGL widgets, button event handlers, WSS client worker task).
   - BSP hardware logic belongs in `components/bsp/`.
   - Business logic, heavy LLM processing, database storage, and transcoding belong on the external server.
2. **Implement Network Worker Tasks**:
   - Keep network and audio tasks strictly separate from the LVGL GUI task.
   - When updating UI from network callbacks, always acquire `bsp_lvgl_lock()`.
3. **Validate**:
   - Run host tests for frame parsing and packet framing (`./tools/validate.sh --static`).
   - Validate build with `./tools/validate.sh --firmware`.
