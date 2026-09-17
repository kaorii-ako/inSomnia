# inSomnia — hardware reference

MCU: **ESP32-S3-WROOM-1-N16R8** (16 MB flash, 8 MB octal PSRAM)
Schematic: `inSomnia.kicad_sch` — ERC clean, 0 violations. PDF: `inSomnia_schematic.pdf`. BOM: `inSomnia_bom.csv`.

## Pin map

| GPIO | Net | Goes to |
|------|-----|---------|
| IO1  | I2C_SDA    | J5 MLX90640 SDA (4.7k pull-up to 3V3) |
| IO2  | I2C_SCL    | J5 MLX90640 SCL (4.7k pull-up to 3V3) |
| IO4  | I2S_BCLK   | J4 INMP441 SCK |
| IO5  | I2S_WS     | J4 INMP441 WS |
| IO6  | I2S_DIN    | J4 INMP441 SD |
| IO7  | THERM_INT  | J5 spare pin (MLX90640 needs no interrupt) |
| IO8  | TFT_RST    | J3 TFT RES |
| IO9  | TFT_DC     | J3 TFT DC |
| IO10 | TFT_CS     | J3 TFT CS |
| IO11 | TFT_MOSI   | J3 TFT SDA |
| IO12 | TFT_SCK    | J3 TFT SCK |
| IO13 | TFT_BL     | J3 TFT BLK (PWM, LEDC ch2) |
| IO14 | AUX_GPIO   | J6 spare 3-pin header |
| IO15 | BUZZER_DRV | R5 1k -> Q1 base (LEDC ch0) |
| IO16 | SHIFT_LOAD | U2+U3 pin 1 (SH/LD) |
| IO17 | SHIFT_CLK  | U2+U3 pin 2 (CLK) |
| IO18 | SHIFT_DATA | U3 pin 9 (Q7) |
| IO19/20 | USB_DM / USB_DP | J7 (native USB, optional) |
| RXD0/TXD0 | UART_RX / UART_TX | J2 programming header |

**Never assign** on the N16R8: `IO35 / IO36 / IO37` (octal PSRAM inside the module),
`IO0 / IO3 / IO45 / IO46` (boot straps; IO46 is input-only). All no-connect on the schematic.

## Power

`J1` 5 V in -> `U4` AMS1117-3.3 -> 3V3 rail.
The PIR runs from **+5 V** (an HC-SR501 will not trigger reliably at 3.3 V); its output
swings 0/3.3 V, so it is safe direct to IO14 with no level shifting.

## Keypad — what the 12 keys do

The board carries 12 keys because the original schematic had 12. For a while
the firmware used exactly two of them. All twelve now do something; the layout
matches the enclosure, front row first.

| | col 1 | col 2 | col 3 | col 4 |
|---|---|---|---|---|
| **row 1** (front) | `DISMISS` | `SNOOZE` | `LIGHT` | `SKIP` |
| **row 2** | `OPEN −` | `OPEN +` | `DEAD −` | `DEAD +` |
| **row 3** (back) | `PAGE` | `SOFT` | `LOUD` | `SAVE` |

Row 1 is what you hit half-asleep: stop it, delay it, dim the display, or tell
it not to bother tonight. Rows 2 and 3 are setup you do awake — nudge the wake
window and the hard deadline in 5-minute steps, page through status / window /
sleep-debt screens, fire a test alarm, and commit settings to flash.

`SKIP` is worth calling out: it is the manual version of context-aware silence.
The automatic path needs the thermal array to be confident you are out of bed;
this is you saying so directly.

![keypad legend](../images/keymap.png)

## Keypad wiring

Active **LOW**: each key has a 10k pull-up to 3V3 and its switch shorts to GND.

Chain is `U2.Q7 -> U3.DS`, `U3.Q7 -> SHIFT_DATA`. U3 is clocked out first, and a
74HC165 presents D7 first, so the firmware bit order is:

- `bit 0..7`  = U3 D7..D0 = Key 1..8
- `bit 8..11` = U2 D7..D4 = Key 9..12
- U2 D3..D0 tied to 3V3 (spare inputs, always read "not pressed")

## Sensors and what each is for

| Part | Role | Honest note |
|------|------|-------------|
| **MLX90640 (J5)** | 32x24 thermal array on the clock, aimed at the bed. Frame differencing -> contactless actigraphy; warm-blob detection -> in-bed / out-of-bed. | Replaces both the accelerometer and the PIR. Reports **movement and position, not posture**. |
| INMP441 (J4) | I2S audio level per epoch; secondary stirring hint | A level detector, not a sound classifier. |
| J6 | Spare 3-pin header (+5V / IO14 / GND) | Was the PIR. Left populated for expansion. |

**Why a thermal array instead of an accelerometer on the bed.** The original design
put a LIS3DH under the mattress, which meant running I2C 1-2 m from the nightstand to
the bed. I2C is spec'd to 400 pF of bus capacitance — about **1 m at 100 kHz** — and
typical cable runs 100-240 pF/m. That is a "fine on the bench, flaky at 3 am" design.
The thermal array sits on the clock and needs no wire to the bed at all.

**Why it is not a downgrade.** Video actigraphy has been validated against
polysomnography at **Cohen's kappa 0.733**, comparable to a wrist actigraph. A duvet
attenuates the thermal signature but does not block it — heat builds under the cover
and the head is usually exposed — so presence and gross movement both survive.

**Why the thermal array drives suppression, not the sleep model:** validated actigraphy
runs ~78-80% for sleep/wake but only **0.35-0.64 specificity on wake**, and sound-based
staging is similar (HomeSleepNet: 76.2% overall, 63.4% on wake). Both are weakest at
exactly the thing "you're already up, stay quiet" needs. "Is there a warm body-sized
mass in the bed?" is close to a direct measurement, so suppression hangs off that.

**Privacy:** 768 pixels of temperature, roughly 1000x coarser than a phone photo.
You cannot identify a person from it, and no image ever leaves the device.

None of this is clinical. Without EEG there is no true sleep staging, only an estimate.

---

## PCB

`inSomnia.kicad_pcb` — **100 × 80 mm**, 2 layer, rounded 3 mm corners.
**DRC clean: 0 violations, 0 unconnected items.** Re-verified 2026-09-17 via KiCad MCP.

| | |
|---|---|
| Components | 53, zero courtyard overlaps |
| Tracks | 737 segments @ 0.20 mm |
| Vias | 141 @ 0.30 mm drill / 0.60 mm pad |
| Copper | GND pour on both layers, islands auto-removed |
| Clearance | 0.20 mm |
| Mounting | 100×80 board sits in 101×81 mm case cavity (0.5 mm clearance) + perimeter lip. 4× M2 bosses in the case close with the base plate (M2×8) — no PCB drill holes needed, so DRC stays clean. Add 4× 3.2 mm NPTH at (5,5) etc if you prefer through-screws. |

Fabrication files are in `fab/`, zipped as `inSomnia_gerbers.zip`
(Gerbers + Excellon drill + drill map + BOM). Also: `images/board-3d-top.png`, `images/board-3d-bottom.png` (renders from the KiCad 3D viewer), `images/schematic.png`.

### Layout

- **Top-left** — ESP32-S3 module, antenna pointing off the top edge (keepout enforced)
- **Top-right** — sensor and display headers in a row (J3 TFT, J4 mic, J5 thermal, J6 spare, J7 USB)
- **Right** — 4 × 3 keypad on a 11 × 12 mm grid
- **Left-centre** — 74HC165 chain and the 12 key pull-ups
- **Bottom-left** — 5 V in, regulator, bulk caps
- **Bottom-centre** — buzzer and its transistor driver

Wiring diagram: `images/schematic.png` + `hardware/inSomnia_schematic.pdf` — all headers are 2.54mm vertical, see pin map above for host-side GPIOs. No wiring harness to the bed (thermal array is on the clock).

### Two things to know before ordering

**1. The antenna keepout is intentional.** A rule area at x 2–50 mm, y 0–6.25 mm
keeps copper, tracks and vias out from under the module's antenna, and the
courtyard deliberately overhangs the top board edge. DRC flags that overhang as
a boundary violation — that is correct RF practice, not a mistake. Verified: no
track or via crosses the keepout.

**2. There are twelve 0.20 mm holes, and they are not mine.** Every via I placed
is 0.30 mm. The 0.20 mm holes are the thermal vias built into the stock
`RF_Module:ESP32-S3-WROOM-1` footprint, under the module's ground pad. Many
budget fab processes have a **0.30 mm minimum drill**, so this may trigger an
upcharge or a rejection. Two options:

- Order from a fab that allows 0.20 mm (costs a little more), or
- Delete the twelve pad-41 thermal vias — fine for a hand-built prototype, since
  the module's ground still connects through the main pad and the GND pour.

The board has **not been fabricated or bench-tested**. It is DRC-clean, which
means the geometry is self-consistent — not that the design is proven.
