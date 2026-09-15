# inSomnia

![inSomnia](images/render-hero.png)

An adaptive, context-aware wake clock. Instead of firing at a fixed timestamp,
it wakes you inside a window at a moment you are already stirring — and if you
got up before the window even opened, it stays quiet.

Built for [Hack Club Forge](https://forge.hackclub.com). Firmware-first,
fully offline, no cloud service.

---

## What it actually does

| Situation | Behaviour |
|---|---|
| You're already up before the window opens | Alarm is suppressed for the day and logged as "skipped" |
| You're carrying sleep debt | The gentle wake is held back by up to 15 min so an early stir doesn't cost you the lie-in. **The hard deadline never moves.** |
| You start stirring inside the window | Gentle wake begins immediately — soft tone ramping up over several minutes |
| You sleep right through to the hard deadline | Full-intensity alarm, no more waiting |
| A sensor dies or is unplugged | **Failsafe**: degrades to a plain fixed-time alarm at the deadline. It never fails silent. |

Dismiss with Key 1, snooze with Key 2, or from the web UI.

## How it decides

One sensor does most of the work:

- **MLX90640 32×24 thermal array**, on the clock, aimed at the bed.
  - *Frame-to-frame change* → a motion index. This is contactless video
    actigraphy, validated against polysomnography at **Cohen's κ = 0.733**,
    comparable to a wrist actigraph. It feeds Cole-Kripke.
  - *Warm body-sized region* → **in bed / out of bed**. Far stronger than a
    PIR, because it can tell you-in-the-bed from a warm object in the room.
  - No wire to the bed, and at 768 pixels you cannot identify a person from it.
- **INMP441 I2S microphone** → RMS level per epoch. A loudness hint that nudges
  the stirring threshold. It is a level detector, *not* a sound classifier.

An earlier version used a mattress accelerometer. It was dropped because it
needs 1–2 m of I2C cable from the nightstand to the bed, and I2C is spec'd to
400 pF (≈1 m at 100 kHz) against cable that runs 100–240 pF/m.

Two separate pieces of logic sit on top, because they answer different questions:

1. **A causal 3-state machine** (asleep / stirring / awake) with hysteresis.
   It may only look backwards, because it drives the alarm in real time.
   This is a threshold heuristic — see the honesty section below.
2. **Cole-Kripke (1992)** for the morning report. It needs the two *following*
   epochs, so it lags 2 minutes and can never drive a live decision.

## Being honest about the "AI"

This matters more than the feature list, so it goes near the top.

**The on-device logic is not AI.** Cole-Kripke is a weighted moving average with
a threshold, published in 1992. The 3-state classifier is thresholds plus
hysteresis. Calling either one "AI" would be marketing, not engineering.

**Accuracy ceiling.** Actigraphy sleep/wake scoring agrees with clinical
polysomnography roughly **78–80%** of the time. But its specificity for *wake*
is only **0.35–0.64** — it is much better at spotting sleep than wakefulness.
Sound-based staging is no better: the best published smartphone-audio model
(HomeSleepNet) gets 76.2% overall and **63.4% on wake epochs**.

That single fact shaped the design. Alarm suppression is the feature most
exposed to a wrong "you're awake" call, so it is **not** driven by the sleep
classifier. It is driven by `outOfBed()` — mattress quiet *and* room motion —
which is close to a binary measurement rather than a 64%-accurate inference.

**This is not clinical sleep tracking.** Without EEG there is no true sleep
staging. Consumer trackers systematically overestimate sleep and underestimate
wake-after-sleep-onset, worst of all for people who sleep badly. The numbers in
the app are estimates and are labelled as such.

**One number you must calibrate.** Cole-Kripke's weights assume ActiGraph
counts, a proprietary unit. Ours are arbitrary units from your own mattress, so
`countScale` in Settings divides ours into that range. The *algorithm* is
validated; this *scaling* is not. Watch epoch counts overnight and set it so a
quiet epoch lands near zero.

**Posture is not claimed.** The thermal array reports *movement and position*.
Classifying sleeping posture under a duvet at 32×24 is a research problem — the
published work that does it used an infrared **depth** camera with synthetic
blanket augmentation. Movement and position are what the sleep model needs, so
that is what is built and that is what it says.

**Sleep debt is an estimate built on an estimate.** Debt is
`max(0, need − actual)` summed over a rolling 14 nights, which is the window
research finds matters for current impairment (Van Dongen 2003: 6 h/night for
14 days ≈ two nights of total deprivation). But "actual" comes from Cole-Kripke
scoring that under-reports wake — so read it as a trend, not a clinical figure.

**No trained model ships yet.** A learned audio or thermal classifier is the
obvious next step, but a model needs labelled data and there is no
polysomnography ground truth here to validate against. The honest path is to
ship the validated heuristic, collect feature data, and only then train.

---

## Hardware

**ESP32-S3-WROOM-1-N16R8** — WiFi + BLE 5, native I2S, 8 MB PSRAM, SIMD for
future MFCC work. Chosen over the ESP32-C3 because the C3 has no PSRAM and too
few GPIO once an I2S microphone is added.

Schematic, pin map, BOM and wiring notes: **[`hardware/HARDWARE.md`](hardware/HARDWARE.md)**
Rendered schematic: `hardware/inSomnia_schematic.pdf`

> The hero image above is a **concept render** of the intended enclosure
> (Blender/Eevee), not a photo — nothing has been fabricated yet. The board
> renders in `images/` are generated from the actual KiCad files.

- 53 components, 41 nets, **schematic ERC clean (0 violations)**
- 100 × 80 mm two-layer board — **DRC clean, 0 violations, 0 unconnected items**
- 737 tracks, 141 vias, GND pour both layers, antenna keepout enforced
- Passives 0805, through-hole tactile buttons and headers — hand-solderable
- Gerbers + drill ready to order: `hardware/inSomnia_gerbers.zip`

Sensor modules (mic, accelerometer, PIR) and the display mount on headers rather
than being soldered down, so they can be positioned around the bed.

## Firmware

PlatformIO + Arduino-ESP32, pinned to `espressif32@6.5.0`.

```bash
cd firmware
pio run              # build
pio run -t upload    # flash
pio device monitor   # serial
```

Footprint: **RAM 15.8%, Flash 13.2%** of a 16 MB part — lots of headroom.

### First boot

No WiFi credentials are stored, so the device opens an access point called
**`inSomnia-setup`** (password `insomnia`). Join it, open `http://192.168.4.1`,
pick your network. It reboots and comes back at **`http://insomnia.local`**.

The web UI gives you the live clock and sleep state, tonight's alarm status,
last night's report, live sensor readings, and every threshold as an editable
setting. Everything stays on your own network — no audio ever leaves the device.

### Layout

```
firmware/
  include/ src/
    main.cpp          loop, epoch tick, display render
    settings.*        NVS-persisted config
    net.*             WiFi STA + setup AP, NTP, mDNS
    web_ui.*          HTTP server, dashboard, JSON API
    display.*         ST7735 128x160
    buzzer.*          LEDC tone + volume
    key_matrix.*      74HC165 chain, debounce, edge latch
    pir_sensor.*      rolling duty cycle
    thermal.*         MLX90640 -> motion index, in-bed detection
    mic.*             INMP441 I2S -> RMS / dBFS
    sleep_model.*     Cole-Kripke + causal 3-state classifier
    sleep_debt.*      rolling 14-night debt, recency-weighted
    alarm.*           window, suppression, escalation, debt shift, failsafe
```

## Known gaps

- No trained sound-event model yet (see honesty section).
- Cole-Kripke `countScale` ships with a placeholder default and needs per-bed tuning.
- The PCB has not been fabricated or bench-tested; it is DRC-checked, not proven.
- The WROOM footprint contributes twelve 0.20 mm thermal vias; some budget fabs
  require 0.30 mm minimum. See `hardware/HARDWARE.md` for the two ways round it.
- Enclosure is out of scope.

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
