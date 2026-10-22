# Battery monitor rev-a: hand-assembly notes, firmware obligations, bring-up tests

**2026-10-26: U201 thermal-pad holes 0.20 -> 0.30 mm (JLCPCB-standard round, board `cf1b4365`, `../../evidence/jlc-standard-20261026/report.md`)**: the only board change; PCB finish HASL (JLCPCB default) instead of ENIG, stencil with JLCPCB's default thickness and apertures. Parts, lands and the soldering order are unchanged.

**2026-10-25: parts re-sourced to Ukrainian stock (IMRAD-stock round), see evidence/imrad-stock-20261025/report.md** (`../../evidence/imrad-stock-20261025/report.md`; layout `imrad-layout-CHANGES.md`, this edit `imrad-docs-CHANGES.md` in the same folder), board `98f3c7e2`. Parts that changed (schematic fields and BOM; land unchanged unless stated):

- F101: Littelfuse NANO2 0451002.MRL, 2 A 125 V very fast, new footprint `Fuse_Littelfuse-NANO2-451_453`: larger body (6.10 x 2.69 x 2.69 mm, pads 1.96 x 3.15 mm), no polarity, either orientation; 3D model `3dmodels/0451002.MRL_C99547_easyeda.step` (EasyEDA/LCSC C99547, 6.10 x 2.70 x 2.70 mm; KiCad 10.0.6 has none), board 0eb3bae2.
- R511: Yageo RC2512JK-074R7L, 4.7 R 2512 1 W thick film, new footprint `R_2512_6332Metric` at a new position (stage 5); a standard 1206 4.7 R is NOT acceptable.
- D101: Diodes B1100-13-F (value `B1100`), same SMA land. D501: Lite-On LTST-C191KGKT (yellow-green). D502: Kingbright KP-1608SURCK (red).
- J201: GCT USB4105-GF-A on the unchanged land of the former HRO part: test-fit it on a bare board before soldering (stage 3).
- L101: Bourns SRN6045-220M, same land. Resistors: Yageo / Royal Ohm 1 % (R304 Panasonic ERA-6AEB224V, R305 Yageo RT0603BRD0710KL unchanged); C103 / C104 group Samsung, C105–C107 / C201 Murata GRM21BR61E226ME44L, D102 STMicroelectronics SMAJ33A-TR. Values unchanged.
- References hidden on the silk since this round: R102, R103, J503, D101 (find them on the assembly drawing; J503 keeps its pin legends `+ GND SNS`).

**2026-10-23: owner silk edit r8** (`../../evidence/owner-silk-r8-20261023/`, this edit: `r8-reexport-CHANGES.md` there): top references J201, U201, D501, R202, U401, J503, J504 and the legend `BTN` (now left of J504) moved by the owner, legends `BOOT` / `EN` deleted (recovery points by part and pad, stage 3), board `f26090a2`.

**2026-10-23: silk rework r7** (`../../evidence/silk-r7-20261023/`, this edit: `r7-docs-CHANGES.md` there): new silk legends, 40 references on the silk, 26 only on the assembly drawing, bottom date line `2026-10-23  4L`.

**2026-10-22: corrections of review round r6, see evidence/review-r6-20261022/report.md** (`../../evidence/review-r6-20261022/report.md`; this edit: `r6-docs-CHANGES.md` in the same folder).

Earlier revision: 2026-10-20 balanced "Feng-Shui" 62 x 50 mm 4-layer rework (62 BOM parts, 0 DNP; `../../evidence/fengshui-20261020/RESULT-SUMMARY.md`).

Board: 4 layers, 62 x 50 mm, 66 footprints, all on the top side (62 BOM references: 59 soldered parts in the position file, plus J101 wire pads, TP102/TP103 test pads; + 4 mounting holes H1–H4). Source: installed `battery-monitor-rev-a.kicad_pcb` `cf1b4365` (JLCPCB-standard round, 2026-10-26: `98f3c7e2` of the IMRAD-stock round with the F101 3D model and the 12 U201 thermal-pad holes at 0.30 mm) (KiCad 10.0.6), the board of every file in this package (Gerbers, drill, IPC-D-356, assembly drawings, renders, STEP). Against the r8 board it changes only the F101 and R511 footprints with their tracks and the GND pour around them, one added GND via (76 vias) and the hidden D101, R102, R103 and J503 references (`../../evidence/imrad-stock-20261025/imrad-layout-CHANGES.md`). DRC with parity: 0 errors / 0 unconnected / 0 parity + 7 accepted `lib_footprint_mismatch` warnings (U201, J201, U401, L101, TP102, TP103, R511). **Never run "Update footprints from library" (or "Change footprints" / any library re-sync) on U201, J201, U401, L101, TP102, TP103 or R511: their board silk was edited against the library footprints (r7; R511 in the IMRAD-stock round: library silk line split around the `BAT -` legend; accepted DRC warnings `lib_footprint_mismatch`), and a library update restores the old silk with its silk-edge and silk-to-pad defects (L101 0.04 mm, TP rings 0.14 mm, U401 at C402 0.095 mm, the R511 line across `BAT -`).** Part values and MPNs: schematic PDF and BOM (`../02-components/battery-monitor-rev-a-bom.csv`; a few schematic texts and BOM notes are stale, see "Schematic errata" in `../README.md`); what to buy: `../02-components/battery-monitor-rev-a-components.xlsx` (sheet `Плата` for the board parts, sheet `Поза платою` for the voltmeter module, XH mating housings and contacts, nylon mounting hardware and the inline battery-lead fuse).

Silkscreen (r7, IMRAD-stock round 2026-10-25): every visible text is at least 1.0 mm / 0.15 mm and every silk line at least 0.15 mm. 36 references are on the silkscreen; 30 are only on F.Fab, i.e. in the assembly drawing: C103, C104, C106, C203, C205, C301, C401, D101, H1–H4, J101, J503, R101, R102, R103, R206, R304, R305, R307, R506–R509, R512, TP102, TP103, U101, U202 (D101, R102, R103 and J503 since 2026-10-25: the NANO2 F101 and the 2512 R511 took their places). J101, TP102, TP103 and J503 are identified by their legends instead (`BAT +` / `BAT -`, `+`, `GND`, `+ GND SNS`); the others have no room for an unambiguous 1.0 mm label. Functional legends: `BAT +` / `BAT -` beside the J101 pads (1.2 mm, top and bottom), J503 pin legends `+` / `GND` / `SNS` above pins 1–3, `BTN` left of J504, `GND` right of TP103, a graphic `+` above TP102, `STATUS` / `ALERT` left of D501 / D502, `rev A` at the top left (no `BOOT` / `EN` legend since r8: the recovery points are R202 pad 2 and C204 pads 1/2, stage 3; R202 and C204 carry their references on the silk); bottom: `battery-monitor rev A` (1.5 mm) and `2026-10-23  4L`. Page 1 of `assembly-top.pdf` shows all 66 references (F.Fab, H1–H4 next to their holes); the map pages 2–6 label all 62 BOM references, each on its own part (H1–H4 are not labelled there); `assembly-top.html` shows every part on hover. Coordinates below are board-relative (top-left corner = (0, 0), x to the right, y down); the KiCad files use the same axes with an offset of (100, 100) mm.

| File in this folder | Use |
|---|---|
| `assembly-top.pdf` | Page 1: F.Fab (pad outlines, all 66 references) + F.SilkS + Edge.Cuts; page 2: overview map 1.5:1, pages 3–6: quadrants Q1–Q4 at 3:1 (all 62 BOM references, each label on its own part) |
| `assembly-top.html` | Interactive map (hover a part for reference, value, MPN; pin-1 pads outlined red) |
| `assembly-bottom.pdf` | Bottom view, mirrored: no parts, only THT holes and the bottom legends (`BAT +` / `BAT -`, `battery-monitor rev A`, `2026-10-23  4L`) |
| `battery-monitor-rev-a-pos-kicad.csv` | Position file, top side, mm, KiCad page coordinates (board spans X 100..162, Y -100..-150) |
| `battery-monitor-rev-a-schematic.pdf` | Root + 5 sheets |
| `render-top.png`, `render-bottom.png`, `battery-monitor-rev-a.step` | 3D views (J101 has no model: two wire pads; F101 uses the EasyEDA/LCSC model of 0451002.MRL, added 2026-10-25) |

## 1. Before soldering

- Bare board check: VIN_PROT (C101 +) to GND, VBAT_FUSED (F101 pad 2) to GND and +3V3 (TP102, silk `+` above it) to GND (TP103, silk `GND` beside it) all open; no copper in the antenna strip along the top edge.
- The ESP32-S3-WROOM-1 is MSL 3 (168 h floor life at <= 30 C / 60 % RH after the bag is opened). Owner stock that has been out of its sealed bag longer, or with a pink humidity card, must be baked before hot air or hot plate reflow, per the Espressif module datasheet (storage section) / J-STD-033 (take the bake time and temperature from that document; NOT VERIFIED here). Bake on a tray, not in the reel tape.
- Stencil: top, JLCPCB's default thickness (usually 0.12 mm) and standard apertures from F_Paste (includes the U201 and U101 exposed-pad windows). PCB finish HASL (leaded): tin the pads flat with the iron before placing fine-pitch parts if the HASL is domed.
- Tools: hot air station, fine iron and flux for 0.5 mm pitch, 60–80 W iron with a large tip for J101, J503, J504 and the ground connections, magnifier.

## 2. Order of soldering (test after each stage)

| Stage | Parts | Notes |
|---|---|---|
| 1 Power | U101, then F101, D101, D102, C102, C103, C104, R101, R102, R103, L101, C105, C106, C107, C101 last | U101 see 2.1; U101 must be a PFM code: LMR38010SDDAR (schematic MPN) or LMR38010SQDDARQ1, which the workbook buys at VD MAIS / Digi-Key (schematic UA_Source: AEC-Q100 grade of the same part, drop-in; not checked here against the TI Q1 datasheet); FDDAR/FSDDAR are forced PWM (SNVSC73B section 5). F101 = Littelfuse NANO2 0451002.MRL (2 A 125 V): larger than the old 1206 (body 6.10 x 2.69 mm, pads 1.96 x 3.15 mm), no polarity, either orientation; H3 is next to its pad 2 (section 3). D101 = B1100 (Diodes B1100-13-F). D101/D102: cathode band = pad 1. L101 = Bourns SRN6045-220M (Isat 1.9 A; not the Sumida CDRH6D38NP-220NC, 1.3 A). C101 electrolytic: + pad on VIN_PROT (silk +). **Test:** bench supply 12 V, current limit 100 mA, into J101: +3V3 at TP102 = 3.22–3.45 V, typ 3.32 V (FB 510k/220k with VREF 1.0 V = 3.318 V nominal; band = PFM regulation -1.5 %/+2.5 % (SNVSC73B) with 1 % feedback resistors), input current a few mA |
| 2 MCU | U201, then C202, C203, C204, C201, C205, U202, R201, R202 | U201 with stencil paste and hot air (or hot plate before C101 is fitted); its exposed pad has 12 x 0.30 mm via-in-pad holes, unfilled: keep the stencil paste, do not add extra. C202, C203 and R201 sit 2.8 mm from the module pads (C204 11.5 mm, C201 6.6 mm); the order is free. Note: SW201, SW202, R207 were removed in the rework |
| 3 USB | J201, U203, R205, R206, R208, R209 | J201 = GCT USB4105-GF-A on the unchanged land of the former HRO part (footprint `USB_C_Receptacle_HRO_TYPE-C-31-M-12`): **test-fit it on a bare board before soldering** (legs in the slots, pads on their lands; J201 BOM note: signal pads identical, VBUS/GND 0.05 mm shift, shell legs 0.80/1.10 mm in the 1.2/1.7 mm slots; not tried on a real board yet). J201 shell tabs: fill the plated slots. The four shell slots are tied solid (no thermal relief) to the GND pours on all four layers: preheat the board (hot plate or hot air from below) and use the 60–80 W iron, otherwise the joint stays cold. **Test:** USB enumeration, flash a test firmware. Native ESP32-S3 USB-Serial/JTAG auto-reset and auto-download flashes over USB-C without BOOT/RESET buttons. Recovery without buttons: **download mode** – hold R202 pad 2 (BOOT net = GPIO0, the pad nearer the right edge, board (48.8, 15.0); no silk legend, identify it by the R202 reference and the coordinates; pad 1 next to it is +3V3, do not bridge the two) to GND (TP103, or C204 pad 2) while power is applied or EN is released; R202 is 10 k, so the short is harmless. **Reset** – short C204 across (pad 1 EN at board (9.5, 11.3); pad 2 GND at (9.5, 9.7); no silk legend, identify it by the C204 reference and the coordinates) with tweezers for about 0.1 s. Keep the 12 V bench supply on J101: USB VBUS does not power the board (design decision, concept report section 3) |
| 4 Sense | U301, C301, C302, R304, R305, R307, R504, R505 | U301 see 2.2. R304 = 220k 0.1 % 0805, R305 = 10k 0.1 % 0603: do not substitute 1 % parts. Note: R309, R310 were removed in the rework. **Test:** I2C scan finds 0x48 |
| 5 Gauge | Q501, Q503, R506, R507, R508, R509, F501, R511, R512 | F501 = 0466.250NRHF (0.25 A). R511 = Yageo RC2512JK-074R7L, 4.7 R 2512 1 W thick film, at its new position since 2026-10-25: below R102/R103, between the `BAT -` legend (left) and J503 (right), board (26.1, 45.5), long axis vertical, pad 1 up; its reference is printed left of the lower pad. A standard 1206 4.7 R is still NOT acceptable: R511 must be this 2512 1 W part (short-time overload 2.5 x rated voltage for 5 s covers the 1.8 W / 5 s gauge-fault case; BOM note) |
| 6 IO | D501 (yellow-green Lite-On LTST-C191KGKT), D502 (red Kingbright KP-1608SURCK), R502, R503, R501, R510 | LED pin 1 (cathode) to GND (the left pad, next to the silk bracket; legends `STATUS` = D501, `ALERT` = D502 to the left); the two makers mark the cathode differently: take it from the bought part's datasheet or a diode tester before placing (NOT VERIFIED here). Note: SW501, R211 were removed in the rework |
| 7 LoRa | C401, C402, R401, then U401 Ra-02 | Before soldering, check the Ra-02 underside for exposed copper over the F.Cu tracks (photo into evidence). Castellations by iron; the pad 1 column is 0.5 mm from the right edge. The left column is crowded: L101 is 3.55 mm from the U401 pads and C105–C107 stand between them, 1.1–1.5 mm from the U401 pads: C105–C107 are soldered in stage 1, so reach the left castellations with a fine tip from above at an angle and do not reflow the C105–C107 joints. C402 (100 nF) now sits below U401 pad 1 at the right edge. Never transmit without the 433 MHz antenna on the module U.FL |
| 8 Connectors | J503 (XH 3P voltmeter), J504 (XH 2P external wake button) | Pin 1 = the square (rounded-rectangle) pad, with the library pin-1 corner mark on the silk; J503 has the pin legends `+` / `GND` / `SNS` above pins 1–3, J504 the function legend `BTN` to its left (section 4 has the pinout). XH latch side as the silk outline. Note: J202, J501, J502 were removed in the rework |
| 9 Battery leads | J101 | Section 3 |

### 2.1 U101 LMR38010 (HSOIC-8 PowerPAD)

- The 5 exposed-pad GND vias (0.6/0.3 mm, cross pattern, board (29.1–30.9, 34.6–36.4)) are **open on the top** inside the pad's solder-mask opening and closed (tented) on the bottom. The stencil paste windows avoid them (no paste over any of the 5 drills), so little solder runs into the barrels, but expect some; the pad cannot be soldered from the bottom through them. Solder the pad with the stencil paste and hot air from the top; do not add extra solder to the pad.
- Paste coverage on the pad is about 36 % (R5-10): enough for the thermal joint; do not flood the pad (the package floats and the pins lift). Align pin 1 with the silk mark.
- Heat until the paste on all 8 pins has flowed and the part self-centres; touch up the pins with the iron if needed.
- Verification: the stage-1 power test; full-load case temperature is bring-up test B10.

### 2.2 U301 ADS1115 (VSSOP-10, 0.5 mm pitch)

Tack two opposite corner pins, check both rows (pin 1 = ADDR, marked), drag-solder with flux, clean, check for bridges under magnification (pins 9/10 SDA/SCL and 4–7 AIN0–AIN3 matter most). ADDR is tied to GND: address 0x48.

## 3. Battery leads (J101, XT30 pigtail) and mounting

- J101 is two plated holes (1.6 mm drill) for 0.75 mm2 wire (the XT30 pigtail leads); the reference J101 is not printed (r7), the `BAT +` / `BAT -` legends identify it. **Pad 1, the square (rounded rectangle) pad with the legend `BAT +` to its left, is plus**; pad 2, round, with `BAT -` to its right, is minus. The same legends (1.2 mm) are on the bottom side at the same places, so seen from the bottom each legend is again beside its own pad.
- Strip about 3 mm, tin, insert from the top, solder from the bottom with the 60–80 W iron, fillet on both sides. Strain-relieve the leads (cable tie or hot glue); the pads are not a strain relief.
- Check the lead polarity against the XT30 shell markings with an ohmmeter before the first connection. A reversed battery is blocked by D101, but the divider and the voltmeter sense line still see the reversed voltage through their resistors (B4).
- **Inline fuse in the battery + lead (owner wiring, required).** Fit an inline blade fuse holder in the + lead within 0.2 m of the + clip: MINI (ATM) or ATO blade, 2 A, 32 V DC rated (24 V systems equalise at up to 31 V; check the chosen fuse's DC rating and interrupting rating on its datasheet). F101 on the board is secondary protection only: the NANO2 0451002.MRL is rated to interrupt 50 A at 125 V AC/DC and 300 A at 32 V DC (Littelfuse 451/453 datasheet, interrupting ratings 1/16–8 A, https://kosmodrom.ua/storage/pdf/FUSE0451.pdf), and a 12/24 V battery can deliver more than that into a shorted input part or a shorted lead. For the documented 48 V part swap use a fuse rated at least 80 V DC. Purchase lines: workbook `../02-components/battery-monitor-rev-a-components.xlsx`, sheet `Поза платою`, П8 (inline MINI blade fuse holder with lead) and П9 (MINI blade fuse 2 A, 32 V DC). Check the marking of the bought fuse for the 32 V DC rating and that the holder wire is not thinner than the battery lead (0.5–0.75 mm2); neither is stated on the shop pages.
- Surge assumption (recorded, no action): the input clamp D102 SMAJ33A is rated for 10/1000 us pulses (53.3 V clamp at 7.5 A); higher-current 8/20 us events reach 69.7 V, inside U101 (85 V absolute maximum) and the Q501 avalanche rating. A low-impedance battery behind clip leads is not expected to deliver such pulses.
- Mounting: 4 x NPTH 3.2 mm for M3 (H1–H4) at (2.5, 2.5), (59.5, 2.5), (2.5, 47.5), (59.5, 47.5) mm; laminate web to the edge 0.9 mm, do not overtighten. Washer OD <= 7 mm (DIN 125 M3); a 5.5 mm head or a 7 mm washer overhangs the outline by 0.25–1.0 mm, which is harmless. GND copper (under solder mask) reaches 1.85 mm from each hole centre. **At H3** (bottom left) the nearest battery-potential copper is F101 pad 2 (VBAT_FUSED), 3.78 mm from the hole centre since the NANO2 fuse (2026-10-25; was 4.45 mm), 0.28 mm outside a 7 mm DIN 125 washer: use a nylon washer there and never a DIN 9021 washer. Use nylon or plastic hardware at all four holes, also at H1 and H2 next to the WROOM antenna (nearest non-GND copper: H1 7.7 mm, H2 8.4 mm, H4 7.3 mm). The workbook sheet `Поза платою` buys this set: nylon standoffs M3 F-F 10 mm, plastic screws M3x6 (head <= 5.5 mm), plastic washers M3 OD 6 mm.
- Enclosure heights (from the 3D model): tallest part C101 (electrolytic, case 8 x 10.2 mm; its 3D model is 10.0 mm), then J503/J504 7.0 mm plus the mated XH plug, L101 4.2 mm, J201 3.25 mm, U401 3.2 mm, U201 3.1 mm (L101 and J201 from the models of the former parts on the same lands: SRN6045-220M and USB4105-GF-A heights NOT VERIFIED here; F101 has no model, NANO2 body 2.69 mm per the Littelfuse 451 drawing). Keep >= 11.0 mm free above the top surface and use standoffs >= 2.5 mm below (XH pin tips 1.8 mm, J101 wire joints). The USB-C receptacle front face is 0.45 mm beyond the left board edge by design: plan the wall opening there.

## 4. External connectors wiring (J503 and J504)

### 4.1 Voltmeter module wiring (J503, JST XH 3P)

The silk carries the pin legends `+` (pin 1), `GND` (pin 2) and `SNS` (pin 3) above the housing (r7); pin 1 is the square (rounded-rectangle) pad, on the left when the board is viewed from the top with the connectors at the bottom.

| J503 pin | Pad | Net | Module wire (3-wire 0.28 inch module) |
|---|---|---|---|
| 1 (`+`) | square | V_GAUGE: switched by Q501 (GPIO38 GAUGE_EN), through F501 0.25 A and R511 4.7 R | red: module supply |
| 2 (`GND`) | oval, middle | GND | black |
| 3 (`SNS`) | oval | GAUGE_SENSE: from VBAT_FUSED through R512 10 k, live whenever the battery is connected | yellow (or white/blue): measure input |

- Check the wire colours against the label of the purchased module; the module supply range is 4–30 V (ANSWERS Q5).
- R512 10 k in series with the sense input makes the module read 1–9 % low (sense input 100 k to 1 M): set the module trimmer once against a DMM at J101. If the module's sense input is 100 k or lower, fit R512 = 2.2 k 1206 (R3-06, B7).
- A 2-wire module: red to pin 1, black to pin 2, pin 3 empty. It reads its own supply, which is the battery minus the D101 and R511/F501 drops (about 0.6–0.8 V low).

### 4.2 External wake button wiring (J504, JST XH 2P)

| J504 pin | Pad | Net | Connection |
|---|---|---|---|
| 1 | square (left) | BTN_PULL (`/05-io/BTN_PULL`) | +3V3 via R510 (1 kOhm series current limiter, max short current 3.3 mA) |
| 2 | oval (right) | BTN_USER (`/BTN_USER`) | GPIO5 (pulled down to GND by R501 10 kOhm; active high when button closed) |

- Connect any external momentary normally-open (NO) pushbutton across pins 1 and 2 of J504 (silk legend `BTN`; no pin legend, the button has no polarity).
- Closing the external button pulls GPIO5 HIGH to +3.3 V through R510 (1 kOhm), generating a wake interrupt or user trigger. Releasing the button allows R501 (10 kOhm) to pull GPIO5 cleanly to GND.
- Because Pin 1 is current-limited through R510 (1 kOhm), an accidental short of external button wiring to GND or chassis draws at most 3.3 mA and cannot damage the +3V3 power rail or the MCU.
- J504 pin 2 goes to GPIO5 without a series resistor or ESD part: keep the button lead short (< 0.5 m) and inside the enclosure, twist the two wires, and use a button with a plastic actuator (rev-b note: 1–4.7 k in series at U201 pin 5).

## 5. Firmware obligations

From the concept report section 1 and the review dispositions r1–r5 and Step 6 review, checked against the installed netlist:

| # | Obligation |
|---|---|
| F1 | Target ESP32-S3-WROOM-1 N16R8 (16 MB flash). **PSRAM is not initialised** (W06): build without SPIRAM support; GPIO35–37 are not connected. ADC2 is unusable with Wi-Fi. PlatformIO (platform espressif32@6.12.0, Arduino core 2.0.17): `board = esp32-s3-devkitc-1`, `board_upload.flash_size = 16MB`, `board_upload.maximum_size = 16777216`, `board_build.partitions = default_16MB.csv`, `board_build.flash_mode = qio`, `build_flags = -DARDUINO_USB_CDC_ON_BOOT=1` (Serial on the USB-C port; without it Serial is UART0 = GPIO43/44); the board file alone defaults to 8 MB (`default_8MB.csv`). No `-DBOARD_HAS_PSRAM`, keep the default `board_build.arduino.memory_type`. Keep the -ci env as `extends`; its own `build_flags` replaces the inherited one, so write it as `${env:<base>.build_flags} -DBATTERY_MONITOR_CI`. Staying on 6.12.0 keeps the IDF 4.4 `esp_task_wdt_init(60, true)` call (`main.cpp:45`) valid. NOT VERIFIED by a build |
| F2 | Pin map: I2C SDA GPIO8, SCL GPIO9; ADS1115 at 0x48; external wake button on J504 to GPIO5 (active high, 10 k pull-down via R501, 1 k series limit via R510, pressed = HIGH; long press 10 s = factory reset); LED_STATUS GPIO40 (yellow-green D501) and LED_ALERT GPIO41 (red D502), active high; GAUGE_EN GPIO38 (active high); LoRa FSPI NSS 10, MOSI 11, SCK 12, MISO 13, RST 15, DIO0 14, DIO1 21. GPIO1, GPIO4, GPIO7, GPIO2, GPIO6 and the ADS1115 ALERT/RDY pin are unconnected (no sense-enable pin; poll the conversion-ready bit instead of an RDY interrupt; J202, J501, J502 removed in rework) |
| F3 | Remaps from the esp32dev firmware: `PIN_VOLTAGE 32` -> ADS1115 AIN0; `PIN_BUTTON 34` -> 5; `PIN_VOLTMETER 4` -> 38 (gauge only); `RESISTOR_R1/R2 30000/7500` -> 220k/10k (ratio 23.0); `V_REF`, `ADC_MAX`, `CORR_FACTOR` replaced by the ADS1115 and a per-unit one-point calibration stored in NVS; the 5–15 V critical range becomes per-nominal ranges (6/12/24 V bins detected from the reading; 48 V only with the documented part swap) |
| F4 | Battery voltage: AIN0 single-ended, FSR +/-2.048 V (31 V gives 1.348 V at the tap; LSB 62.5 uV = 1.44 mV at the battery); report finer than 0.1 V. AIN1–AIN3 on U301 are unconnected (AIN0 measures battery voltage). Config word per conversion: OS = 1, MUX = 100b (AIN0-GND; reset value 000b is AIN0-AIN1), PGA = 010b (+/-2.048 V), MODE = 1 (single-shot); poll OS until 1, then read register 0x00 (SBAS444E Table 8-3) |
| F5 | **The divider is always on**: about 0.1 mA from the battery (V/230 k: 0.05 mA at 12 V, 0.10 mA at 24 V, 0.13 mA at 31 V); there is no switch to open it. Include it in the standby budget |
| F6 | **GAUGE_EN lockout above 30.0 V**: read the ADS1115 first and never assert GAUGE_EN while the battery is above 30.0 V (module supply limit 30 V; 24 V equalisation reaches 31 V). Keep the on-window bounded (existing `VOLT_ON_TIME_MS` 20 s). `voltageRead()` must not touch GAUGE_EN any more (`voltage.cpp:50-54,63` power the gauge around every sample because the prototype pin also fed the divider; delete those lines and `VOLTMETER_SETTLE_MS`). Apply the lockout in `voltageDisplayFor()` (fresh ADS1115 reading first) and in `voltageTick()` on every new reading while the window is open; use about 0.3 V hysteresis (re-enable below 29.7 V); signal a refused press on LED_ALERT GPIO41. Proven by B16 |
| F7 | **I2C at 400 kHz** with on-board ADS1115 (4.7 k pull-ups R504/R505 on board; J501 removed in rework). Run an I2C bus recovery at init (GPIO8/9 glitch low at reset) |
| F8 | Ra-02: put the radio to sleep when unused (1.6 mA standby otherwise); avoid simultaneous Wi-Fi TX and LoRa TX where possible (+3V3 peak budget about 500 mA against the 1 A regulator) |
| F9 | Firmware may stay always-on (W07); the hardware is deep-sleep ready (U101 PFM, 40 uA Iq) |
| F10 | GPIO40 may light the yellow-green LED in USB download mode (ROM drives it): harmless |
| F11 | LoRa reset: keep GPIO15 as input (high-Z); for a manual reset drive it low for >= 100 us, return it to input (released, SX1276/77/78/79 datasheet 7.2.2), wait 5 ms; never drive it high. This row supersedes any older "push-pull" wording of the 04-lora sheet note (ext BM-F09) |

## 6. Bring-up tests

Sources: r1 reviewer report section 5, r2/r3 additions, r4 section 7, r5 section 6, doc-audit 2026-10-16. Run them on the first assembled board and keep the results (scope captures, readings) under `../../evidence/`.

| # | Test | Pass | Source |
|---|---|---|---|
| B1 | +3V3 at U201 pad 2 during simultaneous Wi-Fi 802.11b TX and LoRa TX (scope AC, 20 MHz BW), at 6 V and 31 V input | never below 3.15 V at U201 pad 2 (TPS3808G33 VIT max 3.116 V, SBVS050N 6.5); target dip <= 100 mV | r1-1, r5-1, r6 S-03 |
| B2 | Standby current from 12.0 V and 24.0 V, firmware in deep sleep, GAUGE_EN low, Ra-02 asleep | about 50 uA typical (70 uA worst) **plus the always-on divider** (52 uA at 12 V, 104 uA at 24 V) | r1-2, adjusted for F5 |
| B3 | 20 hot-plug connections with alligator clips to a 29–29.5 V low-impedance source | F101 intact; VIN_PROT overshoot at D102 below 55 V (r1) / 60 V (r4) | r1-3, r4 |
| B4 | Reversed clips (-29.5 V) for 60 s | board unpowered, voltmeter dark, no damage; reverse current small (divider about 0.13 mA plus the R512 path if a module is connected; r1 limit 0.25 mA). Known and accepted: ADS1115 AIN0 then sits about 0.3–0.6 V below GND, beyond the -0.3 V absolute-maximum input voltage (SBAS444E 5.1), with 65–93 uA input current at -29.5 V (71–99 uA at -31 V), far inside the 10 mA limit | r1-4, ext OWN-06 |
| B5 | ADS1115 reading against a 5.5-digit DMM at 6, 12, 24, 29.5/31 V after one-point calibration at 24.00 V | error <= 0.5 % of reading | r1-6, r5-5 |
| B6 | Assert GAUGE_EN 20 times at 29.0 V into the connected voltmeter module | F501 and R511 intact, correct display | r2-7 |
| B7 | Sense input impedance of the purchased voltmeter module | decides R512 10 k or 2.2 k (section 4) | r3 R3-06 |
| B8 | LoRa RSSI noise floor at 433 MHz with no transmitter: (a) 12 V through the buck under Wi-Fi load, (b) buck idle or bench 3.3 V | difference <= 3 dB | r4 |
| B9 | ADS1115 AIN0 with battery input, 128 SPS, buck loaded (AIN1/AIN3 unconnected) | noise <= 1 LSB rms at FSR +/-2.048 V | r4, rework |
| B10 | U101 input ripple at the VIN pins (short ground spring) at 31 V and 0.5 A; U101 case temperature at 31 V and maximum load | ripple < 0.5 V p-p; case < 70 C at 25 C ambient | r4, r5-6 |
| B11 | LMR38010 SW node and output ripple at 0.05 A and 0.4 A; load step | PFM stable, no audible noise, ripple < 50 mV; load step stable (3 x 22 uF after DC bias) | r5-2, r4 NOT VERIFIED |
| B12 | 20 Wi-Fi TX bursts at full power plus 10 presses of external wake button on J504 | no spurious reset | r4, rework |
| B13 | J503 after soldering, board unpowered (D17): J503 pin 1 (V_GAUGE) and pin 3 (GAUGE_SENSE) to pin 2 (GND): no short; Q501 pin 1 (GAUGE_GATE) to GND above 1 MOhm | above 1 MOhm, no short | r4, doc-audit D17 |
| B14 | On-board I2C between U201 and U301 during simultaneous Wi-Fi and LoRa TX | error-free | r5-3, F7 |
| B15 | USB enumeration and flashing at full speed over a 1 m cable; keep the 12 V bench supply on J101: USB VBUS does not power the board (design decision, concept report section 3) | passes | r5-4, ext BM-F07 |
| B16 | GAUGE_EN lockout: bench supply on J101 at 29.5 V, close external button on J504; then 30.5 V, close external button: 29.5 V: module on; 30.5 V: GAUGE_EN stays low (J503.1 below 1 V), alert LED D502 signals the refused press | F6, ext BM-F03 |
| B17 | Gauge short at J503 pin 1-2 with a 5.25 V bench supply on J101 (current limit 2 A): F501 opens within 5 s, R511 intact (resistance 4.7 R +/-5 % after cooling) | F501 opens within 5 s, R511 intact | doc-audit D18, simplify-20261015 |

The r1–r3 tests of the high-side sense switch (Q301 drop and settling) no longer apply: the switch was removed in the simplification step and the divider is always connected (F5).
