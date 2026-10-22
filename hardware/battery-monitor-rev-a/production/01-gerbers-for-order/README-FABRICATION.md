# Battery monitor rev-a: bare PCB and stencil order (JLCPCB standard, no surcharge)

Upload `battery-monitor-rev-a-gerbers.zip` to JLCPCB (Standard PCB/PCBA). The same files are unpacked in `gerbers/`.
No factory assembly: the boards are hand-assembled (JLCPCB offers no PCB assembly for delivery to Ukraine; do not tick
"PCB Assembly" anywhere).

**Rule for this order (owner, 2026-10-26): the board is a JLCPCB standard 4-layer board and is ordered with the quote
page defaults. Change only what the table "JLCPCB quote page" names; every other option stays at its default. Any option
that adds a line to "Charge Details" (other than the PCB price itself, the stencil and the build time at $0) is a
mistake: undo it.**

Source: installed board `cf1b4365` (2026-10-26, JLCPCB-standard round, `../../evidence/jlc-standard-20261026/report.md`).
Against the previous board `0eb3bae2` only the 12 thermal-pad holes of U201 (ESP32-S3-WROOM-1 pad 41) changed from
0.20 to 0.30 mm (pad copper 0.6 mm unchanged): 0.20 mm holes put the order into JLCPCB's "Min via hole size 0.2 mm"
tier, which on 2026-10-26 added $16.93 for the drill, a forced 4-wire Kelvin test ($16.75) and FR4 TG155 instead of TG135
($3.39) to a $8 board. All 10 Gerber layers and the NPTH drill file are identical to the `0eb3bae2` package apart from
date lines; only the PTH drill file, the drill report and map, IPC-D-356, STEP and the documentation captions changed.
KiCad 10.0.6 `kicad-cli`; DRC with schematic parity: 0 errors, 0 unconnected, 0 parity; 7 accepted
`lib_footprint_mismatch` warnings (J201, U201, U401, L101, TP102, TP103, R511: board silk or holes edited against the
library footprints; never run "Update footprints from library" on them).

## Files

| Folder / file | Content |
|---|---|
| `battery-monitor-rev-a-gerbers.zip` | 10 Gerber layers + job file + 2 Excellon drill files (13 files, flat, same as `gerbers/`) |
| `gerbers/` | RS-274X (X2 attributes written as `G04 #@!` comments, `kicad-cli --no-x2`), Protel extensions, mm, format 4.6, soldermask subtracted from silkscreen: F_Cu `.gtl`, In1_Cu `.g1`, In2_Cu `.g2`, B_Cu `.gbl`, F/B_Mask `.gts/.gbs`, F/B_Silkscreen `.gto/.gbo`, F_Paste `.gtp` (stencil only), Edge_Cuts `.gm1`, `-job.gbrjob`; drills `-PTH.drl`, `-NPTH.drl` (Excellon, metric, decimal, absolute origin, PTH and NPTH separate, plated slots as routed paths G00/M15/G01/M16) |
| `drill/` | Drill maps (PDF, PTH and NPTH) and drill report |
| `netlist-ipc356/` | IPC-D-356 netlist for the electrical test (`kicad-cli pcb export ipcd356`) |
| `alt-drill-G85/` | Only if the fab asks for it: the same PTH drill file with the 4 J201 slots written as G85 oval holes (`--excellon-oval-format alternate`) and a ZIP `battery-monitor-rev-a-gerbers-G85-slots.zip` that differs from the main ZIP only in that file. Use one ZIP, never both |

Layer order, top to bottom: F.Cu (signals), In1.Cu (continuous GND plane), In2.Cu (continuous GND plane), B.Cu (signals,
GND pour). All files share one coordinate origin. All components are on the top side; there is no bottom paste layer.
Do not rename any file: the job file names them. The job file carries KiCad's default stack (finish None, 0.48 mm
dielectrics, 1 oz inner); JLCPCB uses the quote-page settings, not the job file.

## JLCPCB quote page (checked 2026-10-26)

| Field | Set to | Default? | Note |
|---|---|---|---|
| Base Material / Layers / Dimensions | FR-4 / 4 / 62 x 50 mm | detected from the Gerbers | the page reads "Detected 4 layer board of 50x62mm" |
| PCB Qty | 5 | default | |
| Product Type | Industrial/Consumer electronics | default | |
| Different Design / Delivery Format | 1 / Single PCB | default | |
| PCB Thickness | 1.6 mm | default | |
| PCB Color / Silkscreen | Green / White | default | |
| Material Type | FR4 TG135 | default | TG155 appears only when a small-hole tier is chosen |
| Surface Finish | HASL (with lead) | default, $0 | ENIG and lead-free HASL cost extra (ENIG +$17.00 for 5 boards on 2026-10-26); leaded HASL is fine for hand soldering of the 0.5 mm pitch parts (U301, J201) |
| Outer / Inner Copper Weight | 1 oz / 0.5 oz | default | |
| Specify Layer Sequence / Specify Stackup | No / No | default | JLCPCB's standard 1.6 mm 4-layer stack is JLC04161H-7628 (prepreg 0.21 mm); no controlled impedance in this design, so leave "No" |
| Via Covering | Plugged | default for 4 layers, $0 | "Tented" is not offered for 4 layers; plugged vias are closed on both sides, except the 5 U101 exposed-pad vias, which stay open on the top inside the pad's mask opening by design |
| Via Plating Method | Not Specified | default | |
| Min via hole size/diameter | 0.3mm/(0.4/0.45mm) | default, $0 | smallest hole in the drill files is 0.30 mm; any smaller option adds a surcharge, a Kelvin test and TG155 |
| Board Outline Tolerance | +-0.2 mm (Regular) | default | |
| Confirm Production file | No | default | |
| Mark on PCB | Remove Mark | default, $0 | |
| Electrical Test | Flying Probe Fully Test | default, $0 | |
| Gold Fingers, Castellated Holes, Press-Fit, Edge Plating, Blind Slots, UL Marking, Backdrill, Humidity card | No | default | none of these features exist in the design |
| PCB Assembly | off | | not offered for Ukraine |
| Stencil | Order together with PCB: on; Framework No; Stencil Side Top only; all other stencil fields default (thickness and apertures "Select by JLCPCB" / "JLCPCB's Standard") | | about $3; JLCPCB's standard apertures are fine for hand paste printing |
| Build time | PCB 3-4 days, stencil 1-2 days | default, $0 | |
| Shipping | choose the cheapest offered method at checkout | | the estimate on the quote page can switch to DHL Express by itself (seen 2026-10-26: $91.33 instead of $12.02) |

Expected "Charge Details" for 5 boards with the stencil: only the PCB price (shown as "Special Offer", $8.00 on
2026-10-26), "Via Covering $0.00", "Surface Finish $0.00" and "Stencil" (about $3.10). A line for "Min via hole
size/diameter", "4-Wire Kelvin Test", "Material Type" or a non-zero "Surface Finish" means a field was changed: undo it.

## Board facts (from the drill and Gerber files of `cf1b4365`)

| Item | Value | JLCPCB standard limit |
|---|---|---|
| Board | 62 x 50 mm, single board, routed outline (Edge_Cuts), no V-cut, no panel | |
| Layers / thickness / copper | 4, 1.6 mm, 1 oz outer, 0.5 oz inner | standard |
| Plated holes | 99: 84 x 0.30 (72 vias + 12 U201 thermal-pad holes), 4 x 0.40 (vias), 4 x 0.60 slots (J201: 0.6 x 1.2 mm x 2, 0.6 x 1.7 mm x 2), 3 x 0.95, 2 x 1.00, 2 x 1.60 | smallest plated hole >= 0.30 mm = no-surcharge tier; plated slot >= 0.5 mm wide, length >= 2 x width |
| NPTH | 6: 2 x 0.65 (J201 pegs), 4 x 3.20 (M3 holes H1-H4, 2.5 mm from the edges) | >= 0.50 mm |
| Vias | 76: 72 x 0.6/0.3 mm, 4 x 0.8/0.4 mm; ring >= 0.15 mm | 0.3/(0.4-0.45) tier |
| Component PTH ring | >= 0.15 mm (U201 thermal holes 0.6/0.3), J201 slot pads 0.20 mm, all others >= 0.35 mm | >= 0.15 mm on 4 layers |
| Hole to hole (edge to edge) | >= 0.50 mm (smallest J201 peg to slot 0.805 mm) | via-to-via 0.20, pad-to-hole 0.45 |
| Track / clearance | >= 0.20 mm / >= 0.15 mm (0.3 to 0.6 mm on the battery-voltage nets) | 0.09 / 0.09 mm |
| Copper to routed edge | >= 0.5 mm | 0.2 mm |
| Solder mask | smallest web 0.10 mm at U301 and J201 (pad spacing 0.20 mm) | 0.10 mm green, 1 oz |
| Silkscreen | text >= 1.0 mm / 0.15 mm (BAT + / BAT - 1.2 / 0.18 mm, bottom name 1.5 / 0.20 mm), every line >= 0.15 mm, >= 0.15 mm from pads (smallest 0.175 mm), about 0.30 mm from the edge | 1.0 / 0.15 mm |
| Holes in SMD pads | 5 GND vias inside the U101 exposed pad (open on the top through the pad's mask opening) and the 12 U201 thermal-pad holes; no paste over the U101 vias | standard; no filling wanted |
| Not used | gold fingers, castellated holes, edge plating, press-fit, blind/buried vias, impedance control | |
| Special | U201 antenna edge flush with the top board edge; ANT_KEEPOUT copper-free on all 4 layers at board x 16.0-46.0, y 0.0-6.2 mm (X 116.0-146.0, Y 100.0-106.2 in KiCad): add nothing there. Only the USB-C connector J201 overhangs the left edge by design | |

## Engineering questions (EQ) JLCPCB may send, and the answers

| Question | Answer (keeps the price) |
|---|---|
| Vias / holes in SMD pads (U101 exposed pad, U201 thermal pad): fill and cap them? | No. Leave them as drilled and plated; no epoxy fill, no capping (that is a surcharge option). |
| Plated slots at J201 read as round holes or slots? | They are 4 plated oval slots (0.6 x 1.2 mm x 2, 0.6 x 1.7 mm x 2) written as routed paths. If the engineer cannot read them, re-upload `alt-drill-G85/battery-monitor-rev-a-gerbers-G85-slots.zip`. |
| Solder mask dam between fine-pitch pads (U301, J201) below your minimum? | Remove the dam where it cannot be made; do not change copper. |
| Remove the order number? | Yes (Mark on PCB: Remove Mark). |

Check the Gerber viewer before paying: no copper in the antenna strip at the top edge (Top, Inner1, Inner2), In1 and
In2 solid GND, 4 oval slots at J201 (not round holes), 12 holes in the U201 thermal pad, silkscreen readable.

## Other fabs

The design stays inside every standard 4-layer process (smallest plated hole 0.30 mm, smallest track/clearance
0.20/0.15 mm). At PCBWay confirm the 0.20 mm ring of the J201 slot pads and the 0.15 mm ring of the U201 thermal
holes; inner copper starts at 1 oz there (fine).
