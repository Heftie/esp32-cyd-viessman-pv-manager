# Viessmann/SAIA ALE3 Meter Emulation ↔ Fronius SunSpec Source

Your ESP32 emulates a **SAIA-Burgess ALE3** three-phase energy meter (Modbus RTU) so a
Viessmann Vitotronic can read PV surplus data from it. Source: Viessmann's own
"Modbus-Stromzaehler-dreiphasig.pdf" datasheet for this exact meter — confirms the
device-type string `ALE3D5FD10C2A00` and the full register table (52 registers, 1-based
addressing; "register R is transmitted as R-1", big-endian word order on all
double-registers: high word first).

## Your pasted registers

| Constant | ALE3 reg # | Meaning per datasheet | Source it needs |
|---|---|---|---|
| `REG_TYP_ASN_FN_1..8` | 7–14 | "Typ / ASN-Funktion" — fixed device-type identity string, spells `ALE3D5FD10C2A00` | **Static — not from SunSpec.** This exact byte sequence must stay unchanged; it's how the Vitotronic recognizes the emulated device as a genuine ALE3. Your example values are already correct. |
| `REG_HW_VERSION` | 15 | Hardware version/modification (e.g. 11 = HW 1.1) | **Static** — arbitrary, not measured. Keep whatever value the Vitotronic already accepted. |
| `REG_SN_HIGH` / `REG_SN_LOW` | 16–17 | 32-bit unique serial number, high word first | **Static/arbitrary.** Not derived from Fronius. If you want it stable per-device, derive once from the ESP32's own chip ID/MAC rather than hardcoding, but it has no SunSpec counterpart. |
| `REG_STATUS` | 22 | 0 = no problem, 1 = problem with last comm request | **Derived from your own poll health**, not a single SunSpec register: set 0 while your last Modbus TCP read of the Fronius meter succeeded and is fresh; set 1 if the poll timed out / failed, so the Vitotronic doesn't act on stale data. |
| `REG_ERROR_REG` | 25 | 0 = no error; 1/2/4 = fault on phase 1/2/3; 3/5/6/7 = combinations | **Derived, not 1:1.** Leave 0 in normal operation. Only worth setting if you detect a specific phase is unreadable/invalid on the Fronius meter (e.g. `PhVphA` stuck at 0 while the others are live) — map that phase to its bit. |
| `REG_TARIFF` | 27 | 0 = Tariff 1, 4 = Tariff 2 | **Static: always 0 (Tariff 1).** The Fronius meter has no tariff concept — this field just needs to be present/valid. |
| `REG_TARIF1_ENERGY_TOTAL_HIGH/LOW` | 28–29 | Cumulative energy, tariff 1, scale 10⁻² kWh (i.e. register value = kWh × 100), high word first | **Fronius meter `TotWhImp`** (SunSpec Meter Model, float32, Wh — see `docs/fronius-meter-registers.md`). Convert: `reg32 = round(TotWhImp_Wh / 10)` (Wh→kWh is ÷1000, then ×100 for the scale factor = ÷10 overall). Then `HIGH = reg32 >> 16`, `LOW = reg32 & 0xFFFF`. |
| `REG_TARIF1_ENERGY_PARTIAL_HIGH/LOW` | 30–31 | Same scale, resettable counter (write 0/0 to reset) | Same source (`TotWhImp`) — either mirror the total, or maintain your own resettable running counter in firmware if you want the reset behavior to work independently. |
| `REG_TARIF2_ENERGY_TOTAL_HIGH/LOW` | 32–33 | Cumulative energy, tariff 2, same scale | **No real second tariff on the Fronius side** — leave at 0 (as in your example) since there's nothing to source it from. |
| *(not pasted, exists too)* `T2 partial` | 34–35 | Same | Leave at 0 for the same reason. |

## Conversion detail

`TotWhImp` is a SunSpec **float32** in the Fronius meter map, register offset ~40138–40139
(see `docs/fronius-meter-registers.md`). Read it as a float, then:

```
uint32_t reg32 = (uint32_t) lround(totWhImp_Wh / 10.0);  // Wh -> 0.01 kWh units
uint16_t high  = reg32 >> 16;
uint16_t low   = reg32 & 0xFFFF;
mb.Hreg(REG_TARIF1_ENERGY_TOTAL_HIGH, high);
mb.Hreg(REG_TARIF1_ENERGY_TOTAL_LOW,  low);
```

## What's still missing from this list — the actual PV-surplus signal

None of the registers you pasted carry the live power value the Vitotronic actually
uses to throttle/boost consumption. That's registers **36–52** (not in your excerpt),
in particular:

| ALE3 reg # | Field | Scale | Fronius SunSpec source |
|---|---|---|---|
| 36/41/46 | `URMS` phase 1/2/3 (V) | ×1 | `PhVphA` / `PhVphB` / `PhVphC` |
| 37/42/47 | `IRMS` phase 1/2/3 | 10⁻¹ A | `AphA` / `AphB` / `AphC` ×10 |
| 38/43/48 | `PRMS` phase 1/2/3 | 10⁻² kW | `-WphA`/`-WphB`/`-WphC` (W) ÷10 (sign flipped, see below) |
| 40/45/50 | `Cos phi` phase 1/2/3 | 10⁻² | `PFphA`/`PFphB`/`PFphC` ×100 |
| 51 | `PRMS total` | 10⁻² kW | `-W` (total, W) ÷10 (sign flipped, see below) |
| 52 | `QRMS total` | 10⁻² kVA | `VAR` (total) ÷10 |

Register 51 (`PRMS total`) is the one the Vitotronic's PV-surplus logic actually watches.
Its sign convention is confirmed further down — it's the **opposite** of the Fronius
meter's own convention, so the Fronius `W` value must be negated before writing it here.

## Implementation: registers 36–52

Register numbering below follows the datasheet's "register R transmitted as R-1" rule,
so the enum values are wire addresses (`doc_register - 1`) — consistent with how
`REG_TARIF2_ENERGY_TOTAL_LOW` (doc reg 33) would be wire address 32.

```cpp
// --- register offsets, continuing your existing REG_* enum ---
enum {
    // ... your existing REG_* entries above (up to REG_TARIF2_ENERGY_TOTAL_LOW = 32) ...
    REG_URMS_L1      = 35,  // doc reg 36
    REG_IRMS_L1      = 36,  // doc reg 37
    REG_PRMS_L1      = 37,  // doc reg 38
    REG_QRMS_L1      = 38,  // doc reg 39
    REG_COSPHI_L1    = 39,  // doc reg 40
    REG_URMS_L2      = 40,  // doc reg 41
    REG_IRMS_L2      = 41,  // doc reg 42
    REG_PRMS_L2      = 42,  // doc reg 43
    REG_QRMS_L2      = 43,  // doc reg 44
    REG_COSPHI_L2    = 44,  // doc reg 45
    REG_URMS_L3      = 45,  // doc reg 46
    REG_IRMS_L3      = 46,  // doc reg 47
    REG_PRMS_L3      = 47,  // doc reg 48
    REG_QRMS_L3      = 48,  // doc reg 49
    REG_COSPHI_L3    = 49,  // doc reg 50
    REG_PRMS_TOTAL   = 50,  // doc reg 51
    REG_QRMS_TOTAL   = 51,  // doc reg 52
};

// --- registration (once, at startup, alongside your existing addHreg calls) ---
mb.addHreg(REG_URMS_L1,   0);
mb.addHreg(REG_IRMS_L1,   0);
mb.addHreg(REG_PRMS_L1,   0);
mb.addHreg(REG_QRMS_L1,   0);
mb.addHreg(REG_COSPHI_L1, 0);
mb.addHreg(REG_URMS_L2,   0);
mb.addHreg(REG_IRMS_L2,   0);
mb.addHreg(REG_PRMS_L2,   0);
mb.addHreg(REG_QRMS_L2,   0);
mb.addHreg(REG_COSPHI_L2, 0);
mb.addHreg(REG_URMS_L3,   0);
mb.addHreg(REG_IRMS_L3,   0);
mb.addHreg(REG_PRMS_L3,   0);
mb.addHreg(REG_QRMS_L3,   0);
mb.addHreg(REG_COSPHI_L3, 0);
mb.addHreg(REG_PRMS_TOTAL, 0);
mb.addHreg(REG_QRMS_TOTAL, 0);
```

```cpp
// --- data pulled from the Fronius meter's SunSpec float32 registers ---
// (see docs/fronius-meter-registers.md for the source register addresses)
struct FroniusMeterReading {
    float voltagePhA, voltagePhB, voltagePhC;   // PhVphA/B/C   [V]
    float currentPhA, currentPhB, currentPhC;   // AphA/B/C     [A]
    float powerPhA,   powerPhB,   powerPhC;     // WphA/B/C     [W], signed
    float pfPhA,      pfPhB,      pfPhC;        // PFphA/B/C    [cos phi], signed
    float powerTotal;                           // W            [W], signed
    float reactivePowerTotal;                   // VAR          [VAr], signed
};

// --- conversion + write, called on every poll cycle ---
//
// SIGN CONVENTION (confirmed against Viessmann's own accessory manual, see below):
//   - Feeding power INTO the grid (PV surplus)  -> Vitotronic expects POSITIVE.
//   - Drawing power FROM the grid (import)      -> Vitotronic expects NEGATIVE.
// The Fronius meter's own SunSpec `W`/`WphX` uses the OPPOSITE convention
// (positive = import, negative = export) — see docs/fronius-meter-registers.md.
// So every power value is negated below when going from Fronius -> ALE3.
void updateAle3PowerRegisters(ModbusRTU &mb, const FroniusMeterReading &m) {
    mb.Hreg(REG_URMS_L1,   (uint16_t) lroundf(m.voltagePhA));            // V, x1
    mb.Hreg(REG_IRMS_L1,   (uint16_t) lroundf(m.currentPhA * 10.0f));    // A, x0.1
    mb.Hreg(REG_PRMS_L1,   (int16_t)  lroundf(-m.powerPhA / 10.0f));     // kW, x0.01, sign-flipped
    mb.Hreg(REG_COSPHI_L1, (int16_t)  lroundf(m.pfPhA * 100.0f));        // x0.01

    mb.Hreg(REG_URMS_L2,   (uint16_t) lroundf(m.voltagePhB));
    mb.Hreg(REG_IRMS_L2,   (uint16_t) lroundf(m.currentPhB * 10.0f));
    mb.Hreg(REG_PRMS_L2,   (int16_t)  lroundf(-m.powerPhB / 10.0f));     // sign-flipped
    mb.Hreg(REG_COSPHI_L2, (int16_t)  lroundf(m.pfPhB * 100.0f));

    mb.Hreg(REG_URMS_L3,   (uint16_t) lroundf(m.voltagePhC));
    mb.Hreg(REG_IRMS_L3,   (uint16_t) lroundf(m.currentPhC * 10.0f));
    mb.Hreg(REG_PRMS_L3,   (int16_t)  lroundf(-m.powerPhC / 10.0f));     // sign-flipped
    mb.Hreg(REG_COSPHI_L3, (int16_t)  lroundf(m.pfPhC * 100.0f));

    mb.Hreg(REG_PRMS_TOTAL, (int16_t) lroundf(-m.powerTotal / 10.0f));          // kW, x0.01, sign-flipped
    mb.Hreg(REG_QRMS_TOTAL, (int16_t) lroundf(m.reactivePowerTotal / 10.0f));   // kVA, x0.01

    // QRMS per phase (doc regs 39/44/49) isn't in the Fronius meter's per-phase
    // set documented here (only VAR total is read above) — Fronius does expose
    // VARphA/B/C too (see docs/fronius-meter-registers.md); add those to
    // FroniusMeterReading and write REG_QRMS_L1/L2/L3 the same way if the
    // Vitotronic actually reads per-phase reactive power.
}
```

### Sign convention — confirmed against Viessmann's PV-kit manual

Viessmann's own accessory installation manual for this exact meter, **"Montage- und
Serviceanleitung: Energiezähler, 1-phasig oder 3-phasig"** (doc no. 5516 060, for use
with Vitotronic 200 Typ WO1C), states explicitly (§ "Stromrichtung prüfen" /
"Hinweis zum Energiebezug..." / "Hinweis zur Energieeinspeisung..."):

- **`Energiebezug vom Stromnetz (EVU)`** (importing from grid) → *"Der Energiezähler
  zeigt die Leistung mit **negativem** Vorzeichen an"* — reported with a **negative**
  sign. ("Die Wärmepumpe soll keine Energie verbrauchen" — heat pump should not consume.)
- **`Energieeinspeisung ins Stromnetz (EVU)`** (feeding surplus into grid) → *"Die
  Leistung wird mit **positivem** Vorzeichen angezeigt"* — reported with a **positive**
  sign. ("Die Wärmepumpe soll Energie verbrauchen" — heat pump should consume/use surplus.)

This is the **opposite** of the Fronius meter's own SunSpec convention (positive =
import, negative = export — see `docs/fronius-meter-registers.md`), confirmed by the same
manual's wiring diagrams: the meter sits at the same physical point (grid connection,
bidirectional "Zweirichtungszähler") as the Fronius Smart Meter, just displaying the
opposite polarity. **The negation in the code above is required, not optional** — without
it, the Vitotronic would read PV surplus as "importing" and refuse to use it, and vice versa.

### Reactive power (`QRMS`) sign — deliberately left un-flipped

None of the three sourced documents (Viessmann's Energiezähler manual, Viessmann's ALE3
marketing datasheet, the SAIA ALE3 datasheet) state a sign convention for reactive power.
Every place a sign is discussed — `Stromrichtung prüfen`, the wiring-diagram notes, the
`Schwelle elektr. Leistung` threshold — talks exclusively about `Wirkleistung` (real
power); `Blindleistung`/`QRMS` isn't mentioned in connection with the PV-surplus function
at all, so there's no evidence the Vitotronic's self-consumption logic even reads it.

Reactive power's sign conventionally encodes power-factor quadrant (leading/capacitive vs.
lagging/inductive), not energy-flow direction — confirmed by the Fronius manual itself,
which defines `VAr`'s sign that way, independent of import/export. There's no stated
reason for the ALE3/Vitotronic side to redefine that. So `REG_QRMS_TOTAL` (and `QRMS`
per-phase, if you wire it up) pass the Fronius `VAR`/`VARphX` value straight through with
**no negation** — as already reflected in the code above.

This is lower-confidence than the real-power finding simply because Viessmann never
documents it either way, rather than because a source contradicts the flip. If the
Vitotronic's diagnostics expose a raw reactive-power reading once this is running, that's
the way to confirm it — nothing in the written material settles it definitively.

Sources:
- [Viessmann — Montage- und Serviceanleitung Energiezähler (5516 060)](https://community.viessmann.de/viessmann/attachments/viessmann/customers-electric-photovoltaics/62/1/MA%20Energiez%C3%A4hler.PDF)
- [Viessmann — Modbus-Stromzaehler-dreiphasig.pdf](https://community.viessmann.de/viessmann/attachments/viessmann/customers-heatpump-hybrid/2200/1/Modbus-Stromzaehler-dreiphasig.pdf)
- [SAIA-Burgess — ALE3 datasheet](https://sbc-support.com/uploads/tx_srcproducts/26-527_GER_DS_EnergyMeter-ALE3-with-Modbus.pdf)
