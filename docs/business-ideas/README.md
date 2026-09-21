<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Business Scenarios & Product Ideas

This directory catalogs business scenarios, product concepts, and real-world implementation proposals for **FoloToy AI Passport** paired with modern SaaS, Low-Code / No-Code platforms, and cloud-edge infrastructure.

---

## Document Index

| Scenario ID | Title | Core Focus | Document Link |
| :--- | :--- | :--- | :--- |
| **IDEA-01** | **Smart Workplace Badge & Physical Assistant** | Low-code integration, personal task/todo management, active schedule notifications, seamless QR/NFC binding & re-binding, JWT session management. | [smart-workplace-badge/README.md](smart-workplace-badge/README.md) |
| **IDEA-02** | **Interactive AI Children's Learning Companion** | AI educational flashcards, multilingual CJK typography, PTT voice dialog, server-side streaming bitmap fallback. | [children-ai-companion/README.md](children-ai-companion/README.md) |

---

## Scenario Document Template Standards

Every scenario design document follows a standardized 5-stage architecture:

1. **Background & Pain Points (Requirements)**: Real-world problems, UX friction, and business rationale.
2. **Business User Story & Workflows**: End-to-end journey from user, administrator, and system perspectives.
3. **Architecture & Product Design**: Screen layout, UI state transitions, interaction paradigms, and hardware-cloud division of responsibilities.
4. **Key Technical Details**: Low-power standby, network protocols (WSS vs. REST), authentication/binding lifecycle (JWT, NFC, QR code), hardware limits (ESP32-C3 zero-PSRAM, CJK font partition).
5. **Business Value & Retrospective**: Measurable ROI, differentiation, and operational takeaways.
