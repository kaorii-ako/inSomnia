# inSomnia

![inSomnia](images/render-hero.png)

An adaptive, context-aware wake clock. Instead of firing at a fixed timestamp, it wakes you inside a window at a moment you are already stirring — and if you got up before the window even opened, it stays quiet.

Built for [Hack Club Forge](https://forge.hackclub.com). Firmware-first, fully offline after WiFi time sync, no cloud service, no audio leaves the device.

---

## What it actually does

| Situation | Behaviour |
|---|---|
| You're already up before the window opens | Alarm is suppressed for the day and logged as "skipped — you were already up" |
| You're carrying sleep debt | The gentle wake is held back by up to 15 min so an early stir doesn't cost you the lie-in. **The hard deadline never moves.** |
| You start stirring inside the window | Gentle wake begins immediately — soft tone ramping up over several minutes |
| You sleep right through to the hard deadline | Full-intensity alarm, no more waiting |
| A sensor dies or is unplugged | **Failsafe**: degrades to a plain fixed-time alarm at the deadline. It never fails silent. Even with no WiFi it runs on an internal fallback clock. |

### Why I made it

I hate being ripped out of deep sleep at 06:50 exactly when I was finally sleeping well, and I also hate my phone blaring when I’m already in the kitchen making coffee. Both are the same bug: a clock that knows time but not context. I wanted a clock that knows *whether I'm in bed and moving* — easily measurable with a thermal array — and uses that to pick a kinder moment inside a window, or to stay quiet if I’m already up. I built it for myself, and because a believable demo needs honest numbers, not "AI" marketing.

### The 12 keys

| | col 1 | col 2 | col 3 | col 4 |
|---|---|---|---|---|
| **row 1** (front) | `DISMISS` | `SNOOZE` | `LIGHT` | `SKIP` |
| **row 2** | `OPEN −` | `OPEN +` | `DEAD −` | `DEAD +` |
| **row 3** (back) | `PAGE` | `SOFT` | `LOUD` | `SAVE` |

Row 1 is what you hit half-asleep. Rows 2 and 3 are setup you do awake: move the window and deadline in 5-minute steps, page through status / window / sleep-debt screens, test the alarm, save to flash. Everything is also on the web UI at `http://insomnia.local`.

![keypad legend](images/keymap.png)

## How to use it — step by step

1. **Assemble** — Solder the PCB (`hardware/inSomnia.kicad_pcb`, 0805 passives, through-hole tactiles), mount the board on the 4× M2 bosses in `hardware/case/inSomnia_case.FCStd`, plug modules onto headers: J3 ST7735 TFT, J4 INMP441 mic, J5 MLX90640 thermal (aimed at the bed), J7 optional native USB.
2. **Flash** — `cd firmware && pio run -t upload` and `pio device monitor` at 115200. Requires PlatformIO + `espressif32@6.5.0`.
3. **First boot WiFi** — No credentials stored, so it opens AP `inSomnia-setup` (pass `insomnia`). Join it, open `http://192.168.4.1`, pick your network + password. Reboots and appears as `http://insomnia.local` (mDNS). If no WiFi, it still runs on a fallback clock (`*` beside time) and will still alarm at the deadline.
4. **Set your window** — On device: `OPEN ±` and `DEAD ±` move times in 5-min steps, `PAGE` to see them, `SAVE` to flash. Or on web UI: *Settings → Window opens / Hard deadline*.
5. **Sleep** — Place the clock on the nightstand with the thermal aperture aimed at the bed (within ~1.5 m, no wire to the bed). ` roi` in Settings crops the bed region if needed.
6. **Morning** — `DISMISS` stops, `SNOOZE` (= snoozeMin, default 7) resumes as hard alarm, `SKIP` is manual "don't bother tonight". Last night's sleep: `PAGE` → debt screen or web *Last night* card (TST / efficiency / WASO / SOL — labelled as estimates).
7. **Tune** — Web *Settings*: `countScale` is the one calibration — watch *Counts this epoch* overnight, set so quiet epoch ≈ 0; `Body margin` and `in-bed min pixels` for duvet thickness.

## How it decides

One sensor does most of the work:

- **MLX90640 32×24 thermal array**, on the clock, aimed at the bed.
  - *Frame-to-frame change* → a motion index. This is contactless video actigraphy, validated against polysomnography at **Cohen's κ = 0.733**, comparable to a wrist actigraph. It feeds Cole-Kripke.
  - *Warm body-sized region* → **in bed / out of bed**. Far stronger than a PIR, because it can tell you-in-the-bed from a warm object in the room.
  - No wire to the bed, and at 768 pixels you cannot identify a person from it.
- **INMP441 I2S microphone** → RMS level per epoch. A loudness hint that nudges the stirring threshold. It is a level detector, *not* a sound classifier.

An earlier version used a mattress accelerometer. It was dropped because it needs 1–2 m of I2C cable from the nightstand to the bed, and I2C is spec'd to 400 pF (≈1 m at 100 kHz) against cable that runs 100–240 pF/m.

Two separate pieces of logic sit on top, because they answer different questions:

1. **A causal 3-state machine** (asleep / stirring / awake) with hysteresis. It may only look backwards, because it drives the alarm in real time. This is a threshold heuristic — see the honesty section below.
2. **Cole-Kripke (1992)** for the morning report. It needs the two *following* epochs, so it lags 2 minutes and can never drive a live decision.

## Being honest about the "AI"

This matters more than the feature list, so it goes near the top.

**The on-device logic is not AI.** Cole-Kripke is a weighted moving average with a threshold, published in 1992. The 3-state classifier is thresholds plus hysteresis. Calling either one "AI" would be marketing, not engineering.

**Accuracy ceiling.** Actigraphy sleep/wake scoring agrees with clinical polysomnography roughly **78–80%** of the time. But its specificity for *wake* is only **0.35–0.64** — it is much better at spotting sleep than wakefulness. Sound-based staging is no better: the best published smartphone-audio model (HomeSleepNet) gets 76.2% overall and **63.4% on wake epochs**.

That single fact shaped the design. Alarm suppression is the feature most exposed to a wrong "you're awake" call, so it is **not** driven by the sleep classifier. It is driven by `outOfBed()` — no warm mass in the bed region for 3 epochs — which is close to a binary measurement rather than a 64%-accurate inference.

**This is not clinical sleep tracking.** Without EEG there is no true sleep staging. Consumer trackers systematically overestimate sleep and underestimate wake-after-sleep-onset, worst of all for people who sleep badly. The numbers in the app are estimates and are labelled as such.

**One number you must calibrate.** Cole-Kripke's weights assume ActiGraph counts, a proprietary unit. Ours are arbitrary units from your own mattress, so `countScale` in Settings divides ours into that range. The *algorithm* is validated; this *scaling* is not. Watch epoch counts overnight and set it so a quiet epoch lands near zero.

**Posture is not claimed.** The thermal array reports *movement and position*. Classifying sleeping posture under a duvet at 32×24 is a research problem — the published work that does it used an infrared **depth** camera with synthetic blanket augmentation. Movement and position are what the sleep model needs, so that is what is built and that is what it says.

**Sleep debt is an estimate built on an estimate.** Debt is `max(0, need − actual)` summed over a rolling 14 nights, which is the window research finds matters for current impairment (Van Dongen 2003: 6 h/night for 14 days ≈ two nights of total deprivation). But "actual" comes from Cole-Kripke scoring that under-reports wake — so read it as a trend, not a clinical figure.

**No trained model ships yet.** A learned audio or thermal classifier is the obvious next step, but a model needs labelled data and there is no polysomnography ground truth here to validate against. The honest path is to ship the validated heuristic, collect feature data, and only then train.

---

## Hardware

**ESP32-S3-WROOM-1-N16R8** — WiFi + BLE 5, native I2S, 8 MB PSRAM, SIMD for future MFCC work. Chosen over the ESP32-C3 because the C3 has no PSRAM and too few GPIO once an I2S microphone is added.

Schematic, pin map, BOM and wiring notes: **[`hardware/HARDWARE.md`](hardware/HARDWARE.md)**
BOM with pricing: [`hardware/inSomnia_bom.csv`](hardware/inSomnia_bom.csv) — **~$68.62 for 1 unit incl. sensors** (MLX90640 dominates)
Rendered schematic: `hardware/inSomnia_schematic.pdf` and `images/schematic.png`
Gerbers: `hardware/inSomnia_gerbers.zip` + `hardware/fab/`

> The hero image above is a **render** from the actual FreeCAD STL (`hardware/case/inSomnia_case_shell.stl`, 7,628 verts), not a photo — nothing has been fabricated yet. It is built to the same dimensions as the FreeCAD enclosure below, and the board renders come from the actual KiCad files.

### Wiring

![schematic](images/schematic.png)

*All modules plug onto headers — no soldering to the bed. See `hardware/HARDWARE.md` pin map for the exact GPIO table and `images/routing-front-copper.png` for the routed board.*

| Header | Module | Wires |
|---|---|---|
| J3 1×08 | ST7735 128×160 TFT | SCK 12, MOSI 11, CS 10, DC 9, RST 8, BL 13 (PWM), +3V3/GND |
| J4 1×06 | INMP441 mic (I2S) | BCLK 4, WS 5, DIN 6, +3V3/GND |
| J5 1×05 | MLX90640 32×24 thermal (I2C) | SDA 1, SCL 2 (4.7k pull-ups), +3V3/GND |
| J6 1×03 | Spare / was PIR | +5V, IO14, GND |
| J7 1×04 | Native USB | D- 19, D+ 20 |
| J1 1×02 | Power 5V in | 5V → AMS1117-3.3 → 3V3 |
| J2 1×06 | UART prog | TX/RX + EN/BOOT tactiles SW1/SW2 |

### Enclosure

Parametric case in FreeCAD: **`hardware/case/`** (STEP + STL + `.FCStd`).

- 106 × 86 × 54 mm outer, 2.5 mm walls, 101 × 81 mm cavity for the board
- Wedge profile, display face sloped 44.1°
- **36 × 29 mm display window** — a 1.8" ST7735's *active area* is only ~35 × 28 mm, much smaller than the module outline
- 12 keypad holes cut at the real SW3–SW14 coordinates from the KiCad file
- Thermal aperture, buzzer grille, rear cable exit, regulator vents, 4× M2 bosses aligning with PCB mounting holes (3.2mm NPTH)
- ~58 cm³ ≈ 72 g of PLA at full infill

FreeCAD + STEP + STL:
`hardware/case/inSomnia_case.FCStd` / `inSomnia_case_shell.step` / `inSomnia_case_shell.stl` / `inSomnia_base_plate.step`
Case views: `images/case-iso.png` `images/case-front.png` `images/case-top.png`

The FreeCAD model now embeds the PCB placement so the board sits on the bosses; the Blender render imports the STL directly so they cannot drift (replaced earlier hand-typed dimensions).

Known cosmetic issue: the keypad sits right-of-centre because that is where it is on the PCB. Fixing it means re-laying-out the board; noted for v2.

- 53 components + 4× M2 mounting holes, 41 nets, **schematic ERC clean (0 violations)**
- 100 × 80 mm two-layer board — **DRC clean, 0 violations, 0 unconnected items**
- 737 tracks, 141 vias, GND pour both layers, antenna keepout enforced
- Passives 0805, through-hole tactile buttons and headers — hand-solderable
- Gerbers + drill ready to order: `hardware/inSomnia_gerbers.zip`

Sensor modules (mic, thermal, display) mount on headers rather than being soldered down, so they can be positioned without a bed wire.

## Firmware

PlatformIO + Arduino-ESP32, pinned to `espressif32@6.5.0`.

```bash
cd firmware
pio run              # build
pio run -t upload    # flash
pio device monitor   # serial 115200 — shows [epoch] counts + state + [alarm] transitions
```

Footprint: **RAM 20.1%, Flash 13.6%** of a 16 MB part — lots of headroom. Verified 2026-09-17.

### First boot

No WiFi credentials are stored, so the device opens an access point called **`inSomnia-setup`** (password `insomnia`). Join it, open `http://192.168.4.1`, pick your network. It reboots and comes back at **`http://insomnia.local`**. Without WiFi it still runs on a fallback clock (time shows `*`) and will still alarm at the hard deadline — it never fails silent.

The web UI gives you the live clock and sleep state, tonight's alarm status, last night's report, live sensor readings (thermal heatmap 32×24, mic RMS/dBFS), and every threshold as an editable setting. Everything stays on your own network — no audio ever leaves the device.

### Layout

```
firmware/
  include/ src/
    main.cpp          loop, epoch tick (60s), display render, 12-key handling
    settings.*        NVS-persisted config (window, thresholds, debt)
    net.*             WiFi STA + setup AP, NTP, mDNS, fallback clock
    web_ui.*          HTTP server, dashboard, JSON API (/api/state, /api/thermal, /api/settings)
    display.*         ST7735 128x160 SPI via GPIO matrix
    buzzer.*          LEDC tone + volume (gentle ramp → hard)
    key_matrix.*      74HC165 chain, debounce, edge latch (12 keys)
    thermal.*         MLX90640 -> motion index, in-bed detection, centroid
    mic.*             INMP441 I2S -> RMS / dBFS / loud fraction per epoch
    sleep_model.*     Cole-Kripke + causal 3-state classifier (asleep/stirring/awake)
    sleep_debt.*      rolling 14-night debt, recency-weighted
    alarm.*           window, suppression, escalation, debt shift, failsafe, snooze
```

Debug mode: serial log each epoch + web *Live* cards show raw `counts`, `warm pixels`, `motion index`, `RMS`, `state`; thresholds are live-editable.

## PCB

`hardware/inSomnia.kicad_pcb` — **100 × 80 mm**, 2 layer, rounded 3 mm corners.
**DRC clean: 0 violations, 0 unconnected items.**

| | |
|---|---|
| Components | 53 (+4 holes), zero courtyard overlaps |
| Tracks | 737 segments @ 0.20 mm |
| Vias | 141 @ 0.30 mm drill / 0.60 mm pad |
| Copper | GND pour on both layers, islands auto-removed |
| Clearance | 0.20 mm |
| Mounting | 4× 3.2mm NPTH at corners → M2 bosses in case |

Fabrication files are in `fab/`, zipped as `inSomnia_gerbers.zip` (Gerbers + Excellon drill + drill map + BOM).

### Layout

- **Top-left** — ESP32-S3 module, antenna pointing off the top edge
- **Top-right** — sensor and display headers in a row (J3 TFT, J4 mic, J5 thermal, J6 spare, J7 USB)
- **Right** — 4 × 3 keypad on a 11 × 12 mm grid
- **Left-centre** — 74HC165 chain and the 12 key pull-ups
- **Bottom-left** — 5 V in, regulator, bulk caps
- **Bottom-centre** — buzzer and its transistor driver

### Two things to know before ordering

**1. The antenna keepout is intentional.** A rule area at x 2–50 mm, y 0–6.25 mm keeps copper, tracks and vias out from under the module's antenna, and the courtyard deliberately overhangs the top board edge. DRC flags that overhang as a boundary violation — that is correct RF practice, not a mistake. Verified: no track or via crosses the keepout.

**2. There are twelve 0.20 mm holes, and they are not mine.** Every via I placed is 0.30 mm. The 0.20 mm holes are the thermal vias built into the stock `RF_Module:ESP32-S3-WROOM-1` footprint, under the module's ground pad. Many budget fab processes have a **0.30 mm minimum drill**, so this may trigger an upcharge or a rejection. Two options:

- Order from a fab that allows 0.20 mm (costs a little more), or
- Delete the twelve pad-41 thermal vias — fine for a hand-built prototype, since the module's ground still connects through the main pad and the GND pour.

The board has **not been fabricated or bench-tested**. It is DRC-clean, which means the geometry is self-consistent — not that the design is proven. Sanity-checked in `#forge` and via KiCad/FreeCAD MCP.

## Known gaps

- No trained sound-event model yet (see honesty section).
- Cole-Kripke `countScale` ships with a placeholder default and needs per-bed tuning.
- The PCB has not been fabricated or bench-tested; it is DRC-checked, not proven.
- The WROOM footprint contributes twelve 0.20 mm thermal vias; some budget fabs require 0.30 mm minimum. See `hardware/HARDWARE.md` for the two ways round it.
- No physical build yet — next step is printing the case and soldering the board, then adding build photos + demo video for the "shipping" submission.

## References

- [Comparison and Validation of Actigraphy Algorithms Using a Large Community Dataset (JMIR Formative Research, 2025)](https://formative.jmir.org/2025/1/e70778)
- [Actigraphy-based sleep estimation in adolescents and adults (PubMed)](https://pubmed.ncbi.nlm.nih.gov/29403321/)
- [Prediction of Sleep Stages Via Deep Learning Using Smartphone Audio Recordings — HomeSleepNet (JMIR, 2023)](https://www.jmir.org/2023/1/e46216)
- [End-to-End Sleep Staging Using Nocturnal Sounds (Nature and Science of Sleep, 2022)](https://www.dovepress.com/end-to-end-sleep-staging-using-nocturnal-sounds-from-microphone-chips--peer-reviewed-fulltext-article-NSS)
- [Accuracy of 11 Wearable, Nearable, and Airable Consumer Sleep Trackers (JMIR mHealth, 2023)](https://mhealth.jmir.org/2023/1/e50983)
- [Video-Based Actigraphy for Monitoring Wake and Sleep (Sensors, 2019)](https://doi.org/10.3390/s19051075)
- [A Blanket Accommodative Sleep Posture Classification System Using an Infrared Depth Camera (2021)](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC8402261/)
- [Human Occupancy Monitoring with an Infrared Thermal Array Sensor](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC11722904/)
- [I²C Cabling — bus capacitance and length limits (Analog Devices)](https://www.analog.com/en/technical-articles/i2c-cabling.html)
