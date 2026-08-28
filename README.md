# Water & Soil Sensor Station — Sundarbans

**Internal engineering README** · `Water_Soil_Sensor_Station_IoT_Device`

| | |
|---|---|
| **Doc version** | v1.0 — 2026-08-28 |
| **Audience** | IoT engineering team (firmware, hardware, field deployment) |
| **Not for** | Client, funder, or end-user distribution — this document contains unresolved defects, cost gaps and risk language that needs engineering context to read correctly |
| **Build phase** | Phase A (module prototype), pre-integration |
| **Blocking status** | 🔴 **Cannot start integration** — MCU board identity unconfirmed (I-38), power subsystem and enclosure not purchased; **12 P0 defects** open (§7) |

---

## 0. Read this first

### 0.1 Document map

This README is the **build document**: what to do, in what order, wired how, and what's broken. It does not repeat the engineering rationale.

| Document | Purpose | When you read it |
|---|---|---|
| **`README.md`** (this file) | Construction roadmap, connection templates, defect register, build sheets, previous-build lessons | Every day you're building |
| `Sundarbans_Monitoring_Station_Design_Spec.md` | Design rationale, theory, PCB/Phase-B design, power budget maths, field risk analysis | When you need to know *why* a choice was made, or you're starting Phase B |
| `Sundarbans_Monitoring_Station_BOM.xlsx` | Procurement record with prices and supplier links | Purchasing, budget reporting |
| `Equipments.xlsx` | Equipment/tooling list | Bench setup |
| `Sundarbans_Monitoring_Station.xlsx` | Station-level planning sheet | Site planning |

> ⚠️ **The spec is one revision behind this README.** Where they disagree, **this README wins** for Phase A wiring. Known deltas are listed in §10.2 — read that before you use any pinout from the spec.

### 0.2 Assumptions this document makes — confirm or correct

| # | Assumption | Why it matters | Confirmed? |
|---|---|---|---|
| A-1 | 3× core electronics received = **3 stations**, built sequentially (1 pilot → 2 fleet), not 1 station + 2 spares | Drives procurement quantity for power/enclosure (§2.3) and the fleet stage S8 | ☐ |
| A-2 | **No LILYGO T-Call in hand** — discrete ESP32 Dev Module + SIM800L Mini on a **30-pin NodeMCU-32S / WROOM-32** board is the build path | All of §4 is wired for discrete, and §4.2 is derived specifically from WROOM-32 on a 30-pin board. The enclosure BOM note still says "Houses LILYGO board" — treated as stale. **⚠ The previous-build photos (§11) show a board that may not match this** — see **I-38**; this is the single assumption most likely to be wrong, and it invalidates §4.2 if it is | ☐ **verify first** |
| A-3 | Water pH and water EC (K=10) RS-485 probes will be bought **later**, not before the pilot | Pilot node ships measuring soil + level + turbidity only (§3, stage S7) | ☐ |
| A-4 | Sampling interval **15 min** default | Power budget and SD/data volume (**D-7**) | ☐ |
| A-5 | 4–20 mA → RS-485 converter is **permanently dropped** | No analog current-loop path exists in the design | ☑ (BOM: "skip it") |

---

## 1. What we're building

A solar-powered, off-grid station that measures **water** (level, turbidity, later pH + EC/salinity) and **soil** (moisture, temperature, pH, EC, indicative NPK) in the Sundarbans mangrove belt, logs every reading to microSD, and pushes it to the cloud over **2G/GPRS**. One station = one enclosure, one SIM, one solar panel. No gateway tier, no LoRa.

```
        ┌─────────── FIELD NODE (×3 planned) ───────────┐        ┌──── CLOUD ────┐
        │                                               │        │               │
 soil ──┤ RS-485 ─┐                                     │ 2G/    │  MQTT broker  │
 wpH  ──┤ (Modbus)├─▶ MAX485 ─┐                         │ GPRS   │      ↓        │
 wEC  ──┘         │           ├─▶ ESP32 ──▶ microSD     ├───────▶│  time-series  │
 level ─── Trig/Echo ─────────┤   (master)   (log first)│        │      ↓        │
 turb ──── 0–4.5 V ─▶ divider ┘        └──▶ SIM800L ────┤        │ Grafana+alert │
        │                                               │        │               │
        │  30 W panel → charge ctlr → 12 V 6 Ah LiFePO4 │        └───────────────┘
        └───────────────────────────────────────────────┘
```

Locked architecture decisions (do not relitigate without a written reason):

| Decision | Choice | Consequence |
|---|---|---|
| Uplink | Cellular 2G/GPRS, one SIM per node | Recurring data cost per node; hard 2G-coverage dependency (I-14) |
| Water level | Ultrasonic, non-contact, measures air gap | Needs mount above max tide + stilling well; temperature compensation |
| Turbidity | Analog into ESP32 ADC | Needs divider; no isolation; module PCB must stay dry (I-04, I-05) |
| RS-485 | Non-isolated MAX485 + external surge module | Cheaper, but probe grounds tie to board ground |
| Timekeeping | Network time (`AT+CCLK`) | Timestamps drift if modem can't attach (I-11) |
| Data integrity | SD log is source of truth, cloud is a mirror | Store-and-forward backfill required in firmware |

---

## 2. Hardware inventory — as bought

Transcribed from the final purchase list dated **2026-08-28**. This supersedes all earlier BOM assumptions. `Qty` = per station; `Have` = units physically received. The `#` column is the line number from the source purchase sheet — **that sheet restarts numbering inside each category, so duplicate numbers below are faithful to it, not typos.**

### 2.1 In hand ✅

| # | Item | Have | Per station | Engineering note | Issue |
|---|---|---|---|---|---|
| 1 | ESP32 Dev Module (NodeMCU-32S, 30-pin) | 3 | 1 | Phase-A MCU. Sleep current is the autonomy problem | I-10 |
| 2 | SIM800L Mini GPRS/GSM module | 3 | 1 | 2G only. **VBAT 3.4–4.4 V, not 5 V** | I-01, I-02, I-13, I-14 |
| 3 | microSD card module (SPI) | 3 | 1 | Confirm variant: LDO+level-shifter type needs 5 V feed | I-08 |
| 4 | microSD 16/32 GB (Apacer R100) | 1 | 1 | FAT32, ≤32 GB — fine for ESP32 SD lib. **Only 1 of 3 in hand** | I-19 |
| 5 | MAX485 TTL↔RS-485 module | 3 | 1 | **5 V part** — RO output must be divided into ESP32 RX | I-06 |
| 8 | LM2596 buck **with display** | 3 | 1 | Dedicate to the **4.1 V modem rail**; display makes setting it easy | — |
| 9 | 12/24 V → 5 V USB step-down, **2 A** | 3 | 1 | Line item said 5 V/3 A screw-terminal; actual part is a **2 A USB-A output** module | I-07 |
| 6 | Soil sensor RS-485 5-pin (pH/NPK/temp/moisture/EC) | 3 (2 new + **1 used**) | 1 | NPK = **relative trend only** in saline soil. Used unit may have a non-default Modbus address | I-16, I-17 |
| 7 | JSN-SR04T-V3.3 waterproof ultrasonic | 3 | 1 | Level = mount height − distance. ~20–25 cm dead zone, ~4.5 m max | I-09, I-18 |
| 11 | Optical turbidity probe + module, analog | 3 | 1 | Output **0–4.5 V** > ESP32 ADC. Module PCB is **not** IP68 | I-04, I-05 |
| 15 | MOSFET switch module — **IRF520** | 3 | 1 | Line item asked for IRLZ44N. **IRF520 is not logic-level and is low-side** | I-03 |
| 23 | 4G/LTE **nano** SIM + data plan | — | 1 | SIM800L runs it on 2G only. Slot size mismatch likely | I-12, I-14 |

### 2.2 Not yet bought ❌

| # | Item | Needed per station | Blocks stage | Issue |
|---|---|---|---|---|
| 6 | 120 Ω termination resistors (1206 SMD ×5) | 2 | S3 | I-20 |
| 7 | 470 µF 16 V capacitors (×5) | 1+ | S2 | I-02 |
| 12 | 12 V 30 W monocrystalline solar panel | 1 | **S2 / S6** | I-19 |
| 13 | 12 V 6 Ah LiFePO4 pack with BMS | 1 | **S2 / S6** | I-15, I-19 |
| 14 | 12 V PWM/MPPT charge controller, 10 A | 1 | **S2 / S6** | I-21 |
| 16 | IP67 enclosure 158×90×65 mm, clear lid | 1 | **S6** | I-15 |
| 17 | IP68 cable glands PG7/PG9 | 5–6 | S6 | — |
| 18 | Shielded 4-core twisted-pair, 15 m | 15 m+ | S6/S7 | I-22 |
| 19 | Flexible PVC conduit 50–75 mm, 3 m | 3 m | S7 | — |
| 20 | Silica-gel desiccant 100 g ×3 | 1 | S6 | — |
| 21 | RS-485 surge/lightning module (TVS, 10 kA) | 1 | S3 | I-23 |
| 22 | EC calibration standard 1413 µS/cm | 1 | S7 | I-24 |

### 2.3 ⚠️ The fleet-quantity gap — read this before purchasing

You bought **3 of every core electronic part** but the procurement sheet lists **quantity 1** for every power, enclosure, mechanical and calibration line.

| Category | Sets in hand | Sets planned in BOM | Gap for a 3-node fleet |
|---|---|---|---|
| Core electronics (MCU, modem, SD, RS-485, bucks) | 3 | 3 | none |
| Sensors (soil, ultrasonic, turbidity) | 3 | 3 | none |
| Water pH + water EC probes | 0 | 0 (pending) | **3 each** |
| Power (panel, battery, controller) | 0 | 1 | **3** |
| Enclosure, glands, cable, conduit, desiccant | 0 | 1 | **3** |
| Surge protection | 0 | 1 | **3** |
| microSD cards | 1 | 1 | **2** |

**Decision required (D-1, §9):** buy power + enclosure for **one pilot node only** (recommended — validate before spending 3×), or for all three now. The roadmap below assumes pilot-first.

### 2.4 Dropped from scope

| Item | Reason |
|---|---|
| 4–20 mA / 0–10 V → RS-485 Modbus ADC converter | Level went ultrasonic, turbidity went analog-direct. No current-loop devices remain |
| LoRaWAN radio + gateway tier | Architecture is cellular-only |
| Dissolved oxygen / ORP probes | Not funded. RS-485 bus and firmware leave room for one more Modbus slave |

---

## 3. Construction roadmap

### 3.1 Stage gates at a glance

Stages are **gated**: do not start a stage until its predecessor's exit criteria are all ticked. The cost of skipping a gate here is a dead board in a mangrove swamp four hours from the bench.

| Stage | Name | Depends on | Effort | Gate to pass |
|---|---|---|---|---|
| **S0** | Procurement & incoming part verification | — | 3–5 d (mostly lead time) | Every part identified, measured, datasheet filed |
| **S1** | Per-module bench bring-up (isolated) | S0 partial | 4–6 d | Each module proven alone, results in §6 table |
| **S2** | Power tree build & characterisation | S0 (power parts) | 2–3 d | All rails within tolerance under load; sleep current measured |
| **S3** | Signal conditioning & protection build | S1, S2 | 2–3 d | Every divider/bias/gate measured, no over-voltage on any ESP32 pin |
| **S4** | Firmware integration — full cycle | S1–S3 | 5–8 d | 100 consecutive cycles, sense→log→publish→sleep, with forced-outage backfill |
| **S5** | Cloud, dashboard, alerting | S4 partial | 2–4 d | Live data + one alert fired end-to-end |
| **S6** | Enclosure, mechanical, soak test | S4, S0 (mech) | 3–4 d + 7 d soak | 7-day unattended bench soak, no resets, no condensation |
| **S7** | Field pilot — node #1 | S5, S6 | 1 d install + 2–4 wk | ≥2 weeks clean data, survives a cloudy stretch |
| **S8** | Fleet build (nodes #2, #3) + Phase-B PCB | S7 | ongoing | Two more nodes reproducible from the build sheet alone |

**Critical path:** S0 (battery + charge controller + panel lead time) → S2 → S4 → S6 → S7. Firmware work in S1/S4 can run in parallel with power procurement, so **start S1 now on USB power** rather than waiting.

---

### 3.2 S0 — Procurement & incoming part verification

**Goal:** close the purchase gaps and physically verify every part before it's designed around.

**Buy list.** Closes the P0/P1 defects named in the last column. Costs are rough BDT estimates — verify locally. **Quantities assume D-1 resolves to "pilot-first": 1 for anything power/enclosure, 3 for cheap parts where buying three now saves a second shipment.**

| Item | Qty | Why / closes | Est. BDT |
|---|---|---|---|
| 12 V 6 Ah LiFePO4 + BMS | 1 | Power subsystem missing (I-19) | 4,500–7,000 |
| 12 V 30 W mono panel | 1 | Power subsystem missing (I-19) | 2,000–3,000 |
| 10 A charge controller **with LiFePO4 profile, no equalisation stage** | 1 | I-21 | 800–2,500 |
| **LM2596 buck with display (2nd unit per node)** | 3 | **I-07 / D-5** — dedicates one buck to 4.1 V and one to 5.0 V, removing the USB-connector risk | 750 |
| 1000 µF 16 V **low-ESR** electrolytic | 3 | **I-02** modem burst hold-up | 60 |
| 470 µF 16 V electrolytic | 5 | I-02 (parallel with above) | 50 |
| P-channel MOSFET AO3401 (or IRF9540N) | 5 | **I-03** high-side rail gate | 100 |
| 2N7002 / BSS138 small-signal N-FET | 5 | I-03 gate driver | 50 |
| Through-hole resistors, ¼ W 1 %: **10 Ω, 120 Ω, 560 Ω, 1 kΩ, 1.8 kΩ, 4.7 kΩ, 10 kΩ, 15 kΩ, 20 kΩ, 27 kΩ, 33 kΩ, 47 kΩ, 100 kΩ, 220 kΩ** (10 of each) | 1 set | **I-04, I-06, I-09, I-20, I-28** + every divider, bias, pull-up and RS-485 series resistor in §4.5. *Do not buy the 2.2 kΩ/3.3 kΩ pair from spec §8.2 — those values are wrong (§10.2)* | 400 |
| 100 nF + 1 µF ceramic caps, 100 µF electrolytic | 20 / 20 / 5 | RC filters, decoupling, rail-gate soft start | 150 |
| Nano→micro SIM adapter | 3 | **I-12** | 50 |
| External GSM antenna, SMA + pigtail | 3 | **I-13** attach reliability | 900 |
| Inline fuse holder + 3 A fuses | 3 | I-23 | 200 |
| SMBJ15A TVS (battery input) + SM712 TVS (RS-485) | 5 each | I-23 | 300 |
| GSM/coax surge arrestor for the antenna bulkhead | 1 | I-23, I-37 | 700 |
| DS3231 RTC module | 3 | **I-11 / D-8** timestamp integrity | 450 |
| SHT31 or AHT20 (enclosure T/RH) | 3 | **I-25 / D-8** condensation diagnosis | 600 |
| EC standard **1413 µS/cm** | 1 | I-24 — low point of the two-point calibration | 3,200 |
| EC standard **12.88 mS/cm** | 1 | **I-24** — the K=10 probe's actual working range | 1,500–3,000 |
| Gore-type breather vent (M12) | 1 | I-26 | 200 |
| microSD 16 GB **high-endurance** | 3 | **I-19, I-35** — 1 per node + 1 spare (you have 1 consumer card) | 3,000 |
| Separate IP67 battery box (~200×150×100 mm) | 1 | **I-15** — the 6 Ah pack will not fit the 158×90×65 box | 900 |
| Terminal blocks (16-pos), heat-shrink label sleeve, standoffs, sub-plate | 1 set | §4.6 harness, §11 lessons | 500 |

**Tasks**

- [ ] 🔴 **Identify the ESP32 board before anything else (I-38).** Read the shielded module's can marking (`ESP32-WROOM-32`, `-32D`, `-32E`, `-WROVER`, `ESP32-S3-WROOM-1`, …), count the pins per side, and count the USB receptacles. **§4.2 is only valid for a 30-pin WROOM-32 board with one USB port.** If the board has two USB ports, an RGB/addressable LED, or an S3/C3 marking, stop and re-derive §4.2 from that board's pinout — see §11.2
- [ ] Confirm assumption **A-1** (3 stations vs 1 + spares) — this sets purchase quantities
- [ ] Confirm assumption **A-2** (no LILYGO board on site, and the board is 30-pin WROOM-32)
- [ ] Place the buy order above; log lead times for the battery/panel/controller
- [ ] **Measure and photograph** the actual 12 V 6 Ah battery: L × W × H, terminal type, mass → fill §5 build sheet
- [ ] **Identify the MAX485 module IC marking** (MAX485 vs MAX3485/SP3485) → decides I-06 fix path
- [ ] **Identify the microSD module type** (bare adapter vs AMS1117 + 74LVC125 level shifter) → decides its supply rail (I-08)
- [ ] Read the **turbidity module output voltage** on the bench in clear water at 5 V supply → confirms the divider ratio (I-04)
- [ ] Open the **SIM800L Mini** and check SIM slot size (micro vs nano) → confirms adapter need (I-12)
- [ ] Bench-read the **used soil sensor's Modbus address and baud** with a USB-RS485 dongle before it goes anywhere near the node (I-16)
- [ ] Obtain the soil probe's **Modbus register map** and confirmed **supply voltage range** from the vendor (I-17)
- [ ] Confirm the SIM's **APN** and that the data plan is provisioned on **2G** (I-14)
- [ ] **Measure the panel's open-circuit voltage in open sun** once it arrives → confirms the 220 k/33 k tap (I-28)
- [ ] File every datasheet in `docs/datasheets/` named `<part>_<rev>.pdf`

**Exit criteria**

- [ ] **I-38 closed** — board variant written on the §5 build sheet, and §4.2 either confirmed or re-derived
- [ ] No line in §2.2 still blocking a stage you intend to start
- [ ] §5 build sheet "Part identification" block filled for node #1
- [ ] I-06, I-08, I-12, I-17 each resolved to a definite answer (not "probably")

---

### 3.3 S1 — Per-module bench bring-up (isolated)

**Goal:** every module proven working *alone*, on USB/bench-supply power, before anything is integrated. Record results in the §6 test table — do not rely on memory.

> **This is the only stage where a solderless breadboard is allowed** (I-39, §11.1 L-1). Nothing that leaves S1 keeps its breadboard.

**Prerequisites:** ESP32, modules, breadboard, USB-RS485 dongle, multimeter, bench PSU (or 12 V wall adapter), and a current meter that resolves **~100 µA or better** — a 100 mA-resolution meter cannot see deep-sleep current at all, which is the one number this whole build's autonomy depends on. A µA-capable DMM, a shunt + scope, or a USB power analyser all work.

**Tasks — do them in this order**

- [ ] **T1 · ESP32 baseline.** Flash, serial console works, `esp_deep_sleep` with timer wake works, boot reason readable
- [ ] **T2 · microSD.** Mount card, append a row to `test.csv`, unmount, power-cycle, read it back. Confirm supply rail per I-08
- [ ] **T3 · RS-485 loopback.** ESP32 → MAX485 → USB-RS485 dongle → PC terminal. Verify DE/RE direction control and that RO into the ESP32 is ≤3.3 V (**measure it** — I-06)
- [ ] **T4 · Soil probe read.** Modbus RTU function `0x03`, all registers, CRC valid, values sane in a cup of moist soil. Record address + baud + register map
- [ ] **T5 · Ultrasonic.** Trigger/Echo distance against a tape measure at 0.3 / 1.0 / 2.0 / 4.0 m. Confirm echo pin level ≤3.3 V (I-09). Note dead-zone behaviour below ~25 cm
- [ ] **T6 · Turbidity.** Powered at 5 V, measure raw output in clear water and in stirred muddy water. **Do not connect to the ESP32 until the divider from S3 exists** (I-04)
- [ ] **T7 · SIM800L standalone.** Bench PSU at **4.1 V**, ≥1000 µF at VBAT, nano→micro adapter fitted, external antenna. `AT` → `OK`, `AT+CSQ` ≥ 10, `AT+CREG?` registered, `AT+CCLK?` returns network time, GPRS attach on the confirmed APN, one MQTT publish to a test broker
- [ ] **T8 · Modem sleep.** `AT+CSCLK=2` sleep entered and woken; measure current in both states

**Exit criteria**

- [ ] The **S1-scope** tests pass with measured numbers written into §6: **T1.1–T1.3, T2.1–T2.2, T3.1, T4.1–T4.3, T5.1–T5.2, T6.1, T7.1–T7.6, T8.1–T8.2**
- [ ] **Deferred by design** (they need hardware that does not exist until S2/S3 — do not treat these as S1 blockers): **T3.2** (RO divider output, needs the S3 divider), **T6.2** (turbidity into ADC, needs the S3 divider), **T7.7** (transmit-burst rail sag, needs the S2 power tree). Each is listed against its own stage in §6
- [ ] No ESP32 input pin has been exposed to >3.3 V at any point (over-voltage is cumulative damage, not pass/fail)
- [ ] Soil probe register map documented in `docs/modbus_soil_probe.md`

> **On test IDs:** the `T1`…`T8` labels above are *groups*; §6 breaks each into numbered steps (`T1.1`, `T1.2`, …) with a pass/fail and a measured-value column. Record against the §6 numbers — the groups here are only for ordering the bench work.

---

### 3.4 S2 — Power tree build & characterisation

**Goal:** a stable, measured three-rail power tree that survives a 2 A modem burst without resetting the MCU.

> **Off the breadboard from here on** (I-39). Soldered protoboard or crimped connectors only — a 2 A burst through a spring contact is not a power tree, it is a reboot generator. Build the input protection **before** the first battery connection (I-23, §11.1 L-5).

**Tasks**

- [ ] Build battery input protection: **fuse → reverse-polarity P-MOS (or Schottky) → SMBJ TVS** (circuit in §4.5.6)
- [ ] Set **LM2596 #1 to 4.10 V** under a 0.5 A dummy load; verify it holds. Lock the pot with nail varnish and label the module
- [ ] Fit **1000 µF low-ESR + 470 µF** directly at SIM800L VBAT/GND with short, thick leads (I-02)
- [ ] Build the **5 V rail**. If keeping the 2 A USB module, solder wires to its output pads — **no USB cable in the final build** (I-07). Preferred: second LM2596 set to 5.0 V
- [ ] Confirm the modem rail and the MCU rail are **separate converters** off the battery bus
- [ ] Build the **high-side +12 V sensor rail gate** (§4.5.5) — do *not* use the IRF520 module as shipped (I-03)
- [ ] Verify rail sequencing on cold start: no rail overshoots, ESP32 boots cleanly
- [ ] **Burst test:** trigger a GPRS transmit while scoping the 5 V rail and VBAT. Acceptance: VBAT sag ≤ 0.4 V, 5 V rail sag ≤ 0.25 V, **zero ESP32 resets over 50 transmits**
- [ ] **Measure deep-sleep current** of the whole assembled node at the battery terminals. Write the number in §5 — the power budget in the spec (§9.4) is only valid once this is real
- [ ] Recompute autonomy from the measured figure; if < 10 days, apply the reductions in I-10 before S6

**Exit criteria**

| Rail | Target | Tolerance | Measured | Pass |
|---|---|---|---|---|
| VBATT bus | 12.0–14.6 V | as battery/controller | ______ V | ☐ |
| Modem rail | 4.10 V | ±0.10 V, ≤0.4 V sag on TX | ______ V | ☐ |
| 5 V rail | 5.00 V | ±0.15 V | ______ V | ☐ |
| 3.3 V rail | 3.30 V | ±0.10 V | ______ V | ☐ |
| +12 V gated rail (ON) | ≥ VBATT − 0.3 V | — | ______ V | ☐ |
| +12 V gated rail (OFF) | ≤ 0.2 V | leakage check | ______ V | ☐ |
| Deep-sleep current | as low as achievable | record actual | ______ mA | ☐ |
| Autonomy from measured sleep | ≥ 10 days | see spec §9.4 | ______ d | ☐ |

---

### 3.5 S3 — Signal conditioning & protection build

**Goal:** nothing outside the enclosure can put more than 3.3 V — or a surge — onto an ESP32 pin.

**Tasks**

- [ ] Build the **turbidity divider + RC** (§4.5.4); verify output ≤3.15 V with the sensor at maximum (clear water) output
- [ ] Build the **battery and solar sense dividers** (§4.5.7); verify ≤3.15 V at 15.0 V input; record the exact ratio for firmware calibration
- [ ] Build the **ESP32 TX → SIM800L RXD divider** (§4.5.2); verify ~2.8 V logic high
- [ ] Build the **MAX485 RO → ESP32 RX divider** if the module runs at 5 V (§4.5.3, I-06); verify ≤3.3 V high level
- [ ] Fit **RS-485 fail-safe bias** (560–680 Ω pull-up on A, pull-down on B) — **first check whether your module already has bias resistors** and don't stack them
- [ ] Fit **120 Ω termination** at the two physical ends of the bus only (I-20)
- [ ] Insert the **RS-485 surge module** inline at the cable entry, and **SM712 TVS + 10 Ω series** on A/B at the board
- [ ] **Run A/B on shielded twisted pair, never on ribbon or loose jumpers** (I-41, §11.1 L-4); shield bonded at the enclosure end only
- [ ] Verify the **JSN-SR04T echo level** matches its supply choice (I-09)
- [ ] Continuity + polarity check every net against §4.2 **before** first power-on with sensors attached

**Exit criteria**

- [ ] Every ESP32 input measured at its worst-case source condition, all ≤3.3 V
- [ ] RS-485 idle bus reads a defined logic state with all probes unpowered
- [ ] §4.4 module tables all have their "Verified" box ticked for node #1

---

### 3.6 S4 — Firmware integration (full cycle)

**Goal:** one unattended firmware image that completes the whole measurement cycle reliably and never loses a reading.

**Tasks**

- [ ] Implement the state machine (spec §14.1): `WAKE → POWER_SENSORS → READ_MODBUS → READ_ULTRASONIC → READ_ADC → GET_TIME → LOG_SD → CONNECT_GPRS → PUBLISH+BACKFILL → SLEEP`
- [ ] **Every stage must be able to time out and still reach `LOG_SD` and `SLEEP`** — a stuck probe or modem must never block the next cycle
- [ ] Modbus master with per-probe timeout, CRC validation, and retry ×2
- [ ] Ultrasonic: N=7 pings, discard out-of-range, take median; apply temperature compensation from the soil probe's temperature reading
- [ ] Turbidity: 64-sample average, median-of-3 batches, apply calibration curve
- [ ] Housekeeping: VBAT, VSOL, `AT+CSQ`, boot count, reset reason, uptime
- [ ] Timekeeping: NITZ + `AT+CCLK?` each cycle, held across sleep in RTC memory; DS3231 fallback if fitted (I-11)
- [ ] SD CSV with `sent_flag`; mark rows on publish ACK; backfill unsent rows oldest-first with a per-cycle cap. Use **high-endurance cards, append-and-flush, never hold a file open across sleep** (I-35)
- [ ] MQTT publish via TinyGSM + PubSubClient to `sundarbans/<nodeID>/data`; subscribe `sundarbans/<nodeID>/cmd` for interval/threshold downlink
- [ ] Config (node ID, interval, APN, broker, thresholds, divider ratios, calibration constants) in a **file on SD or NVS**, not hardcoded
- [ ] Task watchdog enabled and fed; on repeated modem failure, power-cycle SIM800L via PWRKEY/RST with exponential backoff
- [ ] Deep sleep: sensor rails off, modem asleep or gated, all GPIO in a defined state
- [ ] **Update policy decided and written down:** local USB flashing at service visits is the default; any remote update must be resumable, checksum-verified and gated on `CSQ ≥ 15` (I-31)

**Exit criteria**

- [ ] **100 consecutive cycles** unattended with zero unexplained resets
- [ ] Forced-outage test: pull the antenna for 10 cycles → all 10 rows queued on SD → all 10 backfilled on reconnect, in order, no duplicates
- [ ] Yank power mid-write → SD log still mountable, at most the in-flight row lost
- [ ] Firmware tagged in git, config file for node #1 committed

---

### 3.7 S5 — Cloud, dashboard & alerting

**Tasks**

- [ ] Stand up the broker (Mosquitto on a small VPS) with **per-node credentials**
- [ ] Ingest → time-series DB (InfluxDB or TimescaleDB)
- [ ] Grafana dashboards: tidal level curve, salinity/EC trend, pH, turbidity, soil moisture/EC, and a **fleet health panel** (VBAT, VSOL, CSQ, last-seen)
- [ ] **Label the soil N/P/K panels "relative trend only — not agronomic"** on the dashboard *and* in the schema description (I-29). This is the one place a downstream user can be misled by a number that looks authoritative
- [ ] Node registry table: node ID → GPS, install date, Modbus addresses, calibration dates, SIM number
- [ ] Alert rules: salinity threshold, level outside tidal band, VBAT low, **station offline (no data for N intervals — server-side)**, turbidity spike
- [ ] Security baseline per I-30: **unique credentials per node**, non-default port, server-side payload validation, no sensitive data in the payload. Record the transport-security limitation (spec §15.4) in the deployment record — SIM800L's TLS stack is not something to rely on

**Exit criteria**

- [ ] Live data from the bench node visible in Grafana
- [ ] One alert deliberately triggered and received
- [ ] Offline alert verified by powering the bench node down

---

### 3.8 S6 — Enclosure, mechanical & soak test

**Tasks**

- [ ] **Mock the fit first** (I-15). Battery almost certainly goes in a **separate box** — confirm with the measured dimensions from S0
- [ ] Lay out the enclosure per §4.9: power-in corner, modem + antenna at the edge, MCU centre, RS-485 + surge hard against the sensor gland, analog away from the modem
- [ ] Mount modules on standoffs or a cut FR4/acrylic sub-plate — **no modules loose on foam or double-sided tape, and no hot glue as a fixing** (I-40, §11.1 L-3, L-6)
- [ ] Internal harness to a terminal block per §4.6; label every wire per §4.1
- [ ] Drill and fit glands **on the bottom face**, one per §4.7 cable **C1–C4** (C5 is an SMA bulkhead, not a gland); blank any spare positions with proper plugs
- [ ] Fit the breather vent and desiccant (I-26)
- [ ] Conformal-coat or at minimum spray-coat module undersides; keep connectors, SD socket and antenna masked
- [ ] Sun-shield or repaint the clear lid (I-27)
- [ ] **7-day bench soak** with real solar + battery: log resets, condensation, internal temperature, autonomy through a shaded day

**Exit criteria**

- [ ] 7 days unattended, zero resets, no condensation on the board
- [ ] Battery still above the low-voltage threshold after a simulated no-sun day
- [ ] Complete §5 build sheet for node #1, signed off
- [ ] Photographs of the as-built wiring filed in `docs/as_built/node-01/`

---

### 3.9 S7 — Field pilot, node #1

**Tasks**

- [ ] **Site survey first:** confirm 2G signal with the actual node (`AT+CSQ` ≥ 10 at the exact mount point), max flood level, sun path, mounting substrate, theft exposure
- [ ] Install above maximum flood level; drip loops on every cable; glands pointing down
- [ ] Ultrasonic on a rigid bracket over a **stilling well** (vertical perforated PVC) — this is the difference between usable and noisy tidal level data (I-18)
- [ ] Soil probe buried in the root zone at a **documented depth**, backfilled firm
- [ ] **Every potted probe joint dunk-tested for 24 h before install**, not after (§11.1 L-8)
- [ ] Turbidity: probe head only in the water, module PCB inside the enclosure (I-05). Head inside a **stilling well or shrouded holder** so it does not read bubbles and debris (I-36)
- [ ] **Earthing:** drive an earth rod at the mount; bond the enclosure, the GSM arrestor and the RS-485 surge module to it; shield bonded at the enclosure end only (I-37). Without this the surge protection fitted in S2/S3 is decorative
- [ ] Cables in PVC conduit, secured against tidal drag
- [ ] Record GPS, install photos, mount height (needed for the level calculation), Modbus addresses, calibration date
- [ ] Leave a laminated card inside the lid: node ID, Modbus addresses, rail voltages, contact number
- [ ] Book the **first service visit** into the calendar before leaving site — clean the optical window, inspect for fouling, recharge desiccant, re-check calibration, and log the dates (I-32)

**Exit criteria**

- [ ] ≥2 weeks continuous data, gaps explained
- [ ] Survived a multi-day cloudy stretch without dropping below the low-voltage threshold
- [ ] Biofouling and calibration drift observed and quantified at the first service visit
- [ ] Deviations from this README written back into it

---

### 3.10 S8 — Fleet build & Phase B

- [ ] Build nodes #2 and #3 **from the §5 build sheet and §4 templates only** — if you need to ask a question, the templates are incomplete; fix them
- [ ] Add water pH + water EC (K=10) probes when purchased; assign addresses `0x02`/`0x03`; calibrate with both 1413 µS/cm and 12.88 mS/cm (I-24). This is the point at which the **I-33 scope gap closes** and the station measures its full advertised parameter set
- [ ] Track fleet health for one month before declaring the design stable
- [ ] Then start Phase B: custom PCB per spec §12, including the **LTE modem footprint** (I-14) and an isolated RS-485 option

---

## 4. Connection design — templates

These are the **authoritative Phase-A wiring templates**. Copy §4.2, §4.4, §4.6 and §4.7 per node and fill the "as-built / verified" columns during assembly. A blank template that nobody filled in is the same as no template.

### 4.1 Conventions

**Net naming:** `UPPER_SNAKE_CASE`, prefixed by subsystem. The complete prefix set — anything not on this list is an undocumented net and should not appear on a label:

| Prefix / net | Subsystem |
|---|---|
| `SIM_` | SIM800L modem (TXD_IN, RXD_OUT, PWRKEY, RST, STATUS) |
| `RS485_` | RS-485 bus (RO, DI, DE_RE, A, B) |
| `SD_` | microSD SPI (SCK, MISO, MOSI, CS) |
| `US_` | Ultrasonic (TRIG, ECHO) |
| `TURB_` | Turbidity (ADC, and its 5 V supply leg) |
| `HK_` | Housekeeping analog (VBAT_ADC, VSOL_ADC) |
| `I2C_` | Reserved I²C for DS3231 / SHT31 (SDA, SCL) |
| `SENS_EN` | Gate control for the switched +12 V sensor rail |
| `PV_SENSE` | Raw panel tap upstream of the `HK_VSOL_ADC` divider |
| `SHIELD` | Cable shield drain — bonded at the enclosure end only, never used as a return |
| `BATT+` / `BATT−` | Battery terminals upstream of the fuse |
| Rails | `VBATT`, `V12_SENS`, `V5`, `V4V1`, `V3V3`, `GND` |

**Wire colour code — use this everywhere, no exceptions:**

| Colour | Use |
|---|---|
| Red | Any positive power rail (mark voltage on the label) |
| Black | Ground / return |
| Orange | Switched +12 V sensor rail (`V12_SENS`) — a reminder it is *not* always live |
| Yellow | RS-485 A |
| Green | RS-485 B |
| Blue | Digital signal (TRIG, ECHO, DE/RE, PWRKEY, RST) |
| White | Analog signal (turbidity, sense taps) |
| Bare/drain | Cable shield — **bonded at the enclosure end only** |

**Label format:** `<NODE>-<TB>-<POS>-<NET>` → e.g. `N01-TB2-07-RS485_A`. Label **both ends** of every wire. Heat-shrink printed labels, not marker on PVC (it wipes off in salt spray).

**Torque/strain:** every external cable gets a drip loop and a strain relief inside the gland; no wire carries mechanical load to a terminal.

---

### 4.2 Master GPIO map — ESP32 NodeMCU-32S (30-pin, WROOM-32)

**This table supersedes spec §11.1.** Copy it per node and tick as you wire.

> 🔴 **Valid only for a 30-pin WROOM-32 board.** Confirm the module marking and pin count in S0 before you wire a single connection — see **I-38** and **§11.2**. On an ESP32-S3/C3 or a WROVER module this table is wrong in ways that fail silently, and it must be re-derived rather than adapted.

| ESP32 pin | Net | Dir | Goes to | Conditioning required | Wired | Verified |
|---|---|---|---|---|---|---|
| GPIO16 | `SIM_TXD_IN` | in | SIM800L **TXD** | none (SIM ~2.8 V reads high on ESP32) | ☐ | ☐ |
| GPIO17 | `SIM_RXD_OUT` | out | SIM800L **RXD** | **divider 1.8 kΩ / 10 kΩ → ~2.8 V** (§4.5.2) | ☐ | ☐ |
| GPIO4 | `SIM_PWRKEY` | out | SIM800L PWRKEY | none — pulse LOW ~1 s to toggle | ☐ | ☐ |
| GPIO13 | `SIM_RST` | out | SIM800L RST | none | ☐ | ☐ |
| GPIO39 (VN) | `SIM_STATUS` | in only | SIM800L STATUS / NETLIGHT | none; **no internal pull-up on this pin** | ☐ | ☐ |
| GPIO27 | `RS485_RO` | in | MAX485 **RO** | **divider 10 kΩ / 15 kΩ if MAX485 runs at 5 V** (§4.5.3) | ☐ | ☐ |
| GPIO14 | `RS485_DI` | out | MAX485 **DI** | none (3.3 V meets MAX485 V_IH) | ☐ | ☐ |
| GPIO26 | `RS485_DE_RE` | out | MAX485 **DE + RE tied together** | none. HIGH = transmit | ☐ | ☐ |
| GPIO18 | `SD_SCK` | out | microSD module SCK | none | ☐ | ☐ |
| GPIO19 | `SD_MISO` | in | microSD module MISO | 47 kΩ pull-up to 3.3 V | ☐ | ☐ |
| GPIO23 | `SD_MOSI` | out | microSD module MOSI | none | ☐ | ☐ |
| GPIO5 | `SD_CS` | out | microSD module CS | **10 kΩ pull-up to 3.3 V — mandatory**, GPIO5 is a boot strap that must be HIGH at reset | ☐ | ☐ |
| GPIO25 | `US_TRIG` | out | JSN-SR04T Trig | none; 10 µs pulse | ☐ | ☐ |
| GPIO33 | `US_ECHO` | in | JSN-SR04T Echo | **none if sensor powered at 3.3 V**; divider 10 kΩ/15 kΩ if powered at 5 V (§4.5.8) | ☐ | ☐ |
| GPIO34 | `TURB_ADC` | in only | Turbidity signal | **divider 10 kΩ / 20 kΩ + RC 1 kΩ/100 nF** (§4.5.4) | ☐ | ☐ |
| GPIO35 | `HK_VBAT_ADC` | in only | Battery sense tap | **divider 100 kΩ / 27 kΩ + 100 nF** (§4.5.7) | ☐ | ☐ |
| GPIO36 (VP) | `HK_VSOL_ADC` | in only | Solar (PV) sense tap | **divider 220 kΩ / 33 kΩ + 100 nF** — PV *Voc* is ~21–22 V, not 15 V (I-28) | ☐ | ☐ |
| GPIO32 | `SENS_EN` | out | High-side gate driver (2N7002 → P-MOS) | none. HIGH = +12 V sensor rail ON | ☐ | ☐ |
| GPIO21 | `I2C_SDA` | bidir | *reserved* — DS3231 / SHT31 | 4.7 kΩ pull-up to 3.3 V when populated | ☐ | ☐ |
| GPIO22 | `I2C_SCL` | out | *reserved* — DS3231 / SHT31 | 4.7 kΩ pull-up to 3.3 V when populated | ☐ | ☐ |
| GPIO1 / GPIO3 | `UART0 TX/RX` | — | **Leave free** — USB serial console for field debug | — | — | — |
| GPIO0, 2, 12, 15 | — | — | **Do not use.** Boot strapping pins | — | — | — |
| GPIO6–11 | — | — | **Not available** — internal SPI flash | — | — | — |

**Pin rules that bit us before, so they are written down:**

1. **ADC1 only** for analog (GPIO 32–39). ADC2 is unusable while WiFi is active — irrelevant in the field but it will waste a bench afternoon.
2. **GPIO34/35/36/39 are input-only** and have no internal pull-up/pull-down. Never assign an output or a pin that needs a defined idle level without an external resistor.
3. **UART1's default pins (9/10) collide with the flash.** Remap explicitly: `HardwareSerial rs485(1); rs485.begin(9600, SERIAL_8N1, 27, 14);`
4. **GPIO16/17 are free on ESP32-WROOM-32.** If a board turns out to be WROVER (PSRAM), 16/17 are taken — check the module marking and reassign UART2 if so.
5. **GPIO5 and GPIO12/15 are straps.** GPIO5 with a pull-up is safe (that's the normal SPI-boot condition). GPIO12/15 pulled the wrong way will stop the board booting — hence "do not use".

---

### 4.3 Power tree

```
  30 W PV ──▶ ┌──────────────────┐
              │ Charge controller│──▶ 12 V 6 Ah LiFePO4 (+BMS)
              │   10 A, LiFePO4  │         │
              └────────┬─────────┘         │
                    PV+ tap                │  BATT+/BATT−
                       │                   │
                 [220k/33k]        ┌───────┴────────────────────────────┐
                       │           │  FUSE 3 A → rev-pol P-MOS → SMBJ15A│  §4.5.6
                  GPIO36           └───────┬────────────────────────────┘
                                           │
                                     VBATT bus (12.0–14.6 V)
                                           │
              ┌────────────────┬───────────┼──────────────┬─────────────────┐
              │                │           │              │                 │
      [100k/27k] → GPIO35   LM2596 #1   LM2596 #2    High-side P-MOS    (spare)
                            = 4.10 V    = 5.00 V     gate, §4.5.5
                                │           │              │
                                │           │              └──▶ V12_SENS (orange)
                                │           │                     └─▶ RS-485 probes
                                │           ├──▶ ESP32 VIN (5 V pin)
                                │           ├──▶ Turbidity module VCC
                                │           └──▶ microSD module VCC (if level-shifter type)
                                │                  and MAX485 VCC (see I-06)
                                │
                                └──▶ SIM800L VBAT  [1000 µF low-ESR ∥ 470 µF ∥ 100 nF]
                                        §4.5.1

  V3V3: generated by the ESP32 board's own AMS1117 → feeds dividers, pull-ups,
        JSN-SR04T (preferred), and MAX485 only if it is a 3.3 V part (MAX3485/SP3485).
```

| Rail | Source | Nominal | Feeds | Est. load |
|---|---|---|---|---|
| `VBATT` | Battery via protection | 12.0–14.6 V | Both bucks, gate, sense divider | — |
| `V4V1` | LM2596 #1 | 4.10 V | SIM800L VBAT only | 20 mA idle, **2 A burst** |
| `V5` | LM2596 #2 (or the 2 A USB module, wires soldered) | 5.00 V | ESP32 VIN, turbidity, SD module, MAX485 | 200–350 mA |
| `V3V3` | ESP32 onboard AMS1117 | 3.30 V | Dividers, pull-ups, ultrasonic, 3.3 V RS-485 variant | 60–100 mA |
| `V12_SENS` | VBATT via high-side P-MOS | = VBATT | RS-485 probes (gated, off in sleep) | 50–200 mA when on |

> **Rule: the modem never shares a converter with the MCU.** Every "random ESP32 reboot" story on this platform ends here.

---

### 4.4 Per-module connection templates

Fill the "As-built" column with what you actually wired, not what you intended.

#### 4.4.1 SIM800L Mini — GPRS modem

| SIM800L pin | Net | Connect to | Conditioning | As-built | ✓ |
|---|---|---|---|---|---|
| VBAT | `V4V1` | LM2596 #1 output | **1000 µF ∥ 470 µF ∥ 100 nF within 20 mm**, ≥20 AWG | | ☐ |
| GND | `GND` | Star ground | Thick, short — this carries the 2 A burst return | | ☐ |
| TXD | `SIM_TXD_IN` | ESP32 GPIO16 | direct | | ☐ |
| RXD | `SIM_RXD_OUT` | ESP32 GPIO17 | **1.8 kΩ / 10 kΩ divider** | | ☐ |
| RST | `SIM_RST` | ESP32 GPIO13 | direct | | ☐ |
| PWRKEY (KEY) | `SIM_PWRKEY` | ESP32 GPIO4 | direct, pulse LOW ~1 s | | ☐ |
| NETLIGHT / STATUS | `SIM_STATUS` | ESP32 GPIO39 | direct | | ☐ |
| ANT / IPX | — | **External GSM antenna** via SMA bulkhead | Add GSM surge arrestor at the bulkhead | | ☐ |
| SIM socket | — | Nano SIM **in a nano→micro adapter** (I-12) | Verify slot size on the unit in hand | | ☐ |

Checks before first power-on: **measure LM2596 output = 4.10 V with the module disconnected.** 5 V on VBAT destroys the module.

#### 4.4.2 MAX485 module — RS-485 transceiver

| MAX485 pin | Net | Connect to | Conditioning | As-built | ✓ |
|---|---|---|---|---|---|
| VCC | `V5` *(or `V3V3` only if the IC is MAX3485/SP3485)* | 5 V rail | 100 nF decoupling. **See I-06 — this decision drives the RO divider** | | ☐ |
| GND | `GND` | Star ground | | | ☐ |
| DI | `RS485_DI` | ESP32 GPIO14 | direct | | ☐ |
| RO | `RS485_RO` | ESP32 GPIO27 | **10 kΩ / 15 kΩ divider if VCC = 5 V** | | ☐ |
| DE | `RS485_DE_RE` | ESP32 GPIO26 | tie DE and RE together | | ☐ |
| RE | `RS485_DE_RE` | ESP32 GPIO26 | tied to DE | | ☐ |
| A | `RS485_A` | Surge module → TB2-07 (yellow) | 10 Ω series + SM712 + fail-safe pull-up 560 Ω | | ☐ |
| B | `RS485_B` | Surge module → TB2-08 (green) | 10 Ω series + SM712 + fail-safe pull-down 560 Ω | | ☐ |

#### 4.4.3 microSD module — SPI storage

| SD pin | Net | Connect to | Conditioning | As-built | ✓ |
|---|---|---|---|---|---|
| VCC | `V5` *(level-shifter type)* or `V3V3` *(bare adapter type)* | see I-08 | Identify the module first | | ☐ |
| GND | `GND` | Star ground | | | ☐ |
| SCK | `SD_SCK` | GPIO18 | | | ☐ |
| MISO | `SD_MISO` | GPIO19 | 47 kΩ pull-up to 3.3 V | | ☐ |
| MOSI | `SD_MOSI` | GPIO23 | | | ☐ |
| CS | `SD_CS` | GPIO5 | **10 kΩ pull-up to 3.3 V** | | ☐ |

#### 4.4.4 JSN-SR04T-V3.3 — ultrasonic level

| Sensor pin | Net | Connect to | Conditioning | As-built | ✓ |
|---|---|---|---|---|---|
| 5V / VCC | `V3V3` **(preferred)** | 3.3 V rail | Keeps Echo at 3.3 V — no divider needed | | ☐ |
| GND | `GND` | TB3-11 | | | ☐ |
| Trig | `US_TRIG` | GPIO25 via TB3-12 (blue) | | | ☐ |
| Echo | `US_ECHO` | GPIO33 via TB3-13 (blue) | **If powered at 5 V, add 10 kΩ/15 kΩ divider** (I-09) | | ☐ |

Mode note: the module can be reconfigured to UART by fitting the mode resistor — more robust over a long cable. If you switch, re-map `US_TRIG`/`US_ECHO` to a hardware UART and update this table.

#### 4.4.5 Turbidity module — analog

| Module pin | Net | Connect to | Conditioning | As-built | ✓ |
|---|---|---|---|---|---|
| VCC | `V5` | 5 V rail | Module needs 5 V for its stated output range | | ☐ |
| GND | `GND` | TB4-15 | | | ☐ |
| AO / A-out | `TURB_ADC` | GPIO34 | **10 kΩ / 20 kΩ divider + 1 kΩ/100 nF RC — mandatory** (I-04) | | ☐ |
| DO / D-out | — | **not used** | Leave unconnected | | ☐ |

**Mount the module PCB inside the enclosure.** Only the sealed probe head and its cable go in the water (I-05).

#### 4.4.6 +12 V sensor rail gate (replaces the IRF520 module)

| Node | Connect to | Notes | As-built | ✓ |
|---|---|---|---|---|
| P-MOS source | `VBATT` | AO3401 for ≤1.5 A, IRF9540N for more | | ☐ |
| P-MOS drain | `V12_SENS` → TB2-05 (orange) | 100 µF at the drain for probe inrush | | ☐ |
| P-MOS gate | via 10 kΩ to `VBATT`, 10 kΩ to 2N7002 drain | 10 kΩ/10 kΩ gives ≈ −6 V V_GS at 12 V | | ☐ |
| 2N7002 gate | `SENS_EN` ← GPIO32 | HIGH = rail ON | | ☐ |
| 2N7002 source | `GND` | | | ☐ |

#### 4.4.7 Housekeeping sense

| Signal | Tap point | Divider | Reads at 15 V / 22 V | As-built ratio | ✓ |
|---|---|---|---|---|---|
| `HK_VBAT_ADC` | VBATT bus, after the fuse | 100 kΩ / 27 kΩ | 3.19 V @ 15 V | | ☐ |
| `HK_VSOL_ADC` | Charge controller **PV+** terminal | 220 kΩ / 33 kΩ | 2.87 V @ 22 V | | ☐ |

Record the **measured** resistor values and put the real ratio in the firmware config — 5 % resistors will otherwise cost you 0.5 V of accuracy.

#### 4.4.8 I²C expansion (reserved)

| Device | Address | Purpose | Populated | ✓ |
|---|---|---|---|---|
| DS3231 RTC | 0x68 | Backup timekeeping when the modem can't attach (I-11) | ☐ | ☐ |
| SHT31 / AHT20 | 0x44 / 0x38 | Enclosure temp + RH, condensation diagnosis (I-25) | ☐ | ☐ |

---

### 4.5 Circuit blocks

Copy these into the schematic. Resistor values are calculated for this design — do not substitute by eye.

#### 4.5.1 SIM800L supply — the single most important circuit in the build

```
   VBATT ──▶ [ LM2596 #1, Vout trimmed to 4.10 V ]
                          │ Vout+
                          ├──────────┬──────────┬──────────┐
                          │          │          │          │
                       1000 µF     470 µF     100 nF       ├──▶ SIM800L VBAT
                       low-ESR      16 V       cer.        │
                          │          │          │          │
                          └──────────┴──────────┴──────────┴──▶ SIM800L GND
                                     │
                                  star GND
        │←───────────── keep this whole loop under 20 mm ─────────────→│
```

Rules: caps physically at the module, not at the buck. ≥20 AWG on VBAT and GND. Trim the LM2596 **before** the module is connected, under a 0.5 A dummy load. Acceptance: ≤0.4 V sag during transmit.

#### 4.5.2 ESP32 TX → SIM800L RXD level shift

```
   ESP32 GPIO17 ──[ 1.8 kΩ ]──┬──▶ SIM800L RXD
                              │
                          [ 10 kΩ ]
                              │
                             GND
```

`3.3 V × 10/11.8 = 2.80 V` — matches SIM800L's ~2.8 V logic.

> ⚠️ **Correction to spec §8.2**, which specified 2.2 kΩ/3.3 kΩ. That yields **1.98 V**, which sits right on the SIM800L input-high threshold and will produce intermittent AT failures. Use 1.8 kΩ/10 kΩ.

#### 4.5.3 MAX485 at 5 V — RO divider, bias, termination, surge

```
                 V5 ──┬──── VCC ┌──────────────┐
                      │         │              │  A ──┬──[10 Ω]──┬────▶ TB2-07  RS485_A
                   100 nF       │   MAX485     │      │          │
                      │         │              │      │       [120 Ω]   ← board end; the second
                     GND        │              │  B ──┼──[10 Ω]──┤        120 Ω sits at the far
                                │              │      │          │        end of the bus (§4.8)
                                │              │      │          │
  GPIO14 ─────────────────────▶ │ DI           │      │          └────▶ TB2-08  RS485_B
  GPIO26 ─────────────────────▶ │ DE ──┬── RE  │   [SM712]
                                │      │       │      │
                                │ RO ──┴───────┘     GND
                                └──┬──
                                   │  5 V logic — MUST be divided
                        ┌──[10 kΩ]─┴──────┐
             GPIO27 ◀───┤                 │
                        └──[15 kΩ]────────┴── GND      →  5 V × 15/25 = 3.00 V

  Fail-safe bias (only if the module does not already have it):
        V5 ──[560 Ω]── A          B ──[560 Ω]── GND
```

Cheaper alternative that deletes the divider entirely: replace with a **MAX3485 / SP3485 3.3 V module** and run VCC from `V3V3`.

#### 4.5.4 Turbidity divider + anti-alias filter

```
   TURB out (0–4.5 V) ──[10 kΩ]──┬──[1 kΩ]──┬──▶ GPIO34 (ADC1_CH6)
                                  │          │
                             [20 kΩ]     [100 nF]
                                  │          │
                                 GND        GND
```

`4.5 V × 20/30 = 3.00 V` at full scale. Firmware: 64-sample average, median of 3 batches, then the calibration curve. Response is inverse and non-linear — clear water gives the *high* voltage.

#### 4.5.5 High-side +12 V sensor rail gate

```
                        VBATT (+12…14.6 V)
                              │
                    ┌─────────┴──────────┐
                 [10 kΩ]                 │ S
                    │             ┌──────┴──────┐
                    ├────── G ────┤   AO3401    │  P-channel
                    │             └──────┬──────┘
                 [10 kΩ]                 │ D
                    │                    ├──────────▶ V12_SENS (orange) → TB2-05
                    │                    │
              D ┌───┴────┐            [100 µF]   probe inrush / soft start
                │ 2N7002 │                │
              S └───┬────┘               GND
                    │        G ◀── GPIO32  (HIGH = rail ON)
                   GND
```

`GPIO32` HIGH → 2N7002 conducts → gate divider puts ≈6 V on the gate → `V_GS ≈ −6 V` → P-MOS fully enhanced. LOW → gate pulled to VBATT → off, zero probe standby current. Probe ground stays common with board ground, which is what RS-485 needs.

For IRF9540N instead of AO3401, use **10 kΩ top / 4.7 kΩ bottom** for a harder gate drive (`V_GS ≈ −8 V`).

#### 4.5.6 Battery input protection

```
  BATT+ ──[FUSE 3 A]──┬── S ┌───────────┐ D ──┬──────▶ VBATT bus
                      │     │ IRF9540N  │     │
                      │     └─────┬─────┘     │
                      │           │ G         │
                      │        [10 kΩ]   [SMBJ15A]
                      │           │           │
  BATT− ──────────────┴───────────┴───────────┴──────▶ GND (star point)
```

Correct polarity → gate pulled to GND → `V_GS` negative → conducts with ~0 V drop. Reversed → body diode blocks. Simpler fallback: a 3 A Schottky, at the cost of ~0.4 V and some heat.

#### 4.5.7 Battery and solar sense

```
  VBATT ──[100 kΩ]──┬──▶ GPIO35        PV+ ──[220 kΩ]──┬──▶ GPIO36
                    │                                   │
               [27 kΩ]  ┬─[100 nF]                 [33 kΩ]  ┬─[100 nF]
                    │   │                               │   │
                   GND GND                             GND GND

  15.0 V → 3.19 V                        22.0 V → 2.87 V   (25 V → 3.26 V headroom)
```

> ⚠️ A 12 V nominal panel has an open-circuit voltage of **~21–22 V**. A 100 kΩ/27 kΩ divider on PV+ would present ~4.7 V to the ADC and damage the pin (I-28). Use 220 kΩ/33 kΩ.

#### 4.5.8 JSN-SR04T echo, if you power it at 5 V

```
   Echo (5 V) ──[10 kΩ]──┬──▶ GPIO33
                          │
                     [15 kΩ]
                          │
                         GND
```

Only needed if the sensor is on `V5`. Powering it from `V3V3` is preferred and removes this circuit.

---

### 4.6 Internal terminal block / harness template

One 16-position screw terminal strip (or three smaller blocks) on the enclosure floor. Every external cable lands here — **nothing external solders directly to a module.**

| TB | Pos | Net | Colour | To (external) | From (internal) | ✓ |
|---|---|---|---|---|---|---|
| TB1 | 01 | `BATT+` | Red | Charge controller LOAD+ | Fuse → rev-pol P-MOS | ☐ |
| TB1 | 02 | `BATT−` | Black | Charge controller LOAD− | Star ground | ☐ |
| TB1 | 03 | `PV_SENSE` | White | Charge controller PV+ | 220 kΩ divider | ☐ |
| TB1 | 04 | `GND` | Black | — | Star ground | ☐ |
| TB2 | 05 | `V12_SENS` | Orange | RS-485 cable, conductor 1 | P-MOS drain | ☐ |
| TB2 | 06 | `GND` | Black | RS-485 cable, conductor 2 | Star ground | ☐ |
| TB2 | 07 | `RS485_A` | Yellow | RS-485 cable, conductor 3 | Surge module A-out | ☐ |
| TB2 | 08 | `RS485_B` | Green | RS-485 cable, conductor 4 | Surge module B-out | ☐ |
| TB2 | 09 | `SHIELD` | Bare | RS-485 cable drain wire | Enclosure ground stud **(this end only)** | ☐ |
| TB3 | 10 | `V3V3` | Red | Ultrasonic VCC | ESP32 3V3 | ☐ |
| TB3 | 11 | `GND` | Black | Ultrasonic GND | Star ground | ☐ |
| TB3 | 12 | `US_TRIG` | Blue | Ultrasonic Trig | GPIO25 | ☐ |
| TB3 | 13 | `US_ECHO` | Blue | Ultrasonic Echo | GPIO33 | ☐ |
| TB4 | 14 | `V5` | Red | Turbidity probe head + | Turbidity module | ☐ |
| TB4 | 15 | `GND` | Black | Turbidity probe head − | Star ground | ☐ |
| TB4 | 16 | `TURB_SIG` | White | Turbidity probe head signal | Turbidity module input | ☐ |

**Star ground rule:** all `GND` terminals bond to **one** point — the battery-negative stud. Do not daisy-chain grounds through module boards, and keep the modem's ground return on its own leg to that stud.

---

### 4.7 External cable schedule template (per node)

| # | Cable | Conductors | Gland | Length | Route | Shield bonded at | ✓ |
|---|---|---|---|---|---|---|---|
| C1 | RS-485 sensor bus | A, B, +12 V, GND + drain (shielded 4-core TP) | PG9 | ____ m | Conduit → soil probe → (later: water pH, water EC) | Enclosure only | ☐ |
| C2 | Ultrasonic | VCC, GND, Trig, Echo (4-core shielded) | PG9 | ____ m | Bracket over stilling well | Enclosure only | ☐ |
| C3 | Turbidity probe head | V+, GND, Signal (3-core shielded) | PG7 | ____ m | Into water, probe head only | Enclosure only | ☐ |
| C4 | Solar / battery | +, − (2-core, ≥18 AWG) | PG9 | ____ m | To charge controller / battery box | n/a | ☐ |
| C5 | GSM antenna | Coax | **SMA bulkhead, not a gland** | ____ m | Up the pole, above the enclosure | Chassis at bulkhead | ☐ |

Rules: all entries on the **bottom face**, glands pointing down, drip loop on every cable, spare gland positions blanked with proper plugs (not tape).

---

### 4.8 RS-485 bus & Modbus address register (fill per node)

```
  MAX485 ──[surge module]── TB2 ═══════╦═══════════╦═══════════╗
       │                               ║           ║           ║
   [120 Ω]                          soil probe  water pH    water EC
   board end                        0x01        0x02        0x03
                                                            [120 Ω] ← far end
```

| Slave | Device | Address | Baud | Parity | Registers used | Cal. date | ✓ |
|---|---|---|---|---|---|---|---|
| 1 | Soil 5-in-1 (pH/NPK/T/moisture/EC) | 0x01 | ____ | 8N1 | ____ | ____ | ☐ |
| 2 | Water pH *(pending purchase)* | 0x02 | ____ | 8N1 | ____ | ____ | ☐ |
| 3 | Water EC K=10 *(pending purchase)* | 0x03 | ____ | 8N1 | ____ | ____ | ☐ |

Set and record addresses **on the bench with a USB-RS485 dongle**, then write them on the label inside the lid. The used soil sensor may arrive on a non-default address (I-16). Termination goes on the **two physical ends only** — with one probe that means one resistor at the board and one at the probe.

---

### 4.9 Enclosure layout template

Zone by "dirtiness" so modem bursts and surge paths stay away from the ADC. Cable entries on the bottom edge.

```
  ┌──────────────────────────── 158 × 90 mm lid ─────────────────────────────┐
  │                                                                          │
  │  ┌──────────────┐   ┌────────────────────┐   ┌──────────────────────┐   │
  │  │ POWER-IN     │   │  MCU              │   │  MODEM              │   │
  │  │ fuse, P-MOS, │   │  ESP32 dev board  │   │  SIM800L + 1000 µF  │──▶ SMA
  │  │ TVS, LM2596  │   │  microSD module   │   │  own GND leg        │  bulkhead
  │  │ ×2, gate FET │   │                   │   │                     │   │
  │  └──────────────┘   └────────────────────┘   └──────────────────────┘   │
  │                                                                          │
  │  ┌──────────────────────┐         ┌────────────────────────────────┐    │
  │  │ ANALOG (quiet)       │         │ RS-485 + PROTECTION            │    │
  │  │ turbidity module,    │         │ MAX485, surge module, SM712,   │    │
  │  │ dividers, RC filters │         │ 120 Ω  — hard against TB2      │    │
  │  └──────────────────────┘         └────────────────────────────────┘    │
  │                                                                          │
  │  ══ TB1 ══  ══ TB2 ══  ══ TB3 ══  ══ TB4 ══   [desiccant]  [vent]      │
  └───┬─────────┬──────────┬──────────┬───────────────────────────────────────┘
    PG9 C4    PG9 C1     PG9 C2     PG7 C3          (bottom face, glands down)

  ⚠ Battery is NOT in this box — see I-15. Separate IP67 battery box, short
     ≥18 AWG cable to TB1.
```

Placement rules: SIM800L and its bulk cap as far from the analog zone as the box allows; protection components between the terminal block and everything active; nothing mounted on the lid; modules on standoffs over a sub-plate.

---

## 5. Per-unit build sheet — copy one per node

> Save as `docs/as_built/node-01/build_sheet.md`. A node does not ship until this sheet is complete and signed. This is also the document a field engineer reads at 2 a.m. in the rain, so keep it accurate.

### Identity

| Field | Value |
|---|---|
| Node ID | `SBN-___` |
| Built by / date | ____________ / ________ |
| Firmware version + git tag | ____________ |
| GPS (lat, lon) | ____________ |
| Install site description | ____________ |
| Mount height of ultrasonic above datum (**needed for the level calculation**) | ______ m |
| Soil probe burial depth | ______ cm |

### Part identification (fill during S0)

Every row here resolves a defect whose fix *depends on which variant you actually received*. Filling this table is not paperwork — it is how S1 knows what to wire.

| Part | Marking / model observed | Notes |
|---|---|---|
| ESP32 module variant | WROOM-32 ☐ / WROVER ☐ | **I-34** — on WROVER, GPIO16/17 are taken by PSRAM; the §4.2 UART2 assignment breaks silently. Remap and update §4.2 |
| ESP32 board form factor | NodeMCU-32S 30-pin ☐ / other: ________ | **I-38** — if it is not a 30-pin WROOM board (e.g. an S3, a C3, or a LILYGO variant), §4.2 must be re-derived before any wiring. See §11.2 |
| MAX485 IC marking | ____________ | **I-06** — MAX485 (5 V) ☐ / MAX3485 or SP3485 (3.3 V) ☐ → sets whether the RO divider is needed |
| microSD module type | ____________ | **I-08** — LDO + level shifter ☐ / bare adapter ☐ → sets its supply rail |
| SIM800L SIM slot | micro ☐ / nano ☐ | **I-12** — adapter fitted? ☐ |
| Turbidity max output measured in clear water | ______ V | **I-04** — confirms the 10 k/20 k divider |
| Battery dimensions measured | ____ × ____ × ____ mm | **I-15** — fits main box? ☐ yes ☐ **no → separate box** |
| Charge controller LiFePO4 profile? | yes ☐ / no ☐ | **I-21** — if no, do not buy it |
| Solar panel Voc measured in open sun | ______ V | **I-28** — confirms the 220 k/33 k tap; if Voc > 24 V, recompute |

### Measured rails (fill during S2)

| Rail | Target | Measured | Sag on modem TX | ✓ |
|---|---|---|---|---|
| VBATT | 12.0–14.6 V | ______ V | — | ☐ |
| V4V1 (modem) | 4.10 ±0.10 V | ______ V | ______ V | ☐ |
| V5 | 5.00 ±0.15 V | ______ V | ______ V | ☐ |
| V3V3 | 3.30 ±0.10 V | ______ V | ______ V | ☐ |
| V12_SENS ON | ≥ VBATT − 0.3 V | ______ V | — | ☐ |
| V12_SENS OFF | ≤ 0.2 V | ______ V | — | ☐ |

### Measured currents & autonomy

| Metric | Value | Notes |
|---|---|---|
| Deep-sleep current at battery terminals | ______ mA | **The number the whole power budget rests on** |
| Peak current during GPRS transmit | ______ A | |
| Average current over one full cycle | ______ mA | |
| Energy per cycle | ______ mWh | |
| Calculated autonomy, no sun | ______ days | Target ≥ 10 |

### Divider ratios as built (put these in the firmware config)

| Divider | R_top measured | R_bottom measured | Ratio | ✓ |
|---|---|---|---|---|
| Turbidity | ______ | ______ | ______ | ☐ |
| VBAT sense | ______ | ______ | ______ | ☐ |
| VSOL sense | ______ | ______ | ______ | ☐ |
| SIM RXD level shift | ______ | ______ | ______ V out | ☐ |
| MAX485 RO (if 5 V) | ______ | ______ | ______ V out | ☐ |

### Cellular

| Field | Value |
|---|---|
| SIM number / ICCID | ____________ |
| Carrier + APN | ____________ |
| 2G confirmed at mount point? | ☐ yes — `AT+CSQ` = ______ |
| MQTT broker + topic | ____________ |
| Per-node credential set? | ☐ |

### Sensors & calibration

| Sensor | Serial / ID | Modbus addr | Baud | Calibrated | Standard used | Next due |
|---|---|---|---|---|---|---|
| Soil 5-in-1 | | 0x01 | | ☐ | | |
| Water pH | | 0x02 | | ☐ | | |
| Water EC K=10 | | 0x03 | | ☐ | 1413 µS/cm ☐ + 12.88 mS/cm ☐ | |
| Turbidity | | analog | — | ☐ | | |
| Ultrasonic | | Trig/Echo | — | ☐ tape-measure check | | |

### Sign-off

| Gate | Signed | Date |
|---|---|---|
| S1 bench bring-up complete | | |
| S2 power characterised | | |
| S3 conditioning verified, no pin over 3.3 V | | |
| S4 100-cycle soak passed | | |
| S6 7-day enclosure soak passed | | |
| Cleared for field install | | |

---

## 6. Bench bring-up test procedure

Run in order. Record the actual number, not a tick — "works" is not data. Fill one copy per node.

| ID | Test | Method | Expected | Measured | P/F |
|---|---|---|---|---|---|
| T1.1 | ESP32 boots | Serial console at 115200 | Boot banner, no brownout messages | | ☐ |
| T1.2 | Deep sleep + timer wake | 30 s sleep, log wake reason | Wakes on `TIMER`, boot count increments | | ☐ |
| T2.1 | SD mount | `SD.begin(SD_CS)` | Card type + size printed | | ☐ |
| T2.2 | SD write/read persistence | Append row, power-cycle, read back | Row intact | | ☐ |
| T2.3 | SD supply correctness | Measure module VCC and MISO high level | MISO ≤ 3.3 V | | ☐ |
| T3.1 | RS-485 direction control | Scope DE/RE while transmitting | Clean transitions, no bus contention | | ☐ |
| T3.2 | **RO level into ESP32** | DMM/scope on GPIO27 with bus idle high | **≤ 3.3 V** (fails if MAX485 at 5 V without divider) | | ☐ |
| T3.3 | Loopback to PC | USB-RS485 dongle, echo a string | String returns intact | | ☐ |
| T3.4 | Fail-safe bias | All probes unpowered, read bus | Defined idle state, no random framing errors | | ☐ |
| T4.1 | Soil probe discovery | Scan addresses 0x01–0x10 at 4800 and 9600 | Exactly one responds; record addr + baud | | ☐ |
| T4.2 | Soil probe full read | Function 0x03, all documented registers | CRC valid, plausible values in moist soil | | ☐ |
| T4.3 | Soil probe supply range | Vendor datasheet + test at 12 V | Operates on gated 12 V | | ☐ |
| T5.1 | Ultrasonic accuracy | Tape measure at 0.3 / 1.0 / 2.0 / 4.0 m | Within ±2 cm after temperature compensation | | ☐ |
| T5.2 | Ultrasonic dead zone | Target at 10 cm | Invalid/clamped reading — firmware must reject it | | ☐ |
| T5.3 | **Echo pin level** | DMM on echo during a ping | **≤ 3.3 V** | | ☐ |
| T6.1 | Turbidity raw range | 5 V supply, clear water vs stirred sediment | Clear ≈ 4.0–4.5 V, muddy well below | | ☐ |
| T6.2 | **Turbidity after divider** | Measure at GPIO34 with clear water | **≤ 3.15 V** | | ☐ |
| T6.3 | Turbidity ADC stability | 100 samples, still water | Spread < 2 % after averaging | | ☐ |
| T7.1 | Modem power-up | 4.10 V rail, PWRKEY pulse | `AT` → `OK` | | ☐ |
| T7.2 | Signal quality | `AT+CSQ` with external antenna | ≥ 10 (below 8, relocate the antenna) | | ☐ |
| T7.3 | Registration | `AT+CREG?` | `0,1` or `0,5` | | ☐ |
| T7.4 | Network time | `AT+CCLK?` | Correct date/time, sane timezone | | ☐ |
| T7.5 | GPRS attach | APN from the build sheet | IP address assigned | | ☐ |
| T7.6 | MQTT publish | Publish to test topic | Received at broker | | ☐ |
| T7.7 | **Burst stability** | 50 consecutive publishes, scope VBAT and V5 | VBAT sag ≤ 0.4 V, V5 sag ≤ 0.25 V, **0 ESP32 resets** | | ☐ |
| T8.1 | Modem sleep | `AT+CSCLK=2`, then wake | Enters and exits sleep; current drops | | ☐ |
| T9.1 | Rail gate ON | GPIO32 HIGH | V12_SENS ≥ VBATT − 0.3 V | | ☐ |
| T9.2 | Rail gate OFF leakage | GPIO32 LOW | V12_SENS ≤ 0.2 V, probe current ≈ 0 | | ☐ |
| T10.1 | Full cycle | One wake-to-sleep cycle | Row on SD **and** at the broker, timestamps match | | ☐ |
| T10.2 | Outage backfill | Antenna removed for 10 cycles | 10 rows queued, all backfilled in order, no duplicates | | ☐ |
| T10.3 | Power-loss during write | Cut power mid-SD-write | Log still mountable, ≤1 row lost | | ☐ |
| T10.4 | Watchdog | Force a hang in firmware | Node resets and resumes | | ☐ |
| T10.5 | Modem recovery | Hold the modem in a failed state | PWRKEY power-cycle recovers it with backoff | | ☐ |
| T11.1 | 100-cycle soak | Unattended run | 0 unexplained resets, 100/100 rows logged | | ☐ |
| T11.2 | 7-day enclosure soak | Sealed, solar + battery | 0 resets, no condensation, battery holds | | ☐ |

**Any test in bold is a safety test for the ESP32.** Do not proceed past a failed bold test — you will be debugging a slowly dying GPIO for weeks.

---

## 7. Compatibility issues & required improvements

Every item below is a real mismatch between the parts as bought and what the design needs. Nothing here is theoretical.

**Severity:**

- 🔴 **P0** — will destroy hardware, prevent a working build, or leave a required measurement missing. Fix before the stage named.
- 🟠 **P1** — works on the bench, fails or corrupts data in the field. Fix before S7 (field install).
- 🟡 **P2** — accuracy, maintainability, future-proofing. Schedule, don't ignore.

### 7.1 Register

| ID | Sev | Item | The problem | Fix | Fix before |
|---|---|---|---|---|---|
| I-01 | 🔴 | SIM800L Mini | VBAT range is **3.4–4.4 V**. It is *not* a 5 V module. 5 V will damage it | Dedicate LM2596 #1, trimmed to **4.10 V** under load, pot locked and labelled | S2 |
| I-02 | 🔴 | SIM800L Mini | GSM TDMA transmit draws **~2 A bursts**. The 470 µF on the BOM is marginal; the rail collapses and the ESP32 brown-out-resets | **≥1000 µF low-ESR ∥ 470 µF ∥ 100 nF within 20 mm of VBAT**, ≥20 AWG leads, modem on its own converter | S2 |
| I-03 | 🔴 | MOSFET module received is **IRF520**, not the IRLZ44N specified | IRF520 is **not logic-level** — R_DS(on) is specified at V_GS = 10 V; a 3.3 V gate leaves it barely conducting and hot. It is also **low-side**, which breaks the shared ground RS-485 needs | Build the **high-side P-MOS gate** in §4.5.5 (AO3401 + 2N7002). Keep the IRF520 modules as spares for ground-referenced loads | S2 |
| I-04 | 🔴 | Turbidity module | Output swings to **~4.5 V**; ESP32 ADC absolute maximum is 3.3 V. Direct connection damages the pin | **10 kΩ / 20 kΩ divider + 1 kΩ/100 nF RC** into GPIO34 (ADC1). Verify ≤3.15 V before connecting | S3 |
| I-06 | 🔴 | MAX485 module | The classic red MAX485 breakout carries a **5 V-only transceiver (4.75–5.25 V)**. Run it at 5 V and its **RO output is 5 V logic straight into an ESP32 input**. Run it at 3.3 V and it is out of spec — it may work today and fail at temperature | **Check the IC marking.** MAX485 → run VCC at 5 V and fit a **10 kΩ/15 kΩ divider on RO**. Better: swap to a **MAX3485/SP3485 3.3 V module** and delete the divider (**D-4**) | S3 |
| I-09 | 🔴 | JSN-SR04T-V3.3 | The Echo output level follows its supply. Powered at 5 V it presents **5 V to GPIO33** | **Power it from `V3V3`** (preferred — the V3.3 variant is made for this), or add a 10 kΩ/15 kΩ divider on Echo | S3 |
| I-12 | 🔴 | SIM + modem | You have a **nano** SIM; SIM800L Mini boards normally carry a **micro-SIM** holder. Physical mismatch — the node cannot connect at all | Nano→micro **SIM adapter** ×3. Verify the actual slot on the unit during S0 | S1 |
| I-15 | 🔴 | Enclosure vs battery | The 158 × 90 × **65 mm** box cannot hold a 12 V 6 Ah LiFePO4 — packs in that capacity are typically **~151 × 65 × 94 mm** (SLA form factor), taller than the box's internal depth. The BOM note ("houses … battery") and the stale LILYGO reference are both wrong | **Measure the actual pack in S0.** Plan a **separate IP67 battery box** with a short ≥18 AWG feed to TB1. Do not shop for a bigger main box until the pack is measured | S6 |
| I-19 | 🔴 | Procurement quantity | 3 sets of electronics, but **zero** power/enclosure sets and only **1 microSD card**. Nothing can be assembled into a station today | Decide D-1 (pilot-first vs fleet-first) and order per §2.3 / §3.2 | S2 |
| I-28 | 🔴 | Solar voltage sense | A 12 V nominal panel's **open-circuit voltage is ~21–22 V**. The spec's 100 kΩ/27 kΩ divider would put **~4.7 V** on GPIO36 and damage it | Use **220 kΩ / 33 kΩ** on the PV+ tap (§4.5.7). 100 kΩ/27 kΩ is correct only for the battery tap | S3 |
| I-38 | 🔴 | **MCU board identity not confirmed** | The board in the previous-build photos (§11) shows **two USB receptacles and an on-board RGB LED**, which a classic NodeMCU-32S (WROOM-32, one micro-USB, blue LED) does not have. That pattern matches an **ESP32-S3 / C3-class devkit or a LILYGO-family board** instead. If the build board is not a 30-pin WROOM-32, **the entire §4.2 GPIO map is invalid** — S3 boards renumber almost everything, have no GPIO16/17 UART convention, and route native USB on GPIO19/20, which §4.2 assigns to SD_MISO | **Read the module can marking and the silkscreen before any wiring** and record it on the §5 part-identification table. If it is not WROOM-32 on a 30-pin board, re-derive §4.2 from that board's datasheet — do not adapt it pin-by-pin. Assumption **A-2** depends on this | **S0 — before S1** |
| I-39 | 🔴 | **Solderless breadboard + DuPont jumpers as the interconnect** | The previous build (§11) is entirely breadboard-and-jumper. Breadboard spring contacts are a high-resistance, un-sealable joint: contact resistance climbs with humidity and salt film, and the 2 A modem burst across a springy contact is exactly how "random reboots" are manufactured. Jumper pins also back out under thermal cycling and vibration | **No breadboard and no bare DuPont jumper in any field node.** Solder to protoboard, or crimp latching connectors (JST-XH / Molex), and land every off-board wire on the §4.6 terminal block. Breadboard is for S1 bench work only, and S1 explicitly ends before the power tree | S2 |
| I-05 | 🟠 | Turbidity module | The probe head is sealed but the **comparator PCB is bare** — it is not an IP68 assembly, despite the BOM line reading "potted/IP68" | Mount the PCB **inside the enclosure**; only the probe head and its cable go in the water. Pot the cable-to-head joint | S6 |
| I-07 | 🟠 | 5 V supply | Received part is a **2 A USB-output** step-down, not the 5 V/3 A screw-terminal module specified. A USB-A plug is a poor connection in a humid, vibrating enclosure, and 2 A leaves thin headroom | **Solder leads to the module's output pads** (no USB cable in the final build), or better: buy a **second LM2596** and set it to 5.00 V — you already trust that part (**D-5**) | S2 |
| I-08 | 🟠 | microSD module | Two common variants: bare adapter (3.3 V) and AMS1117 + 74LVC125 level-shifter type (needs **5 V** to work correctly). Feeding the wrong one the wrong rail gives intermittent mount failures that look like a bad card | Identify the module in S0 and set its rail accordingly (§4.4.3). Verify MISO high level ≤3.3 V (T2.3) | S1 |
| I-10 | 🟠 | ESP32 dev board | The onboard **AMS1117** (~5–11 mA quiescent), USB-serial chip and power LED keep deep-sleep current in the **milliamp** range, not microamps. With 6 Ah this is what decides whether the node survives a cloudy week | **Measure it in S2**, then: put the modem in `AT+CSCLK=2`, remove the power LED, consider lifting the USB-serial chip's supply, and/or move to a bare **ESP32-WROOM-32E** in Phase B. If autonomy < 10 days, go to a **12 Ah** pack (**D-3**) | S2 |
| I-11 | 🟠 | No RTC in the BOM | Timestamps come from `AT+CCLK`. If the modem can't attach — the exact scenario where store-and-forward matters most — queued rows get **drifting or wrong timestamps** | Fit a **DS3231** on the reserved I²C pins (§4.4.8). Sync it from the network when available, read it when not | S4 |
| I-13 | 🟠 | Modem antenna | The stub/helical antenna shipped with SIM800L modules performs badly. At a fringe rural site it is the difference between attaching and not | **External GSM antenna + SMA bulkhead**, mounted high, plus a **surge arrestor at the bulkhead** (a tall antenna is a lightning target) | S6 |
| I-14 | 🟠 | SIM800L is **2G only** | The entire uplink depends on 2G being alive at the site. Carrier 2G retirement kills every node at once | Confirm 2G at the exact mount point with `AT+CSQ` during the S7 site survey; SD logging covers outages; put an **LTE Cat-1/NB-IoT footprint (A7670 / SIM7000)** on the Phase-B PCB so migration is a component swap | S7 / Phase B |
| I-16 | 🟠 | One soil sensor is **used** | It may carry a non-default Modbus address, a different baud rate, or drifted calibration. Dropped onto a shared bus it can collide with another slave | Bench-interrogate it with a USB-RS485 dongle **before** it touches a node; reassign its address; recalibrate; record on the build sheet | S1 |
| I-17 | 🟠 | Soil probe spec unknown | Register map, scaling, baud and **supply voltage range** vary by vendor. The design assumes it accepts the gated 12 V rail | Get the vendor Modbus map, confirm the supply range, document in `docs/modbus_soil_probe.md`. If it is 5 V-only, the gated rail must be 5 V for that probe | S1 |
| I-20 | 🟠 | Termination resistors specified as **1206 SMD** | Awkward to fit on a Phase-A protoboard and at the far end of a field cable, where you need a resistor across a screw terminal. Not yet purchased, so this is cheap to correct | Buy **through-hole 120 Ω ¼ W** for Phase A (2 per node). Order the 1206 parts later, only for the Phase-B PCB | S3 |
| I-21 | 🟠 | Charge controller not yet chosen | Many cheap PWM controllers are **lead-acid only**, and some run a periodic **equalisation stage at 14.8–15 V** that will trip a LiFePO4 BMS and leave the station dead until someone visits | Buy a controller with a **LiFePO4 profile** (≈14.4–14.6 V absorb, 13.6 V float, **no equalisation**). Also check its own quiescent draw — it counts against autonomy | S2 |
| I-23 | 🟠 | No input protection on the BOM | No fuse, no reverse-polarity protection, no TVS on the battery input, and no antenna surge arrestor. Monsoon lightning near an unattended coastal station is routine | Fit **3 A fuse + reverse-polarity P-MOS + SMBJ15A** (§4.5.6) in S2; **SM712 + the RS-485 surge module** in S3; **GSM arrestor at the antenna bulkhead** in S6; earth the mounting pole (see I-37) | S2 / S3 |
| I-24 | 🟠 | Calibration standards | Only **1413 µS/cm** is on the list. A **K=10** probe reads brackish water in the **10–50 mS/cm** range — 1413 µS/cm sits at the very bottom of its span and cannot calibrate it | Add a **12.88 mS/cm** standard. Two-point calibration (1413 µS/cm + 12.88 mS/cm) | S7 |
| I-33 | 🟠 | Water pH and water EC probes **not purchased** | Two of the headline water parameters cannot be measured. The pilot ships measuring **soil + level + turbidity** only | Explicit, documented scope decision (**D-2**). Bus, addresses (0x02/0x03), firmware and cable are already provisioned — adding them later is a plug-in, not a redesign | S7 scope |
| I-36 | 🟠 | Turbidity sensor type | This class of module is a **transmission-type** optical sensor designed for a still, clean flow path. In tidal water it sees bubbles, floating debris and will biofoul within weeks | Mount inside a **stilling well** or a shrouded holder; median-filter aggressively; schedule optical-window cleaning at every service visit and log cleaning dates | S7 |
| I-40 | 🟠 | **Hot glue used as strain relief and fixing** | Visible throughout the previous build (§11). Hot-melt adhesive softens at roughly **60–70 °C** — a clear-lid enclosure in Sundarbans sun reaches that (I-27) — and it creeps under load and traps moisture against copper rather than sealing it out | Mechanical fixing first: standoffs, cable ties to anchored bases, P-clips, and a gland doing the actual strain relief. Where a flexible bead is genuinely needed use **neutral-cure RTV silicone** — not acetoxy RTV, which releases acetic acid and corrodes copper inside a sealed box | S6 |
| I-41 | 🟠 | **Flat rainbow ribbon cable on signal runs** | The previous build (§11) uses ribbon throughout. Ribbon has **no twisted pair and no shield**, which discards the entire reason RS-485 survives a 15 m outdoor cable sitting next to a 2 A GSM burst. Expect CRC failures that look exactly like a failing probe | **Shielded twisted pair for A/B** (§4.7 cables C1/C2), shield bonded at the enclosure end only. Ribbon is fine inside the box for short low-speed hops, never for an off-board differential pair or a long analog run | S3 |
| I-18 | 🟡 | Ultrasonic physics | ~20–25 cm dead zone, ~4.5 m max range, ~60° beam, and speed of sound drifts ~0.6 m/s per °C (≈2–3 % error across a tropical day) | Mount so the whole tidal range sits inside 0.3–4.0 m; **stilling well**; temperature-compensate from the soil probe's temperature register; median of 7 pings | S7 |
| I-22 | 🟡 | Cable budget | 15 m of shielded 4-core total, for a bus that must reach a buried soil probe plus two future water probes, per node | Measure the site before cutting. Budget per node, not per project; buy the reel length once C1–C3 runs are known | S6 |
| I-25 | 🟡 | No enclosure T/RH sensor | Condensation failures look identical to random faults. Without internal RH you are guessing | Add **SHT31 or AHT20** on the reserved I²C pins; publish with housekeeping | S6 |
| I-26 | 🟡 | Sealed box, no vent | Daily thermal cycling pumps humid air past the glands and condenses it inside. Desiccant alone saturates | Fit a **Gore-type breather vent** plus the silica gel; recharge desiccant every service visit | S6 |
| I-27 | 🟡 | Clear-lid enclosure in tropical sun | Greenhouse heating raises internal temperature, derating the modem and shortening LiFePO4 life; UV embrittles the lid | Sun-shield the box, or use an opaque UV-stable enclosure. Never mount the panel on the box | S6 |
| I-29 | 🟡 | Soil NPK in saline soil | These probes infer NPK from EC. Na⁺/Cl⁻ in Sundarbans soil inflates the reading — the numbers are not agronomic-grade | Publish raw values but label them **relative trend only** in the schema and on the dashboard, so nobody downstream over-reads them | S5 |
| I-30 | 🟡 | Transport security | SIM800L's TLS stack is old and weak-ciphered; practical deployments end up publishing in the clear | Unique credentials per node, non-default port, server-side validation, no sensitive payloads. Revisit when the LTE modem lands | S5 |
| I-31 | 🟡 | OTA over 2G | Full OTA over GPRS is slow and prone to half-flashed images | Plan **local USB updates at service visits**. If remote update is essential, make it resumable and checksum-verified, and only over `CSQ ≥ 15` | S4 |
| I-32 | 🟡 | Biofouling | Submerged optical and EC surfaces foul within weeks in warm brackish water; readings drift silently | Cleaning schedule; copper tape/mesh guards near optical windows; log clean + calibration dates; consider a wiper-equipped probe when budget allows | S7 |
| I-34 | 🟡 | ESP32 module variant | On **WROVER** (PSRAM) modules GPIO16/17 are consumed by the PSRAM and the UART2 assignment in §4.2 silently breaks | Confirm the marking is **WROOM-32** in S0; if WROVER, remap UART2 and update §4.2 | S1 |
| I-35 | 🟡 | SD card | One 32 GB consumer card in hand for three nodes; consumer cards wear and fail in field heat with frequent appends | **High-endurance / industrial** cards, ≤32 GB (FAT32), one per node plus a spare. Append-and-flush, never hold a file open across sleep | S4 |
| I-37 | 🟡 | Earthing | A pole-mounted antenna and a 15 m sensor cable with no earth reference makes surge protection largely decorative | Proper earth rod at the mount; bond enclosure and arrestor to it; shield bonded at the enclosure end only | S7 |

### 7.2 What to fix first

If you only do six things before touching the soldering iron:

1. **I-38** — confirm the MCU board is a 30-pin WROOM-32. This costs thirty seconds and it decides whether §4.2 is a wiring map or a work of fiction. Do it first.
2. **I-01 + I-02** — trim the modem rail to 4.10 V and fit ≥1000 µF at VBAT. Skipping this produces weeks of "random reboots".
3. **I-06** — read the MAX485 IC marking and decide the RO path. This is the most-missed defect on this platform and it damages the ESP32 slowly.
4. **I-04 + I-28** — build both dividers before any analog wire reaches a GPIO.
5. **I-03** — build the high-side P-MOS gate; put the IRF520 modules in the spares drawer.
6. **I-15 + I-19** — measure the battery and place the power order, or S2 onward stays blocked.

### 7.3 Improvements deliberately deferred

Not defects — conscious Phase-B scope, recorded so they don't get rediscovered as surprises: isolated RS-485 (ADM2582E) if ground loops appear; bare ESP32-WROOM-32E for real microamp sleep; LTE Cat-1 modem footprint; 4-layer board with a continuous ground plane and ENIG finish; conformal coating as a process step rather than a spray afterthought; dissolved-oxygen probe on the spare Modbus address. All of these are specified in `Sundarbans_Monitoring_Station_Design_Spec.md` §12.

---

## 8. Firmware

### 8.1 Repo layout (proposed)

```
firmware/
  src/
    main.cpp              state machine (spec §14.1)
    config.h/.cpp         load/save config from SD or NVS
    sensors_modbus.cpp    RS-485 Modbus master, per-probe timeout + CRC
    sensors_analog.cpp    turbidity + housekeeping dividers, oversampling
    sensors_ultrasonic.cpp  ping, median, temperature compensation
    storage_sd.cpp        CSV append, sent_flag, backfill cursor
    net_gprs.cpp          TinyGSM lifecycle, CSQ, CCLK, recovery
    net_mqtt.cpp          publish + downlink cmd handling
    power.cpp             rail gating, sleep entry/exit, brown-out policy
    diag.cpp              boot count, reset reason, watchdog
  config/
    node-01.json          per-node config, committed
  docs/
    modbus_soil_probe.md  register map (fill in S1)
```

### 8.2 Libraries

`TinyGSM` (SIM800 profile) · `PubSubClient` over `TinyGsmClient` · `ModbusMaster` (or a small custom RTU master) · `SdFat` · `esp_sleep` + RTC-memory state · `esp_adc_cal` for ADC linearisation.

### 8.3 Config template (`config/node-01.json`)

```json
{
  "node_id": "SBN-01",
  "interval_s": 900,
  "apn": {"name": "", "user": "", "pass": ""},
  "mqtt": {"host": "", "port": 1883, "user": "", "pass": "",
           "topic_data": "sundarbans/SBN-01/data",
           "topic_cmd":  "sundarbans/SBN-01/cmd"},
  "modbus": {"baud": 9600, "soil_addr": 1, "wph_addr": 2, "wec_addr": 3,
             "timeout_ms": 500, "retries": 2},
  "ultrasonic": {"mount_height_m": 0.0, "pings": 7, "min_m": 0.30, "max_m": 4.00},
  "dividers": {"turb": 0.667, "vbat": 0.213, "vsol": 0.130},
  "turbidity_cal": {"clear_v": 4.40, "curve": "inverse_poly2", "coeff": [0,0,0]},
  "thresholds": {"vbat_low_v": 12.0, "ec_alarm_mScm": 30.0},
  "features": {"ds3231": true, "sht31": true, "modem_sleep": true}
}
```

Every number in `dividers` and `ultrasonic.mount_height_m` comes from the **§5 build sheet**, not from this template. A node with a copy-pasted config reports wrong numbers convincingly.

### 8.4 Data schema

**SD CSV** — one row per cycle, header written once:

```
iso_time,node_id,soil_temp,soil_moist,soil_ph,soil_ec,soil_n,soil_p,soil_k,
water_level_m,water_ph,water_ec_mScm,turbidity_ntu,vbat_v,vsol_v,csq,boot,rst,uptime_s,
encl_temp,encl_rh,sent_flag
```

**MQTT JSON** — as in spec §14.4. Keep the field names identical between CSV and JSON so backfill and live rows land in the same schema.

### 8.5 Non-negotiable firmware rules

1. **Log to SD before attempting to transmit.** The SD row is the record; the cloud is a mirror.
2. **Every stage times out.** No blocking call without a deadline. A dead probe must not cost a cycle.
3. **Always reach `SLEEP`.** Any failure path still turns rails off and re-arms the timer.
4. **Never hold a file handle across deep sleep.** Append, flush, close (I-35).
5. **Rails off before sleep**, GPIO in defined states, modem asleep or gated.
6. **Log reset reason and boot count every cycle.** Field diagnosis is impossible without them.
7. **No magic numbers.** Calibration and divider ratios live in config, sourced from the build sheet.

---

## 9. Open decisions

| # | Decision | Options | Recommendation | Owner | Status |
|---|---|---|---|---|---|
| D-1 | Buy power + enclosure for 1 node or 3? | pilot-first / fleet-first | **Pilot-first** — validate before spending 3× on parts a field failure might change | | ☐ |
| D-2 | Ship the pilot without water pH + EC? | wait / ship partial | **Ship partial.** Level + turbidity + full soil is a useful dataset; the bus is provisioned for the probes | | ☐ |
| D-3 | Battery 6 Ah or 12 Ah? | 6 / 12 | **Decide after the S2 sleep-current measurement**, not before | | ☐ |
| D-4 | MAX485 5 V + divider, or buy MAX3485? | divider / swap part | Divider now (parts in hand), MAX3485 on the Phase-B board | | ☐ |
| D-5 | Keep the 2 A USB buck or buy a second LM2596? | solder pads / buy | **Buy a second LM2596** — 250 BDT to remove a connector-reliability risk | | ☐ |
| D-6 | Ultrasonic Trig/Echo or UART mode? | GPIO / UART | UART is more robust over a long cable; decide after the **T5.1** accuracy test at final cable length | | ☐ |
| D-7 | Sampling interval | 15 / 30 / 60 min | 15 min for the pilot; lengthen if autonomy is tight and the science allows | | ☐ |
| D-8 | Fit DS3231 and SHT31? | yes / no | **Yes to both** — ~1,050 BDT for all three nodes (§3.2: 450 + 600), and both pay for themselves the first time you debug a field fault | | ☐ |
| D-9 | Cloud platform | ThingSpeak / self-hosted Mosquitto+Influx+Grafana | Self-hosted — you need offline alerts and a node registry | | ☐ |
| D-10 | Start the Phase-B PCB now or after the pilot? | now / after | **After S7.** The pilot will change the schematic | | ☐ |

---

## 10. Document control

### 10.1 Change log

| Version | Date | Change |
|---|---|---|
| v1.0 | 2026-08-28 | First README. Built against the final purchased-parts list. Supersedes spec §11.1 pinout; adds **41** tracked compatibility issues (I-01…I-41) and 10 open decisions. Adds §11, lessons carried forward from the previous breadboard build |

### 10.2 Deltas vs `Sundarbans_Monitoring_Station_Design_Spec.md` v0.2

Read these before using any figure from the spec:

| Topic | Spec v0.2 says | This README says | Why |
|---|---|---|---|
| Pin map | GPIO21 = SD_CS, GPIO22 = SIM_RST, GPIO13 = DE/RE, GPIO36 = SIM_STATUS, GPIO26 = TRIG, GPIO33 = SENS_EN | Reassigned — see §4.2 | Frees **GPIO21/22 for I²C** (DS3231, SHT31) and moves SIM_STATUS to GPIO39, keeping GPIO36 for the solar tap |
| SIM RXD level shift | 2.2 kΩ / 3.3 kΩ | **1.8 kΩ / 10 kΩ** | The spec's values give 1.98 V — on the SIM800L input threshold. New values give 2.80 V |
| MAX485 supply | "3.3 V → MAX485 logic" | **5 V + RO divider**, or swap to MAX3485 | MAX485 is a 5 V part; 3.3 V operation is out of spec (I-06) |
| Solar sense divider | 100 kΩ / 27 kΩ for both taps | **220 kΩ / 33 kΩ** on the PV tap | Panel Voc ~22 V, not 15 V (I-28) |
| MOSFET part | "IRLZ44N or IRF520" | IRF520 received; **use the AO3401 high-side circuit** | Purchase confirmed as IRF520 (I-03) |
| LILYGO T-Call | Open decision | **Closed — not in hand.** Discrete build | Not on the final purchase list (A-2) |
| Turbidity IP rating | "potted/IP68" per BOM | **Module PCB is not sealed** | Physical inspection required; mount PCB indoors (I-05) |
| Fleet size | Single node implied | **3 nodes** of electronics, 0 sets of power/enclosure | Purchase quantities (§2.3) |

### 10.3 Conventions for editing this file

Issue IDs (`I-nn`) and decision IDs (`D-n`) are **permanent** — never renumber. Close an issue by striking the row and adding the resolution date; do not delete it. When field reality contradicts this README, change the README in the same session, not "later".

---

## 11. Lessons from the previous build

Two photographs of the earlier prototype are the most useful engineering document we have, because they show what was actually done rather than what was planned. This section exists so those lessons become build rules instead of things someone remembers.

**What the photos show.** A solderless breadboard on plastic sheeting carrying an MCU board, two small green step-down modules, three large electrolytics, a blue 8-pin transceiver-style module, a red/purple module with a QR label, two blue modules with green screw terminals, DuPont and flat ribbon jumpers throughout, hot glue tacking wires down, and the JSN-SR04T transducer on its coiled cable. The second view adds PVC-potted probes and a large quantity of rainbow ribbon.

**The honest reading:** as a *bench feasibility rig* this was the right thing to build, and it did its job — it proved the sensors respond and the modules power up. As the basis for a field node it fails on interconnect, not on architecture. Every finding below is about how things are joined together, which is exactly the class of defect that passes on the bench and then fails three weeks into a monsoon.

### 11.1 Build rules carried forward

| # | What the photos show | Why it does not survive the Sundarbans | Rule for this build | Issue |
|---|---|---|---|---|
| L-1 | Solderless breadboard as the interconnect | Spring contacts are an un-sealable, high-resistance joint. Contact resistance rises with humidity and salt film, and the SIM800L's 2 A TDMA burst across a springy contact is precisely how "random reboots" get manufactured | Breadboard is permitted **only in S1**, on USB power, and S1 ends before the power tree exists. From S2 onward: soldered protoboard or crimped latching connectors | **I-39** |
| L-2 | DuPont jumpers on every signal | The friction-fit pins back out under thermal cycling and vibration, and the exposed crimp corrodes in salt air | Crimped **JST-XH / Molex** with a latch, or solder. Every off-board wire lands on the §4.6 terminal block — nothing off-board plugs straight onto a header | **I-39** |
| L-3 | Hot glue as strain relief and fixing | Hot-melt softens around **60–70 °C**, which a clear-lid box in tropical sun reaches (I-27). It also creeps under load and traps moisture against copper instead of excluding it | Standoffs, cable ties on anchored bases, P-clips, and glands doing the actual strain relief. If a flexible bead is genuinely needed, **neutral-cure RTV** — never acetoxy RTV, which outgasses acetic acid and corrodes copper in a sealed box | **I-40** |
| L-4 | Flat rainbow ribbon on signal runs | No twisted pair, no shield. Using it for RS-485 A/B discards the entire reason RS-485 tolerates a 15 m outdoor cable sitting beside a 2 A GSM burst; you get CRC failures that look exactly like a dying probe | **Shielded twisted pair** for A/B (§4.7 C1/C2), shield bonded at the enclosure end only. Ribbon is fine for short low-speed hops *inside* the box | **I-41** |
| L-5 | No fuse, no reverse-polarity device, no TVS anywhere in frame | An unattended coastal station sees monsoon lightning and, sooner or later, a reversed battery lead at 2 a.m. | Build the §4.5.6 input-protection block **before** the first battery connection in S2. This is not an optional hardening pass | **I-23** |
| L-6 | Modules loose on the bench, no sub-plate | Field vibration and shipping load fall entirely on the wiring | Modules on standoffs over a cut FR4/acrylic sub-plate per §4.9; nothing mounted on the lid | §3.8 |
| L-7 | Large electrolytics already present near the modem | Good instinct, and it should carry forward — but capacitance only works if it is **at** VBAT | ≥1000 µF low-ESR ∥ 470 µF ∥ 100 nF **within 20 mm of the SIM800L VBAT/GND pads**, on short thick leads. Bulk capacitance 100 mm away down a breadboard rail does close to nothing | **I-02** |
| L-8 | Probes potted into PVC by hand | Also the right instinct — but hand-potting is where water gets in | Pot the cable-to-head joint with a proper two-part epoxy or marine polyurethane, cure fully, then **pressure- or dunk-test each probe for 24 h before install**, not after | §3.9 |

### 11.2 The one thing to check before wiring anything

The board at top centre in both photos has **two USB receptacles and an on-board RGB LED**. A classic NodeMCU-32S (WROOM-32) has one micro-USB and a plain blue LED. Two ports plus an RGB LED is the signature of an **ESP32-S3 / C3-class devkit** (native USB alongside the UART bridge) or a LILYGO-family board.

This matters more than anything else in this document, because **§4.2 is derived specifically for a 30-pin WROOM-32 board**:

- On an **ESP32-S3**, the GPIO numbering is different end to end. GPIO19/20 carry native USB, and §4.2 assigns GPIO19 to `SD_MISO` — that alone breaks the SD card and possibly the USB port. ADC channel grouping, strapping pins and the input-only pins (34–39, which do not exist on S3) all change too.
- On a **WROVER** module, GPIO16/17 are consumed by PSRAM and the UART2 modem assignment fails silently (**I-34**).
- On a **LILYGO T-Call**, the SIM800L is already wired on-board to fixed pins and most of §4.4.1 becomes moot.

The photo resolution is not good enough to call this from here, and guessing would be worse than not knowing. **Read the module can marking and count the pins per side.** Then either tick assumption **A-2** and proceed, or re-derive §4.2 from that board's own pinout — derive it fresh, do not adapt the table pin-by-pin, because the failure mode of a half-adapted pin map is a node that mostly works.

Record the answer on the §5 part-identification table and close **I-38**.





