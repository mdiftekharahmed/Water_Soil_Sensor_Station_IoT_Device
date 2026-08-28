# Incremental Bring-Up Plan — one peripheral at a time

| | |
|---|---|
| **Project** | Sundarbans Water + Soil Monitoring Station |
| **Document** | Bench integration plan, Phase A |
| **Version** | v1.0 — 2026-08-28 |
| **Board** | ESP32 **ESP-32S 30-pin NodeMCU** development board (WROOM-32 class) — confirmed by white 2026-08-28 |
| **Integration order** | **Sensors first, modem last** (decision **D-11**) |
| **Audience** | IoT engineering team |
| **Companion docs** | `README.md` **v1.1** — pin map §4.2, circuit blocks §4.5, test IDs §6, defect register §7, previous-build lessons §11. **This plan sequences that work; it does not replace it.** |

---

## 0. How to use this document

One peripheral per step. Each step is a **closed loop**: wire it, flash a sketch that exercises only that peripheral, measure the numbers, tick the pass criteria, commit, *then* move on. The entire value of working this way is that when something breaks you already know what caused it — so the rule that matters most is the boring one:

> **Never add two things between two working states.** If a step fails, the cause is inside that step. If you added two things, you have just bought yourself an afternoon of bisecting.

Three habits make that guarantee real:

1. **One sketch per step**, named for the step, kept in the repo. `bringup/step03_ultrasonic.ino` still compiling in six months is what lets you go back and prove a fault is new.
2. **Commit and tag at every green step** (`git tag bringup-step-03`). That tag is your rollback point. A step that never got a tag was never actually verified.
3. **Write the measured number, not a tick.** "Works" is not data. `README.md` §6 has a column for the number; use it.

### 0.1 Board identity — the 60-second check you still owe

You've confirmed a **30-pin ESP-32S NodeMCU**, which is what `README.md` §4.2 is derived for, so the pin map below stands. One residual check remains before Step 2, and it is cheap:

- [ ] Read the **metal can marking** on the module. You want `ESP32-WROOM-32`, `-32D` or `-32E`. If it reads **`ESP32-WROVER`**, GPIO16 and GPIO17 are consumed by the PSRAM and the modem UART assignment fails *silently* — you get a modem that never answers `AT` and no clue why. That is **I-34**; remap UART2 and update §4.2 if it applies.
- [ ] Count **15 pins per side** and confirm **one micro-USB socket**. (The board in your earlier build photos appeared to have two USB receptacles — if that board is the one on the bench, it is not this one, and the map below does not apply.)

Everything after this assumes a WROOM-32 on 30 pins.

---

## 1. Pin distribution

### 1.1 What the 30-pin board actually gives you

A 30-pin ESP-32S NodeMCU breaks out **25–26 GPIO** — vendor variants differ over whether GPIO0 reaches a header, which is why the count is a range and not a number. It does not matter, because GPIO0 is a strapping pin you must not use anyway. Of what is exposed, four pins are strapping, two are the serial console, and four are input-only:

| Class | Pins | Count | Verdict |
|---|---|---|---|
| General-purpose I/O | 4, 5, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33 | 16 | Usable (GPIO5 needs a pull-up — see §1.3) |
| **Input-only**, no pull-up/pull-down | 34, 35, 36 (VP), 39 (VN) | 4 | Analog inputs and passive status lines only |
| UART0 console | 1 (TX0), 3 (RX0) | 2 | **Reserved** — this is your field debug port |
| Boot strapping | 2, 12, 15, and 0 if your board exposes it | 3–4 | **Do not use** |
| Internal SPI flash | 6, 7, 8, 9, 10, 11 | 6 | Not broken out on any variant |

> **Do not transcribe a pinout diagram off the internet.** The two-column silkscreen order differs between vendors selling the same "30P ESP-32S" — where 3V3, EN and VIN sit is not standardised. Read *your* board's silkscreen, and use the labels, not the physical positions. This plan therefore specifies pins **logically** (GPIO number → net) and leaves the physical map to your own board.

### 1.2 Final allocation

This is `README.md` §4.2, re-sorted into the order you will actually wire it. **Step** is the step in §2 that claims the pin.

| Step | GPIO | Net | Dir | Peripheral | Conditioning — build this *before* connecting |
|---|---|---|---|---|---|
| 2 | 18 | `SD_SCK` | out | microSD | none |
| 2 | 19 | `SD_MISO` | in | microSD | 47 kΩ pull-up to 3V3 |
| 2 | 23 | `SD_MOSI` | out | microSD | none |
| 2 | **5** | `SD_CS` | out | microSD | **10 kΩ pull-up to 3V3 — mandatory** (strap, must be HIGH at reset) |
| 3 | 25 | `US_TRIG` | out | JSN-SR04T | none, 10 µs pulse |
| 3 | 33 | `US_ECHO` | in | JSN-SR04T | none **if the sensor runs on 3V3**; 10 k/15 k divider if on 5 V (§4.5.8) |
| 4 | **34** | `TURB_ADC` | in only | Turbidity | **10 kΩ/20 kΩ divider + 1 kΩ/100 nF RC** (§4.5.4) |
| 5 | **35** | `HK_VBAT_ADC` | in only | Battery sense | **100 kΩ/27 kΩ + 100 nF** (§4.5.7) |
| 5 | **36** (VP) | `HK_VSOL_ADC` | in only | Solar sense | **220 kΩ/33 kΩ + 100 nF** — PV *Voc* is ~21–22 V (I-28) |
| 6 | 27 | `RS485_RO` | in | MAX485 | **10 kΩ/15 kΩ divider if the MAX485 runs at 5 V** (§4.5.3, I-06) |
| 6 | 14 | `RS485_DI` | out | MAX485 | none |
| 6 | 26 | `RS485_DE_RE` | out | MAX485 | none — DE and RE tied together, HIGH = transmit |
| 7 | 32 | `SENS_EN` | out | +12 V rail gate | none — drives 2N7002 → P-MOS, HIGH = rail ON |
| 8 | 21 | `I2C_SDA` | bidir | DS3231, SHT31 | 4.7 kΩ pull-up to 3V3 |
| 8 | 22 | `I2C_SCL` | out | DS3231, SHT31 | 4.7 kΩ pull-up to 3V3 |
| 9 | 16 | `SIM_TXD_IN` | in | SIM800L TXD | none — the modem's ~2.8 V reads HIGH on the ESP32 |
| 9 | 17 | `SIM_RXD_OUT` | out | SIM800L RXD | **1.8 kΩ/10 kΩ divider → 2.80 V** (§4.5.2) |
| 9 | 4 | `SIM_PWRKEY` | out | SIM800L | none — pulse LOW ~1 s |
| 9 | 13 | `SIM_RST` | out | SIM800L | none |
| 9 | **39** (VN) | `SIM_STATUS` | in only | SIM800L STATUS | none, but **no internal pull-up on this pin** |
| — | 1, 3 | `UART0` | — | Serial console | **Leave free** |
| — | 2, 12, 15 (+ 0) | — | — | — | **Do not use — strapping pins** |

### 1.3 The pin budget — and why there is no slack

**20 GPIO claimed. 2 reserved for the console. Everything still unclaimed is a strapping pin.**

That is the whole finding: 16 general-purpose pins exist and all 16 are spoken for, all 4 input-only pins are spoken for, and the only pins left over are GPIO2, GPIO12, GPIO15 (and GPIO0 if your variant exposes it) — every one of which is sampled at reset and must be left at its default level. **There is no free general-purpose pin.**

That is worth knowing before you get attached to any late additions. There is no pin left for a status LED, a service push-button, a reed switch, a second analog channel or a tamper sensor — not without giving something up. If you want one later, the honest trades are:

| If you need a pin | Give up | Cost of the trade |
|---|---|---|
| 1 pin | `SIM_RST` (GPIO13) | PWRKEY alone can power-cycle the modem; RST is belt-and-braces. **Cheapest trade — take this one first.** |
| 1 pin | `SIM_STATUS` (GPIO39) | You lose hardware confirmation the modem is alive and must infer it from AT timeouts. Acceptable, mildly annoying to debug. |
| 2 pins | I²C (GPIO21/22) | Drops DS3231 and SHT31 — but D-8 says fit both, and both exist to make field faults diagnosable. **Do not trade these away.** |
| 2 pins | Ultrasonic Trig/Echo → UART mode | Frees nothing net (UART needs 2 pins too), but see D-6: UART is more robust over a long cable, so this is a wash on pins and a win on reliability. |

If you genuinely need several spare pins, the answer is not a trade — it is a **PCF8574 I²C expander** on the pins you already reserved at Step 8, or the bare **WROOM-32E** in Phase B, which exposes everything the devkit hides.

### 1.4 Three rules that will otherwise cost you a day

1. **Analog goes on ADC1 only** (GPIO32–39). ADC2 is unusable whenever WiFi is active. You are running cellular so it does not matter in the field, but it *will* eat a bench afternoon when a GPIO25 reading refuses to change.
2. **UART1's default pins (9/10) collide with the SPI flash.** Never call `Serial1.begin()` bare. Always remap:
   ```cpp
   HardwareSerial rs485(1);
   rs485.begin(9600, SERIAL_8N1, /*RX=*/27, /*TX=*/14);
   ```
3. **GPIO34/35/36/39 have no internal pull-up or pull-down.** Floating means noise, and noise on an ADC pin looks exactly like a failing sensor. Every one of them gets a divider or a resistor to a known level.

---

## 2. The integration sequence

Twelve steps. Steps 1–8 are sensors and storage on USB and bench power; the modem lands at Step 9; Step 10 exists specifically to pay for that ordering, and Step 11 is the whole-node proof.

**Order rationale (D-11).** You chose sensors-first. That is the right call for *debuggability* — every peripheral gets validated on a clean, quiet 3.3 V rail with no radio present, so a failure is unambiguously the peripheral's fault. The cost is real and you should know its shape: the SIM800L draws **~2 A bursts every 4.6 ms** while transmitting, and that is exactly the disturbance that unmasks a marginal supply, an under-decoupled ADC, or an SD card that writes fine until the rail dips. **Nothing validated in Steps 1–8 stays validated once the modem is in.** Step 10 re-tests all of it under GSM load, and Step 10 is not optional.

### Step 0 — Bench setup

Nothing is wired yet. This step exists so the later steps can actually be measured rather than guessed at.

- [ ] Toolchain: Arduino IDE or PlatformIO, ESP32 core installed, board = **ESP32 Dev Module**, upload at 115200, flash 4 MB.
- [ ] Create `bringup/` in the repo. One sketch per step, plus `bringup/pins.h` holding **every** pin constant. Nothing after this hard-codes a GPIO number anywhere but `pins.h`.
- [ ] Bench supplies ready: USB cable, **12 V wall adapter ≥ 1 A** (Steps 6–7), one LM2596 free for the 4.10 V modem rail (Step 9).
- [ ] Instruments: DMM (mandatory), oscilloscope or logic analyser (needed properly at T3.1 and T7.7 — a DMM cannot see a 4.6 ms burst), µA-capable meter for sleep current.
- [ ] Do the §0.1 module-can check and write the marking into `README.md` §5.
- [ ] Measure the **panel open-circuit voltage in full sun** and record it (**I-28**) — this sets the Step 5 divider ratio, so do it before Step 5, not after.

`bringup/pins.h` — write this once, now:

```cpp
#pragma once
// microSD (Step 2)
#define PIN_SD_SCK    18
#define PIN_SD_MISO   19
#define PIN_SD_MOSI   23
#define PIN_SD_CS      5   // 10k pull-up to 3V3 MANDATORY (boot strap)
// Ultrasonic (Step 3)
#define PIN_US_TRIG   25
#define PIN_US_ECHO   33
// Analog (Steps 4-5) - ADC1 only
#define PIN_TURB_ADC  34
#define PIN_VBAT_ADC  35
#define PIN_VSOL_ADC  36
// RS-485 (Step 6)
#define PIN_485_RO    27
#define PIN_485_DI    14
#define PIN_485_DE_RE 26
// Sensor rail gate (Step 7)
#define PIN_SENS_EN   32
// I2C (Step 8)
#define PIN_I2C_SDA   21
#define PIN_I2C_SCL   22
// SIM800L (Step 9)
#define PIN_SIM_RX    16   // ESP32 receives modem TXD
#define PIN_SIM_TX    17   // via 1.8k/10k divider
#define PIN_SIM_PWRKEY 4
#define PIN_SIM_RST   13
#define PIN_SIM_STATUS 39  // input-only, no internal pull
```

**Exit:** toolchain uploads a blink sketch, `pins.h` committed, panel Voc recorded, module marking recorded.

---

### Step 1 — ESP32 alone

**Pins claimed:** none (1/3 console only) · **Cumulative: 0/26** · **Power:** USB · **Depends on:** Step 0

Prove the board before blaming anything else on it.

Wire nothing. Flash a sketch that prints the boot banner, chip revision, flash size, MAC address, then sleeps 30 s and wakes on the timer, incrementing an RTC-memory boot counter.

| Test | Measure | Pass |
|---|---|---|
| **T1.1** | Serial console at 115200 | Boot banner, **no `Brownout detector was triggered`** |
| **T1.2** | 30 s deep sleep, log wake reason | Wakes on `TIMER`, boot count increments across sleeps |
| — | Sleep current on the 5 V USB line, µA meter | Record the number. Expect **mA, not µA** — the devkit's regulator and USB-serial chip never sleep |

**The trap:** that sleep-current figure is the one that decides whether a 6 Ah battery lasts the wet season. A bare WROOM-32 sleeps at ~10 µA; a devkit typically lands **1000× higher**. Do not average it away or assume the datasheet. Write down the measured milliamps — it feeds the Phase-B decision about dropping the devkit for a bare module.

**Stop if:** brownout messages appear on USB. That is a cable or a PC port problem and it will masquerade as ten different sensor faults later.

---

### Step 2 — microSD (SPI)

**Pins claimed:** 18, 19, 23, 5 · **Cumulative: 4/26** · **Power:** USB · **Depends on:** Step 1

Storage first, before any sensor. Once this works you have somewhere to log every later step, which turns "it failed while I was at lunch" into evidence.

Wire: `SCK→18`, `MISO→19`, `MOSI→23`, `CS→5`, `GND→GND`. **47 kΩ from MISO to 3V3. 10 kΩ from CS (GPIO5) to 3V3.** Both resistors go in *before* first power-up.

Then decide the module's supply, and measure rather than assume: if the breakout has an on-board 3.3 V regulator and level shifter, feed it **5 V**; if it is a bare passive adapter, feed it **3.3 V**. Feeding a bare adapter 5 V puts 5 V logic on GPIO19.

| Test | Measure | Pass |
|---|---|---|
| **T2.1** | `SD.begin(PIN_SD_CS)` | Card type and size printed |
| **T2.2** | Append a row, power-cycle, read back | Row intact |
| **T2.3** | DMM on module VCC **and** on MISO while idle-high | **MISO ≤ 3.3 V** |

**The traps, in the order they bite:**
1. **GPIO5 without its 10 kΩ pull-up.** GPIO5 is a strapping pin sampled at reset; an SD card holding it low makes the ESP32 boot into the wrong mode, or not at all. The symptom is a board that "randomly stops working when the card is inserted" — this is **I-08**, and it is the single most common failure at this step.
2. **Module rail identity** (above). T2.3 is the check that catches it.
3. **SPI clock.** Start at 4 MHz. Cheap cards on long jumpers do not do 25 MHz.

**Stop if:** the card mounts only when you remove and reinsert it. That is contact or supply, not software — fix it now, because at Step 11 it becomes a lost dataset.

**Tag:** `bringup-step-02`

---

### Step 3 — Ultrasonic level (JSN-SR04T-V3.3)

**Pins claimed:** 25, 33 · **Cumulative: 6/26** · **Power:** USB (3.3 V) · **Depends on:** Step 2

Wire: `Trig→25`, `Echo→33`, `VCC→3V3`, `GND→GND`.

**Run it at 3.3 V, not 5 V.** The `-V3.3` suffix on your part means it is specified for it, and doing so removes the divider on the echo line entirely — one fewer part, one fewer thing to get wrong. If you ever move it to 5 V, the 10 k/15 k divider becomes mandatory (§4.5.8).

Sketch: 10 µs trigger pulse, `pulseIn` with a timeout, distance = `µs / 58`, temperature-compensated speed of sound, 5 readings with the median taken. Log to SD (you have that now).

| Test | Measure | Pass |
|---|---|---|
| **T5.1** | Tape measure at 0.3 / 1.0 / 2.0 / 4.0 m | Within **±2 cm** after temperature compensation |
| **T5.2** | Target at 10 cm | Invalid or clamped — firmware must **reject**, not report, a dead-zone reading |
| **T5.3** | DMM on echo during a ping | **≤ 3.3 V** |

**The traps:** the **20–25 cm dead zone** is physics, not a defect — the transducer is still ringing. Mount so the water surface never enters it. The **~60° beam** will bounce off a well wall and return early, reading high; aim it down a stilling well (**I-18**). And speed of sound drifts **~0.6 m/s per °C**, which is ~3 cm at 4 m across a 20 °C swing, so the compensation term is not optional — that is what the SHT31 at Step 8 is for.

**Stop if:** readings are stable on the bench but jump by tens of centimetres over water. That is beam geometry. Solve it with the well, not with more averaging.

**Tag:** `bringup-step-03`

---

### Step 4 — Turbidity (analog)

**Pins claimed:** 34 · **Cumulative: 7/26** · **Power:** USB 5 V for the module · **Depends on:** Step 3

**Build the divider on the bench and confirm it with a DMM before it ever touches GPIO34.** The module swings to ~4.5 V and the ESP32 pin is a hard 3.3 V maximum — this is **I-04**, and getting the order of operations wrong here damages the MCU rather than producing a wrong reading.

Wire: module `VCC→5 V`, `GND→GND`, `AOUT→` 10 kΩ → node → 20 kΩ → GND; from the node, 1 kΩ → `GPIO34`, with 100 nF from GPIO34 to GND.

Ratio check: 20/(10+20) = 0.667, so 4.5 V → **3.00 V**. Headroom is deliberate.

| Test | Measure | Pass |
|---|---|---|
| **T6.1** | DMM on `AOUT`, clear water vs stirred sediment | Clear ≈ **4.0–4.5 V**, muddy well below |
| **T6.2** | DMM on GPIO34, clear water | **≤ 3.15 V** |
| **T6.3** | 100 samples, still water | Spread **< 2 %** after averaging |

**The traps:** the response is **inverse and non-linear** — high voltage means clear water — so do not fit a straight line through two points and call it NTU. Take 4–5 points against a known standard or report raw volts and be honest about it. The module PCB is **not sealed**; only the optical head may be immersed. And ambient light through clear water shifts the reading, which is why §3.9 calls for a shrouded holder (**I-36**).

**Stop if:** clear water reads below 3.5 V at `AOUT`. The LED or photodiode is dirty or the supply is sagging — calibrating on top of that just bakes the fault into your curve.

**Tag:** `bringup-step-04`

---

### Step 5 — Housekeeping ADCs (battery + solar sense)

**Pins claimed:** 35, 36 · **Cumulative: 9/26** · **Power:** USB, plus bench supply for the divider inputs · **Depends on:** Step 4, and the panel Voc measurement from Step 0

These two channels are what let you diagnose a dead node from 200 km away instead of driving to it. Fit them even though nothing else needs them.

| Channel | Pin | Divider | Full-scale in | At the pin |
|---|---|---|---|---|
| `HK_VBAT` | 35 | 100 kΩ / 27 kΩ | 15 V | 3.19 V |
| `HK_VSOL` | 36 (VP) | **220 kΩ / 33 kΩ** | 22 V | 2.87 V |

100 nF from each pin to GND. **The PV divider must be 220 k/33 k, not 100 k/27 k** — a 12 V nominal panel sits at **21–22 V open-circuit** in full sun, and 22 V through a 100 k/27 k divider is 4.68 V into an input rated 3.3 V. That is **I-28**, it is a P0, and it destroys the pin silently.

Verify with a bench supply, not by reasoning: sweep the battery input 10 → 15 V and the PV input 0 → 22 V, and confirm the pin voltage tracks the ratio.

> **These two channels had no test IDs in `README.md` §6 when this plan was written — they do now, added as part of README v1.1.**

| Test | Measure | Pass |
|---|---|---|
| **T12.1** | Bench supply 10 → 15 V into `HK_VBAT` | Pin ≤ 3.19 V, reported volts within **±2 %** of the DMM after `esp_adc_cal` |
| **T12.2** | Bench supply 0 → 22 V into `HK_VSOL` | Pin ≤ 2.90 V, monotonic, no clipping |

**The traps:** the ESP32 ADC is **not linear near the rails** and varies part to part — use `esp_adc_cal` against the factory eFuse calibration, not a bare `analogRead()` scaled by 3.3/4095, or you will chase a 200 mV error that is not in your hardware. And these are input-only pins with **no internal pulls**, so if a divider leg is cracked the reading drifts rather than reading zero.

**Stop if:** either channel reads a plausible-looking voltage with the bench supply disconnected. Something is floating, and a floating housekeeping channel will tell you the battery is fine on the day it is flat.

**Tag:** `bringup-step-05`

---

### Step 6 — RS-485 transceiver + soil probe

**Pins claimed:** 27, 14, 26 · **Cumulative: 12/26** · **Power:** USB for logic, **12 V wall adapter (always on, not gated yet)** for the probe · **Depends on:** Step 5

This is the hardest step in the plan and the one with the most ways to half-work. Do it in two halves and do not merge them.

**6a — transceiver only, no probe.** Wire `RO→27` *through the divider*, `DI→14`, `DE+RE tied→26`. Your MAX485 module is a **5 V part** (**I-06**), so RO idles at 5 V: 10 kΩ from RO to a node, 15 kΩ from that node to GND, node → GPIO27. Ratio 15/25 = 0.6, so 5 V → **3.00 V**. Fail-safe bias: **560 Ω from A to +5 V and 560 Ω from B to GND**. Termination: **120 Ω across A–B at the board**, and a second 120 Ω at the far physical end of the bus — *the two ends only*, never one per device.

Firmware, and this is where the bug lives:

```cpp
HardwareSerial rs485(1);
rs485.begin(9600, SERIAL_8N1, PIN_485_RO, PIN_485_DI);  // never Serial1.begin() bare
// transmit: DE/RE HIGH, write, flush, THEN drop
digitalWrite(PIN_485_DE_RE, HIGH);
rs485.write(frame, len);
rs485.flush();                 // waits for the last bit to leave the shift register
digitalWrite(PIN_485_DE_RE, LOW);
```

| Test | Measure | Pass |
|---|---|---|
| **T3.1** | Scope DE/RE while transmitting | Clean transitions, no contention window |
| **T3.2** | DMM/scope on GPIO27, bus idle high | **≤ 3.3 V** — fails outright if the divider is missing |
| **T3.3** | USB-RS485 dongle on A/B, echo a string | String returns intact |
| **T3.4** | All probes unpowered, read the bus | Defined idle state, **no random framing errors** |

**6b — add the soil probe.** A/B to the probe, probe power from the 12 V adapter, **common ground with the ESP32**.

| Test | Measure | Pass |
|---|---|---|
| **T4.1** | Scan addresses 0x01–0x10 at **4800 and 9600** | Exactly one responds — record address **and** baud in §5 |
| **T4.2** | Function 0x03 across all documented registers | CRC valid, plausible values in moist soil |
| **T4.3** | Datasheet + test at 12 V | Operates on the gated 12 V rail |

**The traps:**
1. **`flush()` before dropping DE.** Without it you truncate your own last byte and the probe never answers. The symptom is 100 % timeouts with a perfectly correct frame on the scope.
2. **Address and baud are per-probe and undocumented.** You have **2 new probes and 1 used one** — the used one may have been re-addressed by whoever had it. Scan all three separately and label them physically.
3. **No common ground = no bus.** RS-485 is differential, not isolated. A/B without a shared reference works on the bench and fails in the field.
4. **Ribbon cable will not do** (**I-41**). Use shielded twisted pair, shield grounded at the enclosure end only.
5. Your N/P/K numbers are **relative trends only** in saline soil — EC-derived NPK is inflated by Na⁺/Cl⁻ (**I-29**). Label the columns accordingly now, so nobody downstream reads them as mg/kg.

**Stop if:** T3.4 shows framing errors with everything unpowered. The bias resistors are wrong or missing, and every intermittent Modbus fault you chase after this will trace back here.

**Tag:** `bringup-step-06`

> **Step 6c — deferred, parts not in hand.** The **water pH** and **water EC (K=10)** probes are still to be purchased. When they arrive they are a repeat of 6b with nothing new to learn: set them to **0x02** and **0x03**, hang them on the same bus, re-run T4.1/T4.2 per probe, re-check that termination is still only at the two physical ends. Budget a **12.88 mS/cm** standard as well as 1413 µS/cm for the K=10 cell.

---

### Step 7 — +12 V sensor rail gate (`SENS_EN`)

**Pins claimed:** 32 · **Cumulative: 13/26** · **Power:** 12 V adapter through the gate · **Depends on:** Step 6

Until now the probe has been permanently powered. This step makes it switchable, which is what makes the power budget close.

**Do not use the IRF520 module you bought.** It is not logic-level (V_GS(th) up to 4 V, so 3.3 V never fully enhances it) and it is a **low-side** switch, which would break the RS-485 ground reference. Build a **high-side P-channel** gate instead:

```
+12V ──┬─────── S │ AO3401 (P-MOS) │ D ───── +12V_SENS ──> probe
       │            G
       │            │
       └── 100kΩ ───┤          (gate pulled to source = OFF by default)
                    │
                    └── 10kΩ ── D │ 2N7002 (N-MOS) │ S ── GND
                                    G
                                    └── 1kΩ ── GPIO32
```

GPIO32 HIGH → 2N7002 on → gate pulled toward ground → P-MOS on → rail on. Defaults to **off** if the MCU is unpowered or the pin floats, which is the behaviour you want.

| Test | Measure | Pass |
|---|---|---|
| **T9.1** | GPIO32 HIGH, DMM on `+12V_SENS` | **≥ V_BATT − 0.3 V** |
| **T9.2** | GPIO32 LOW | **≤ 0.2 V**, probe current ≈ 0 |

**The traps:** allow a **settle delay** after switching on — these probes need roughly 1–2 s before the first Modbus read returns sane data, and reading too early gives plausible-looking garbage. Confirm the leakage in T9.2 with a current meter, not just a voltmeter: 2 mA of leak × 24 h is a real fraction of your daily budget. And re-run **T4.2** after gating, because the probe now sees a switched rail rather than a steady one.

**Stop if:** the rail only reaches ~10 V when on. The P-MOS is not fully enhanced — you have the wrong part or a wrong gate resistor, and the probe will behave erratically rather than fail cleanly.

**Tag:** `bringup-step-07`

---

### Step 8 — I²C: DS3231 RTC + SHT31 enclosure sensor

**Pins claimed:** 21, 22 · **Cumulative: 15/26** · **Power:** USB 3.3 V · **Depends on:** Step 7

> **⚠ Parts not in hand.** Neither the DS3231 nor the SHT31 has been purchased (**D-8**, ≈ 1,050 BDT for all three nodes). **This step does not block Step 9** — skip it, keep GPIO21/22 unwired, and come back. Everything else in the plan proceeds without it.

Both share the bus. 4.7 kΩ pull-ups on SDA and SCL to 3.3 V — **once, not per device.** Addresses: DS3231 at **0x68**, SHT31 at **0x44** (or 0x45 if its ADDR pin is strapped high).

| Test | Measure | Pass |
|---|---|---|
| **T13.1** | I²C scan | Exactly 0x68 and 0x44 respond, no ghosts |
| **T13.2** | Set RTC, remove USB for 10 min, re-read | Time correct — proves the coin cell and holder |
| **T13.3** | SHT31 vs a reference thermometer | Within ±1 °C; RH plausible |

**Why bother, given the modem gives you network time?** Because `AT+CCLK?` needs a registered network, and the failure mode you are designing against is *no network for three days*. Without an RTC, every row logged during an outage carries a wrong timestamp and the backfill at T10.2 is worthless. The SHT31 earns its place twice: it detects condensation inside the enclosure before it kills the board, and it supplies the temperature term for the ultrasonic compensation at T5.1.

**The trap:** many DS3231 modules ship wired to **trickle-charge** the backup cell. With a non-rechargeable CR2032 that is a slow leak and, in a sealed box in 40 °C heat, a hazard. Check for the charging resistor/diode and remove it, or fit a LIR2032.

**Tag:** `bringup-step-08`

---

### Step 9 — SIM800L modem (last)

**Pins claimed:** 16, 17, 4, 13, 39 · **Cumulative: 20/26 — full** · **Power:** dedicated **LM2596 at 4.10 V**, sourced from 12 V · **Depends on:** Steps 1–7

Everything else works. Now introduce the one part that can destabilise all of it.

**Build the power path first and measure it with nothing connected.** Set the LM2596 to **4.10 V** with a DMM on its output before the modem is anywhere near it — the SIM800L wants 3.4–4.4 V, and 5 V kills it immediately. Then fit **≥ 1000 µF low-ESR within 20 mm of the modem's VBAT/GND pads** (**I-02**, and lesson **L-7** from your previous build: the capacitor sitting near the modem on a breadboard is not the same as the capacitor sitting *at* the pads). Add 100 nF in parallel for the fast edge.

Wire: `SIM TXD → GPIO16` direct (~2.8 V reads HIGH on the ESP32). `GPIO17 → 1.8 kΩ → node → 10 kΩ → GND`, node → `SIM RXD`; that gives **2.80 V**, comfortably above the modem's V_IH and safely below its absolute max. `PWRKEY → GPIO4`, `RST → GPIO13`, `STATUS → GPIO39`. **Grounds must be short, thick, and star-connected to the buck output** — not daisy-chained through the sensor ground.

Two physical items before you power on: the **nano→micro SIM adapter** (the client's SIM is nano, the holder is micro), and the **external antenna on the SMA pigtail**. Do not attempt T7.2 on a bare spring antenna and then conclude the site has no coverage.

| Test | Measure | Pass |
|---|---|---|
| **T7.1** | 4.10 V rail, PWRKEY pulsed LOW ~1 s | `AT` → `OK` |
| **T7.2** | `AT+CSQ`, external antenna | **≥ 10**. Below 8, relocate the antenna |
| **T7.3** | `AT+CREG?` | `0,1` or `0,5` |
| **T7.4** | `AT+CCLK?` | Correct date/time, sane timezone |
| **T7.5** | APN from the build sheet | IP assigned |
| **T7.6** | Publish to a test topic | Received at the broker |
| **T7.7** | **50 consecutive publishes, scope on VBAT and V5** | VBAT sag **≤ 0.4 V**, V5 sag **≤ 0.25 V**, **0 ESP32 resets** |
| **T8.1** | `AT+CSCLK=2`, then wake | Enters and exits sleep; current drops |

**T7.7 is the test this whole ordering was built around.** A DMM cannot see a 4.6 ms trough — use the scope, on the actual VBAT pad, AC-coupled, and watch for the ESP32 resetting rather than just the voltage dipping.

**The traps:** the SIM800L is **2G only**, and Bangladesh's 2G sunset is a live commercial risk, not a technical one — flag it, don't engineer around it. `AT+CSQ` returning **99** means "unknown", not "excellent". A modem that answers `AT` but never registers is usually antenna or SIM seating, not firmware. And if the module can turns out to be a **WROVER**, GPIO16/17 do not exist as I/O and this step fails at T7.1 with no other symptom (**I-34** — which is why §0.1 comes first).

**Stop if:** the ESP32 resets during T7.6. Do not proceed to Step 10 and do not "fix" it in software. Add capacitance, shorten grounds, and re-run. A node that resets on every publish will look like a hundred different faults in the field.

**Tag:** `bringup-step-09`

---

### Step 10 — Re-verification sweep under GSM load 🔴

**Pins claimed:** none new · **Cumulative: 20/26** · **Depends on:** Step 9

**This step is the price of sensors-first, and it is mandatory.** It is logged as a single test — **T14.1** in `README.md` §6 — but it is eight re-runs. Every result from Steps 2–8 was obtained on a quiet rail with no radio. You now have a device that yanks 2 A off the supply every few milliseconds and radiates at 900/1800 MHz next to your analog front ends. Re-run the load-sensitive subset with the modem **actively publishing in a loop**:

| Re-run | Watching for | Fails if |
|---|---|---|
| **T2.2** SD write/read | Corrupted or missing rows during bursts | Any row lost or garbled |
| **T6.3** turbidity stability | ADC spread widening | Spread > 2 %, or a shift in the mean |
| **T12.1 / T12.2** housekeeping | Rail noise appearing as voltage error | Reading moves > 2 % between modem idle and modem transmitting |
| **T3.3 / T4.2** Modbus | CRC errors, timeouts | Any CRC failure that was absent in Step 6 |
| **T5.1** ultrasonic | `pulseIn` timing corrupted by interrupt load | Error grows beyond ±2 cm |
| **T9.1** rail gate | Gate glitching on supply dips | Rail drops below V_BATT − 0.3 V during a burst |

If something regresses, the fix is almost always physical, in this order: **more bulk capacitance at the modem**, then **shorter/star grounds**, then **separate the analog return from the modem return**, then **move the antenna away from the analog wiring**, then **schedule around it** — read all sensors *before* opening the GPRS session, so the two never overlap in time. That last one is cheap and effective, and it is the reason the firmware in §8 is structured as read-then-transmit.

**Exit:** the six re-runs above pass with the modem transmitting. Record both numbers — quiet and loaded — side by side, because the delta is the most useful diagnostic you will own. Tick **T14.1** in §6 only when all eight measurements are green.

**Tag:** `bringup-step-10`

---

### Step 11 — Whole-node integration

**Pins claimed:** none new · **Cumulative: 20/26** · **Depends on:** Step 10

Now merge the eleven step sketches into the real firmware: wake → gate the rail on → settle → read all sensors → gate off → append CSV to SD → open GPRS → publish unsent rows → mark `sent_flag` → sleep.

| Test | Measure | Pass |
|---|---|---|
| **T10.1** | One wake-to-sleep cycle | Row on SD **and** at the broker, timestamps match |
| **T10.2** | Antenna removed for 10 cycles | 10 rows queued, all backfilled **in order, no duplicates** |
| **T10.3** | Cut power mid-SD-write | Log still mountable, ≤ 1 row lost |
| **T10.4** | Force a firmware hang | Watchdog resets and the node resumes |
| **T10.5** | Hold the modem in a failed state | PWRKEY power-cycle recovers it, with backoff |
| **T11.1** | 100-cycle unattended soak | **0 unexplained resets, 100/100 rows logged** |

T11.2 (7-day sealed enclosure soak) belongs to **S6** and needs the enclosure — it is not part of this bench path.

**Stop if:** T11.1 shows even one unexplained reset. One in a hundred on the bench is one every few hours in the field, 200 km away, in the monsoon.

**Tag:** `bringup-step-11` — and at this point stage **S4** in `README.md` §3.6 is complete.

---

## 3. Parts you need to finish this path

Deliberately separated from the big outstanding order (panel, battery, controller, enclosure, glands, conduit). **None of the items below block Steps 1–5**, and everything through Step 9 is achievable with what you have plus a single small shopping trip.

| Need it by | Item | Note |
|---|---|---|
| Step 2 | ¼ W 1 % resistors: 10 Ω, 120 Ω, 560 Ω, 1 k, 1.8 k, 4.7 k, 10 k, 15 k, 20 k, 27 k, 33 k, 47 k, 100 k, 220 k | Buy a strip of 10 each. This is the cheapest line item and it gates five separate steps |
| Step 4 | 100 nF + 1 µF ceramics | Handful of each |
| Step 6 | **USB-RS485 dongle** | Without it T3.3 is not testable and you are debugging Modbus blind |
| Step 6 | 12 V wall adapter ≥ 1 A | Stands in for battery + controller so Steps 6–9 need no power subsystem |
| Step 7 | **AO3401** (P-MOS) + **2N7002** (N-MOS) | Replaces the unusable IRF520 module. Buy 5 of each; they are pennies |
| Step 8 | DS3231 module + SHT31 module | ≈ 350 BDT/node. Skippable — does not block Step 9 |
| Step 9 | **1000 µF low-ESR** electrolytic (+ 470 µF spare) | The single most important part in the modem path |
| Step 9 | **nano→micro SIM adapter** | Trivial part, hard stop without it |
| Step 9 | **External GSM antenna + SMA pigtail** | T7.2 is meaningless without it |
| Step 0 | µA-capable current meter (borrow is fine) | Needed once, at T1.2, for a number that drives the battery sizing |

Already in hand and sufficient: 3× LM2596 (one becomes the 4.10 V modem rail), microSD module, MAX485 module, JSN-SR04T, turbidity module, soil probes, 32 GB card, SIM.

## 4. Progress ledger

Fill this in as you go. It is the one-screen answer to "where are we".

| Step | Peripheral | Pins | Cum. | Tests | Done | Tag | Notes / measured |
|---|---|---|---|---|---|---|---|
| 0 | Bench setup | — | 0 | — | ☐ | — | Module marking: ______ · Panel Voc: ______ V |
| 1 | ESP32 alone | — | 0 | T1.1, T1.2 | ☐ | | Sleep current: ______ mA |
| 2 | microSD | 18, 19, 23, 5 | 4 | T2.1–T2.3 | ☐ | | Module rail: 3V3 / 5V · MISO: ______ V |
| 3 | Ultrasonic | 25, 33 | 6 | T5.1–T5.3 | ☐ | | Error @2 m: ______ cm |
| 4 | Turbidity | 34 | 7 | T6.1–T6.3 | ☐ | | Clear: ______ V raw / ______ V at pin |
| 5 | Housekeeping ADC | 35, 36 | 9 | T12.1, T12.2 | ☐ | | VBAT err: ____ % · VSOL max: ____ V |
| 6a | RS-485 transceiver | 27, 14, 26 | 12 | T3.1–T3.4 | ☐ | | RO at pin: ______ V |
| 6b | Soil probe | — | 12 | T4.1–T4.3 | ☐ | | Addr/baud: ____ / ____ (×3 probes) |
| 6c | Water pH + EC | — | 12 | T4.1, T4.2 | ⊘ | — | **Blocked — probes not purchased** |
| 7 | +12 V rail gate | 32 | 13 | T9.1, T9.2 | ☐ | | Rail on: ____ V · off leak: ____ mA |
| 8 | DS3231 + SHT31 | 21, 22 | 15 | T13.1–T13.3 | ⊘ | | **Blocked — parts not purchased.** Skippable |
| 9 | SIM800L | 16, 17, 4, 13, 39 | **20** | T7.1–T7.7, T8.1 | ☐ | | CSQ: ____ · VBAT sag: ____ V |
| 10 | **Re-verification sweep** | — | 20 | **T14.1** (8 re-runs) | ☐ | | Quiet vs loaded deltas recorded |
| 11 | Whole node | — | 20 | T10.1–T10.5, T11.1 | ☐ | | Soak: ____ / 100 rows |

**All 41 test IDs in `README.md` §6 are reachable from this plan** except T11.2 (7-day sealed-enclosure soak), which belongs to stage S6 and needs the enclosure.

## 5. What this plan deliberately does not cover

So nobody mistakes a green ledger for a finished station:

- **Input protection** — fuse, reverse-polarity P-MOS, TVS, RS-485 surge, earth rod. Bench work runs off a wall adapter; the field build does not. `README.md` §3.4 / **I-23**, **I-37**.
- **Interconnect quality** — every step above may be built on a breadboard **only through Step 1**. From Step 2 onward it goes on protoboard or PCB with soldered joints and proper strain relief. This is **I-39**, and it is the single clearest lesson from the previous build (§11).
- **Power subsystem** — panel, LiFePO4, charge controller, and the battery-does-not-fit-the-box problem (**I-33**). Stages S2 and S6.
- **Enclosure, sealing, mounting, stilling well** — S6/S7. Also: no hot glue as a fixing (**I-40**), neutral-cure RTV only.
- **Calibration against standards** — pH buffers, 1413 µS/cm, 12.88 mS/cm. S7.
- **Cloud side** — broker, TLS, retention, dashboards, and the security baseline (**I-30**, **D-9**). S5.

---

*Companion to `README.md` v1.0. Test IDs, issue IDs (I-nn), decision IDs (D-n), assumption IDs (A-n) and lesson IDs (L-n) are shared with that document and are permanent — never renumber them.*

---
