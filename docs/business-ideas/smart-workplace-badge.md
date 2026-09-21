<p align="right">
  <a href="smart-workplace-badge.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Smart Workplace Badge & Personal Task Assistant (IDEA-01)

## 1. Background & Pain Points (Requirements)

In modern enterprise workflows, Low-Code / No-Code (LCNC) platforms (e.g. Lark Base, DingTalk Yida, Monday.com, Retool, n8n, self-hosted BPM/ERP systems) power core operations: approval workflows, meeting bookings, defect logging, and task dispatching.

However, the user experience currently relies entirely on two virtual touchpoints:
- **PC Browser**: Workers away from their desks (patrols, workshops, meetings, pantry) completely lose connection. Desktop notification popups are frequently buried beneath stacked windows.
- **Mobile Phone Apps**:
  - Notification blindness: Enterprise push notifications are easily buried under WeChat, social media, and spam.
  - Distraction & friction: Unlocking a smartphone to view a single task frequently leads to phone distraction.
  - Environmental limits: In cleanrooms, labs, workshops, or when wearing gloves, unlocking phones is impractical.

**The Solution**: Turn the **FoloToy AI Passport** into a dedicated **Smart Workplace Badge**. It acts as a lightweight physical extension and tangible HUD of the enterprise low-code platform, worn around the neck or docked on the office desk.

---

## 2. Business User Story & Workflows (Business)

```
┌─────────────────┐       ┌─────────────────┐       ┌─────────────────┐       ┌─────────────────┐
│ 1. 08:55 Check-in│       │ 2. 10:00 Daily  │       │ 3. 14:25 Active │       │ 4. 16:30 Quick  │
│  & Desk Presence│       │   Todo Overview │       │   Meeting Alert │       │   Voice Logging │
├─────────────────┤       ├─────────────────┤       ├─────────────────┤       ├─────────────────┤
│ Tap NFC on desk │       │ Browse tasks via│       │ Urgent beep and │       │ Hold OK button: │
│ to log presence;│ ────► │ UP/DOWN keys;   │ ────► │ auto-wakeup on  │ ────► │ "Log 2hr on PRD"│
│ badge shows     │       │ long-press OK to│       │ screen: 5m before│      │ AI fills out the│
│ name & department│      │ check off items │       │ 302 room meeting│       │ low-code form   │
└─────────────────┘       └─────────────────┘       └─────────────────┘       └─────────────────┘
```

1. **Morning Clock-In & Profile Mode**:
   - The badge idles in low-power Profile Mode, displaying employee name, department, role, and avatar.
   - Touching the badge back (NTAG213 NFC) to a smartphone or reader pulls up their digital business card or logs physical attendance.
2. **Personal Todo Hub**:
   - Pressing `UP` / `DOWN` switches the display to the personal task list synchronized with the low-code database.
   - Completed an item? Long-press `OK`. The square `■` immediately flips to a green checkmark `✓`, writing the updated state back to the low-code database in real time.
3. **Active Meeting & Schedule Interruption**:
   - 5 minutes before a calendar event, the screen wakes up automatically with a clear chime: `14:30 Product Sync @ Room 302`.
   - The employee glances down, taps `OK` to acknowledge, and heads to the conference room without checking their phone.
4. **Conversational Data Entry**:
   - While walking, the employee holds `OK` and dictates: *"Book meeting room 302 for tomorrow 2 PM to review QA testing"*.
   - The low-code backend AI Agent extracts the structured JSON, checks room availability, creates the reservation, and flashes confirmation on the badge.

---

## 3. Architecture & Product Design (Design)

### 3.1 Screen Layout & Typography
Leveraging the repository's dedicated `passport-cjk-font` asset (`noto_cjk_16_compact_4bpp.bin`), all Chinese, Japanese, English, and status icons are rendered smoothly with 16-level grayscale anti-aliasing.

```text
┌──────────────────────────────────────────────┐
│ [Profile Mode]               10:45 [Battery] │
│                                              │
│ Technology Innovation Dept                   │
│ Zhang San  |  Senior Software Architect      │
│ ID: 8021                      Status: Active │
├──────────────────────────────────────────────┤
│ [Active Schedule Popup]                      │
│ ⏰ 14:25 (Starts in 5m)                      │
│ [Meeting] Architecture Review & Milestone    │
│ Location: Room 302 (3rd Floor)               │
│ ──────────────────────────────────────────── │
│ [OK] Acknowledge           [DOWN] Snooze 5m  │
├──────────────────────────────────────────────┤
│ [Personal Todo List]                         │
│ Personal Tasks (2/4 Done)                    │
│ ✓ 1. Resolve cloud websocket auth issue      │
│ ■ 2. Review low-code BPM design proposal     │
│ □ 3. Sign external vendor NDA contract       │
│ □ 4. Submit weekly engineering sprint report │
│ ──────────────────────────────────────────── │
│ [UP/DOWN] Scroll     [Hold OK] Mark Complete │
└──────────────────────────────────────────────┘
```

### 3.2 Hardware-Cloud Responsibility Boundary

| Node | Responsibilities | Constraints |
| :--- | :--- | :--- |
| **Physical Badge (ESP32-C3)** | • Display viewport rendering (LVGL 9 + CJK binary font).<br>• Physical button debounce and PTT voice streaming.<br>• Long-lived WSS connection handling and TLS ticket caching.<br>• Storing assigned Device-JWT in local NVS Flash. | Strictly zero PSRAM, maximum 4 KB receive window, never parses heavy JSON payloads. |
| **Cloud BFF / Gateway** | • WebSocket full-duplex session multiplexing.<br>• Low-code webhook ingestion and transformation into compact card JSON.<br>• Real-time speech recognition (ASR) and text-to-speech (TTS) streaming.<br>• Device binding lifecycle, token issuance, and permissions. | Keeps persistent socket handles for active badges. |
| **Low-Code Platform** | • Master database for tables, tasks, workflows, and calendar records.<br>• Visual business rules, automation triggers, and permission roles. | Dispatches event webhooks upon database row inserts/updates. |

---

## 4. Key Technical Details (Technical Implementation)

### 4.1 Zero-Friction Binding, Unbinding & Re-Authentication (UX Priority)

Enterprise users and business operators prioritize convenience and usability over military-grade zero-trust complexity. The badge must eliminate on-device password typing while preventing accidental logout.

```
┌─────────────────┐             ┌─────────────────┐             ┌─────────────────┐
│ AI Passport     │             │   Employee Phone│             │  Low-Code Server│
└────────┬────────┘             └────────┬────────┘             └────────┬────────┘
         │                               │                               │
         │ 1. Boot up (No token in NVS)  │                               │
         │    Display dynamic QR Code    │                               │
         │ -------------------------►    │                               │
         │                               │ 2. Scan QR via phone browser  │
         │                               │    (Already logged into SSO)  │
         │                               │ ----------------------------► │
         │                               │                               │ 3. Issues Long-Lived
         │                               │                               │    Device-JWT for User
         │ 4. Receives JWT via local net │                               │ ◄---------------------
         │    or gateway; writes to NVS  │                               │
         │ ◄-----------------------------│                               │
         │                               │                               │
         │ 5. Initiates WSS connection ──┼─────────────────────────────► │ 6. Handshake verified;
         │    Header: Bearer <Device-JWT>│                               │    Pushes personal
         │    Badge renders profile/todos│                               │    profile & schedules
```

1. **Initial Device Binding (3-Second QR Scan)**:
   - When a fresh badge boots, finding no credentials in NVS, it renders a high-contrast QR code on the ST7789 display (the NFC tag also broadcasts the same URL).
   - The employee scans the code with their smartphone. Because the smartphone is already authenticated in Lark/DingTalk/SSO, a mobile page appears: `Bind badge to Zhang San (Emp #8021)? [Confirm]`.
   - Tapping confirm prompts the low-code backend to generate a long-lived **`Device-JWT`** tied to that user. The badge receives the token, writes it to internal NVS Flash, and instantly switches to the employee's live dashboard.
2. **Re-Authentication & Offline Resilience**:
   - Powering off or battery exhaustion requires **zero re-login steps**.
   - On next boot, the firmware reads `Device-JWT` from NVS, connects to Wi-Fi, and resumes the WSS channel within ~300 ms via TLS session caching.
3. **Re-Binding / Handover Workflow**:
   - If the badge is handed over to a colleague:
     - **Software method**: Colleague scans the badge screen; the web portal shows: `Currently bound to Zhang San. Transfer to Li Si? [Confirm]`. Confirming revokes the old token, issues a new JWT over the WSS pipe, and refreshes the screen.
     - **Hardware reset method**: Simultaneously hold `[UP] + [DOWN]` for 5 seconds. The badge purges local NVS tokens and returns to the initial clean QR binding screen.

### 4.2 Full-Duplex WSS vs. REST: Network & Resource Invariant
- **Why REST is Prohibited for Live Operations**:
  - Repeated HTTPS REST requests consume 30–35 KB heap during each TLS negotiation, triggering severe memory fragmentation on the ESP32-C3 (~100 KB total heap).
- **The WSS Standard**:
  - Negotiates TLS once at connection time.
  - Heartbeat maintenance uses compact 2-byte ping/pong frames every 30–60 seconds.
  - While idle, Wi-Fi enters **Modem-Sleep mode** (average current ~15–20 mA), allowing an on-board 600–800 mAh battery to last through a standard 8–10 hour workday.

### 4.3 Active Wakeup Notification Flow
- **Urgent Notification**: When the low-code platform calendar triggers an event:
  1. Low-code sends an event payload to the cloud BFF.
  2. BFF pushes a compact JSON packet (`< 200 bytes`) down the open WebSocket connection.
  3. ESP32-C3 Wi-Fi interrupt wakes the CPU.
  4. Firmware turns on display backlight, triggers the active alert tone, and renders the schedule card.

---

## 5. Business Value & Retrospective (Summary)

1. **Tangible Edge for Low-Code Platforms**:
   - Transforms intangible digital workflows into dedicated, ambient physical hardware.
   - Eliminates notification fatigue and reduces approval turnaround time from hours to seconds.
2. **Zero Maintenance Burden**:
   - Elimination of on-device login menus prevents accidental logouts, user confusion, and IT service desk tickets.
   - Seamless QR/NFC binding allows self-service provisioning in seconds.
3. **Low BOM Cost & High Scalability**:
   - Standard ESP32-C3 hardware with no external PSRAM or bespoke chips minimizes unit production cost while preserving enterprise-grade CJK display typography and voice streaming.
