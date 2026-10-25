# Fronius Verto Plus — Modbus TCP test result

Tested 2026-10-25 against a Fronius Verto 17.5 Plus (fw 1.42.9-1) at `192.168.178.115:502`,
using `tools/fronius_modbus_check.py` (stdlib only). Smart meter was not connected.

## Verified working

- Modbus TCP on port 502, unit ID **1** (inverter), FC3 holding registers.
- SunSpec header `SunS` at 40001 (zero-based address 40000).
- Common block (Mn, Md, Vr, SN) readable.
- Models found by walking the chain: 1, 103, 120, 121, 122, 123, 160, 124.
- Model 103 (3-phase inverter, integer + scale factors): AC power, frequency, phase
  voltage/current, lifetime energy, cabinet temperature, state. All plausible.
- Model 160 (MPPT): 5 modules (MPPT 1-3, StCha 4, StDisCha 5) with A/V/W per module.

## Verified not available (expected)

- Unit IDs 240 and 200 return Modbus exception 11 (gateway target device failed to respond)
  because no smart meter is connected. Meter registers (see `fronius-meter-registers.md`)
  are therefore **untested**.

## Notes

- Model addresses are not fixed: walk the model chain instead of hard-coding them.
  In this setup model 103 data starts at 1-based register 40072, model 160 at 40255.
- Model 103 scale factors are int16 (`value * 10^SF`). DC V/A in model 103 read 0
  (unsupported); use model 160 for DC values.
- At very low light, model 160 shows noise: MPPT 3 power is negative (about -23 W) and
  the battery modules show a fixed -233 V without a battery. Do not trust these values.
- Inverter state 4 = THROTTLED.
