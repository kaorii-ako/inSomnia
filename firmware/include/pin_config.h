#pragma once

// inSomnia pin map — ESP32-S3-WROOM-1-N16R8
// Matches hardware/inSomnia.kicad_sch (ERC clean).
//
// RESERVED, never assign on the N16R8 part:
//   IO35 / IO36 / IO37  -> octal PSRAM inside the module
//   IO0 / IO3 / IO45 / IO46 -> boot straps (IO46 is also input-only)
//   IO19 / IO20 -> native USB D-/D+
// All of the above are left as no-connect on the schematic.

// ── TFT ST7735 128x160 (SPI, remapped via the GPIO matrix) ────────
#define TFT_SCK     12
#define TFT_MOSI    11
#define TFT_CS      10
#define TFT_DC      9
#define TFT_RST     8
#define TFT_BL      13

// ── INMP441 I2S MEMS microphone ──────────────────────────────────
#define I2S_BCLK    4
#define I2S_WS      5
#define I2S_DIN     6
#define I2S_PORT    I2S_NUM_0

// ── MLX90640 32x24 thermal array (I2C) ───────────────────────────
// Sits on the clock, aimed at the bed. Replaces both the mattress
// accelerometer and the PIR: it gives contactless actigraphy AND a direct
// in-bed / out-of-bed measurement, with no wire running to the bed.
#define I2C_SDA       1
#define I2C_SCL       2
#define THERM_INT     7    // spare, MLX90640 breakout does not need it
#define MLX90640_ADDR 0x33

// ── Spare (was PIR; the thermal array supersedes it) ─────────────
#define AUX_GPIO    14

// ── Buzzer (via MMBT3904 NPN, active high) ───────────────────────
#define BUZZER_PIN  15

// ── 74HC165 keypad shift chain ───────────────────────────────────
#define SHIFT_LOAD  16
#define SHIFT_CLK   17
#define SHIFT_DATA  18

// ── Panel geometry ───────────────────────────────────────────────
#define TFT_PANEL_W     128
#define TFT_PANEL_H     160
#define TFT_ROTATION    1     // landscape -> 160 wide x 128 tall

// ── LEDC ─────────────────────────────────────────────────────────
// timer = (channel / 2) % 4, so channels 0 and 1 share timer 0.
// Backlight must not share a timer with the buzzer.
#define BUZZER_CHANNEL       0   // timer 0
#define BACKLIGHT_CHANNEL    2   // timer 1
#define BUZZER_RESOLUTION    8
#define BUZZER_IDLE_FREQ     2000
#define BACKLIGHT_FREQ       5000
#define BACKLIGHT_RESOLUTION 8

// ── Keypad ───────────────────────────────────────────────────────
// Chain: U2.Q7 -> U3.DS, U3.Q7 -> SHIFT_DATA. U3 is clocked out first,
// and the 74HC165 presents D7 first, so:
//   bit0..7  = U3 D7..D0 = Key 1..8
//   bit8..11 = U2 D7..D4 = Key 9..12
#define NUM_SHIFT_REGS   2
#define NUM_KEY_BITS     (NUM_SHIFT_REGS * 8)
#define NUM_KEYS         12
#define KEY_ACTIVE_LOW   1
#define KEY_DEBOUNCE_MS  25

// ── PIR sampling (Phase 1 proxy; Phase 3 replaces the classifier) ──
#define PIR_SAMPLE_INTERVAL_MS  1000
#define PIR_WINDOW_SAMPLES      300
