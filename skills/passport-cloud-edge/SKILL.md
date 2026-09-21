---
name: passport-cloud-edge
description: Design and implement cloud-edge collaborative and thin-client applications for FoloToy AI Passport. Covers self-hosted server/BFF, WebSocket bidirectional streaming, real-time voice intercom, AI flashcards, LLM dialogs, low-code integrations, and streaming bitmaps while strictly respecting ESP32-C3 memory (no PSRAM) and audio streaming hardware constraints.
---

<p align="right"><a href="SKILL.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Cloud-Edge Collaborative Development Skill

## When to Use & Triggers

Activate this skill when user requirements involve:
- Connecting to a self-hosted backend (Node.js / Python / Go) or cloud BFF gateway;
- Real-time PTT voice streaming, streaming TTS playback, or network audio;
- Receiving LLM structured payloads to render interactive cards (flashcards, character cards, HUD telemetry, smart badges);
- Infinite text pagination, 1-bit/2-bit streaming bitmap animations, or passive NFC cloud relay workflows.

The device functions as a **thin client / physical avatar** (sensors, display viewport, buttons, audio I/O), while the external server handles **heavy compute, LLM orchestration, and business state**. Refer to [`docs/cloud-edge/architecture.md`](../../docs/cloud-edge/architecture.md) for full architectural guidelines and API payload schemas.

## Invariant Hardware & Resource Rules (ESP32-C3)

1. **Memory Budget**:
   - ESP32-C3 has ~100 KB usable heap and **NO PSRAM**.
   - Maximum network receive buffer is 4 KB.
   - Never load full JSON payloads, long texts, or uncompressed images into heap memory.

2. **Zero-Flash Audio Streaming & Physical Button Mapping**:
   - **Hardware Button Mapping**: The board features only three physical buttons (`UP`, `DOWN`, `OK`) sharing a single resistor ladder on GPIO0 (ADC1_CH0), with no separate voice side button. PTT voice streaming is typically mapped to **holding the `OK` button** (press triggers `ptt_start`, release triggers `ptt_end`), while `UP`/`DOWN` buttons are mapped to viewport scrolling and navigation.
   - **Audio input (PTT 16kHz 16-bit PCM, ~32 KB/s)**: stream directly from the I2S microphone to WebSocket using a 2 KB ping-pong double buffer.
   - **Audio output (MP3/PCM stream)**: stream directly from network to a circular buffer, feed to a lightweight software decoder (such as Helix-MP3), and send decoded PCM to I2S DMA.
   - **CRITICAL**: Never write streaming audio chunks to NOR Flash (SPI bus lock stalls I2S audio and burns flash endurance).

3. **Virtual Viewport for Large Texts (Mandatory WebSocket Pagination)**:
   - For articles, books, or lengthy LLM outputs, never buffer the full text on device.
   - The device requests and holds only the **active viewport window** (current screen + 1 screen buffer, < 1 KB RAM).
   - **Paging Interaction**: When pressing `UP`/`DOWN` to flip pages, the client sends a `{"type":"page_req", ...}` text frame over the active WebSocket connection. **Using REST requests for paging is prohibited**, preventing repeated TLS handshakes and paging latency.
   - Use server-side line-wrapping and pagination indexes (`page_idx`, `total_pages`).

4. **Retro Palette & Animation Engine**:
   - Do not attempt native on-device MP4/H.264 video decoding (uncompressed 240×320 frame is 150 KB, exceeding available RAM).
   - Video and animations must be transcoded on the server into 1-bit or 2-bit RLE/bitmap streams (Pip-Boy Green, Amber, Matrix, Game Boy palettes).
   - Device draws pre-indexed palettes or bitmaps to ST7789 via LVGL canvas with hardware scanline effects.

5. **NFC Dynamic Cloud Relay**:
   - The on-board NTAG213 tag is passive and fixed.
   - Never treat the NFC tag as dynamic memory; use its UID / static URL as an immutable pointer that maps to dynamic user metadata on the cloud server.

6. **Mandatory WebSocket Invariant for Interactive AI**:
   - **Interactive AI conversations, voice PTT streaming, card state updates, and audio/video streaming MUST strictly use a single full-duplex WebSocket (WSS) connection**.
   - **REST APIs are strictly prohibited for voice capture or interactive AI turn loops**. Root causes:
     1. **No PSRAM; RAM cannot buffer audio**: A 5-second PCM audio recording is 160 KB, while the ESP32-C3 has only ~100 KB free heap. It is physically impossible to buffer full audio in memory for a REST POST; audio must stream over WSS as it is captured (2 KB ping-pong DMA buffer).
     2. **TLS Handshake RAM Spike**: Each HTTPS handshake dynamically consumes 30–35 KB heap, triggering severe heap fragmentation and Out-of-Memory (OOM) aborts. In contrast, WebSocket handshakes once at boot/wakeup and maintains a steady ~12–16 KB context.
     3. **Full-Duplex Streaming & Barge-In**: REST half-duplex cannot concurrently push UI card states alongside audio streams, nor can it handle mid-speech user interruption (barge-in).
   - **Connection Lifecycle & Power-Saving Standards**:
     - Keep the WSS connection multiplexed during active conversations.
     - If idle for a configured timeout (recommended 30–60 seconds), the client should gracefully close the WSS connection and put the Wi-Fi modem into Modem-Sleep / Light-Sleep to conserve battery. The next button press instantly reconnects (leveraging TLS Session Resumption / tickets for sub-second handshake).
   - **Embedded Multilingual Font Asset & Bitmap Fallback**:
     - For on-device Chinese, Japanese, and English UI rendering, activate the dedicated [`passport-cjk-font`](../passport-cjk-font/SKILL.md) skill to integrate the pre-compiled 837 KB CJK binary font (`noto_cjk_16_compact_4bpp.bin`, mapped via `esp_partition_mmap` with zero SRAM heap cost).
     - When encountering ultra-rare characters, stroke-order animations, or glyphs outside the embedded font, the server streams a 1-bit monochrome bitmap slice (e.g. 64×64 bitmap is only 512 bytes) directly over WebSocket, cleanly bypassing MCU flash font limitations.

## Dual Protocol Strategy

- **REST API (HTTP/HTTPS) — Strictly for Cold-Path Operations**:
  - Used strictly for one-time or low-frequency stateless configurations: initial device registration, Wi-Fi credential provisioning, firmware manifest checks, static asset inventory.
  - JSON responses must remain concise (< 512 bytes).
  - **PROHIBITED** for voice streaming, interactive query loops, live token deltas, or card state updates.
- **WebSocket (WSS) — The Mandatory Standard for Interactive & Streaming AI**:
  - Multiplexes all hot-path traffic: real-time PTT voice streaming, live typewriter token streaming, card structured JSON payloads, TTS audio playback streaming, retro bitmap frames, and button telemetry.
  - Frame multiplexing:
    - Text frame: Structured JSON commands and card models (`type: "cmd"`, `"card_word"`, `"card_char"`, `"page"`).
    - Binary frame: Prefixed data streams (`[0x01]` raw audio stream, `[0x02]` bitmap/glyph slice).

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
