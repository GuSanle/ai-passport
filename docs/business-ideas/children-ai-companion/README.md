<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Children's Interactive AI Learning Companion (IDEA-02)

## 1. Background & Pain Points (Requirements)

Early childhood and primary language education (Chinese character literacy, English vocabulary, pinyin, stroke order) relies heavily on either printed flashcards or tablet apps.

Both approaches suffer from critical flaws:
- **Traditional Printed Flashcards**: Completely static, cannot pronounce or interact, lacks feedback loops, and carries zero personalized AI responsiveness.
- **Tablets & Smartphones**:
  - Screen addiction: High risk of myopia, gaming temptations, and digital distraction.
  - Parents constantly worry about unmonitored browsing and screen time.
  - Clunky for young children who cannot type or search efficiently.

**The Solution**: The **FoloToy AI Passport** configured as a **Pocket AI Learning Companion**. It combines screen-free eye-friendly display aesthetics, physical tactile buttons, real-time voice streaming with speech recognition, and instant AI-generated educational flashcards.

---

## 2. Business User Story & Workflows (Business)

```
┌─────────────────┐       ┌─────────────────┐       ┌─────────────────┐       ┌─────────────────┐
│ 1. Child Holds  │       │ 2. Cloud AI     │       │ 3. Instant Card │       │ 4. Parent Views │
│    OK to Ask    │       │    Orchestration│       │    & Voice Echo │       │    Progress     │
├─────────────────┤       ├─────────────────┤       ├─────────────────┤       ├─────────────────┤
│ "How do you say │       │ ASR transcribes;│       │ Clear audio:    │       │ Low-code system │
│ 'apple' and how │ ────► │ LLM creates a   │ ────► │ "A-P-P-L-E";    │ ────► │ updates daily   │
│ do you write it?│       │ flashcard model │       │ LCD shows card  │       │ word count &    │
│                 │       │ + phonetic text │       │ with definitions│       │ review mastery  │
└─────────────────┘       └─────────────────┘       └─────────────────┘       └─────────────────┘
```

1. **Natural Voice Interaction**:
   - The child presses and holds the large center `OK` button: *"What does 'giraffe' mean in Chinese, and what is its pinyin?"*
   - Releasing the button sends the audio directly to the cloud backend.
2. **Audio-Visual Synchronized Learning**:
   - The speaker sounds immediately with natural streaming TTS, speaking with a warm child-friendly voice.
   - Concurrently, the screen renders an interactive flashcard: word, phonetic guide, radical breakdown, and example sentences.
3. **Paging & Quiz Reinforcement**:
   - The child presses `UP` / `DOWN` to flip through their daily review deck.
   - Long-pressing `OK` enters a quick oral test mode where the device prompts the child to pronounce the word.
4. **Parental Low-Code Sync**:
   - Words learned and quiz results synchronize directly to the family or school low-code database (e.g. child literacy progress tracker).

---

## 3. Architecture & Product Design (Design)

### 3.1 Flashcard Visual Layout
Utilizing the `noto_cjk_16_compact_4bpp.bin` font, character glyphs, tone marks, and symbols render crisply on the 240×320 display.

```text
┌──────────────────────────────────────────────┐
│ [English Word Flashcard]      [Audio Icon] ♫ │
│                                              │
│               G i r a f f e                  │
│              /dʒəˈræf/  (Noun)               │
│                                              │
│ Meaning: A tall African animal with a very   │
│ long neck and spotted skin.                  │
│                                              │
│ Example: The giraffe eats leaves from trees. │
│ ──────────────────────────────────────────── │
│ [Hold OK] Speak     [UP/DOWN] Flip Next Card │
├──────────────────────────────────────────────┤
│ [Chinese Literacy Flashcard]                 │
│                                              │
│               [ HANZI GLYPH ]                │
│               cháng jǐng lù                  │
│                                              │
│ Radical: Zhi (7 strokes)  Structure: Semi-enc│
│ Meaning: Mammal living in grasslands with an │
│ extremely long neck (Giraffe).               │
│ ──────────────────────────────────────────── │
│ [Hold OK] Practice  [UP/DOWN] Flip Next Card │
└──────────────────────────────────────────────┘
```

---

## 4. Key Technical Details (Technical Implementation)

### 4.1 Zero-Heap CJK Font Asset Integration
- Embedded Flash partition holds the 837 KB `noto_cjk_16_compact_4bpp.bin` font file.
- Memory-mapped using `esp_partition_mmap`. Zero SRAM heap consumption ensures plenty of room for LVGL drawing buffers and networking buffers.

### 4.2 Handling Rare Characters & Stroke Order Animations (Streaming Bitmap Bypass)
- **Problem**: Pre-compiled fonts cannot hold every ancient oracle bone glyph, rare calligraphy style, or dynamic stroke-by-stroke animation without exceeding flash budgets.
- **Solution**:
  - The cloud BFF renders the character stroke animation or rare glyph server-side into a 1-bit monochrome bitmap frame (e.g. 64×64 pixels = 512 bytes).
  - The frame is transmitted over WebSocket binary frames (`[0x02] + raw payload`) and drawn directly onto the LVGL canvas.
  - This completely bypasses MCU font size limitations.

### 4.3 Low-Latency Audio Streaming
- Child audio input is recorded at 16 kHz 16-bit PCM and streamed immediately to the cloud over a ping-pong buffer (< 2 KB).
- Cloud TTS is transcoded to lightweight MP3 streams and decoded on-chip with zero flash write operations.

---

## 5. Business Value & Retrospective (Summary)

1. **Hardware-Guaranteed Healthy Screen Time**:
   - Small, non-distracting screen dedicated solely to literacy and vocabulary. Zero video games, zero social media temptations.
2. **True Interactive Conversational Tutor**:
   - Far superior to static cards; answers infinite questions with LLM intelligence.
3. **Effortless Integration with Learning Dashboards**:
   - Schools, parents, and tutoring providers can manage word lists, review schedules, and learning analytics directly from low-code tabular databases.
