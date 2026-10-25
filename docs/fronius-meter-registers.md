# Fronius Smart Meter — SunSpec Modbus Registers (Float Model 211/212/213)

Source: `data/42,0410,2049.pdf` (Fronius Datamanager Modbus TCP & RTU manual) and
`data/SE_EI_Modbus_Sunspec_Maps_State_Codes_Events/Meter_Register_Map_Float_v1.0.xlsx`.

Applies to the separate Smart Meter device, **not** the inverter. Read via Modbus TCP,
port 502, on its own unit/device ID (default **240**, or 241/242/... for additional meters).

## ⚠️ Addresses are not fixed

The manual explicitly states register addresses depend on which optional SunSpec blocks
are enabled and can shift. Do not hard-code the absolute addresses below — read the
Common Model (`SID`/`ID`/`L` starting at register 40001) to find the Meter Model's `ID`
register (211/212/213), then use the **offsets** below relative to that register.
Addresses shown assume the common minimal layout (meter model starting at 40070) and
are only a reference/example.

All multi-register values are **IEEE754 float32**, stored as 2 consecutive 16-bit
registers, most-significant word first (standard Modbus register order).

## Identification (Common Model, offset from 40001)

| Reg | Name | Type | Description |
|---|---|---|---|
| 40005–40020 | `Mn` | String32 | Manufacturer ("Fronius") |
| 40021–40036 | `Md` | String32 | Meter model/device name |
| 40045–40052 | `Vr` | String16 | Meter firmware version |
| 40053–40068 | `SN` | String32 | Meter serial number |

## Current

| Reg | Name | Type | Unit | Description |
|---|---|---|---|---|
| 40072–40073 | `A` | float32 | A | Total AC current (sum of phases) |
| 40074–40075 | `AphA` | float32 | A | Phase A current |
| 40076–40077 | `AphB` | float32 | A | Phase B current |
| 40078–40079 | `AphC` | float32 | A | Phase C current |

## Voltage

| Reg | Name | Type | Unit | Description |
|---|---|---|---|---|
| 40080–40081 | `PhV` | float32 | V | Average phase-to-neutral voltage |
| 40082–40083 | `PhVphA` | float32 | V | Phase A-to-neutral voltage |
| 40084–40085 | `PhVphB` | float32 | V | Phase B-to-neutral voltage |
| 40086–40087 | `PhVphC` | float32 | V | Phase C-to-neutral voltage |
| 40088–40089 | `PPV` | float32 | V | Average phase-to-phase voltage |
| 40090–40091 | `PPVphAB` | float32 | V | Voltage A-B |
| 40092–40093 | `PPVphBC` | float32 | V | Voltage B-C |
| 40094–40095 | `PPVphCA` | float32 | V | Voltage C-A |

For a single-phase installation, use `A`/`PhV` (or the `...phA` variants — both read the
same on single-phase meters).

## Frequency

| Reg | Name | Type | Unit |
|---|---|---|---|
| 40096–40097 | `Hz` | float32 | Hz |

## Real Power

| Reg | Name | Type | Unit | Description |
|---|---|---|---|---|
| 40098–40099 | `W` | float32 | W | **Total AC real power.** Sign convention at the grid connection point: **positive = importing/consuming from grid, negative = exporting/feeding into grid.** |
| 40100–40101 | `WphA` | float32 | W | Phase A power |
| 40102–40103 | `WphB` | float32 | W | Phase B power |
| 40104–40105 | `WphC` | float32 | W | Phase C power |

## Apparent Power

| Reg | Name | Type | Unit |
|---|---|---|---|
| 40106–40107 | `VA` | float32 | VA |
| 40108–40109 | `VAphA` | float32 | VA |
| 40110–40111 | `VAphB` | float32 | VA |
| 40112–40113 | `VAphC` | float32 | VA |

## Reactive Power

| Reg | Name | Type | Unit |
|---|---|---|---|
| 40114–40115 | `VAR` | float32 | VAr |
| 40116–40117 | `VARphA` | float32 | VAr |
| 40118–40119 | `VARphB` | float32 | VAr |
| 40120–40121 | `VARphC` | float32 | VAr |

## Power Factor

| Reg | Name | Type | Unit |
|---|---|---|---|
| 40122–40123 | `PF` | float32 | cos(φ) |
| 40124–40125 | `PFphA` | float32 | cos(φ) |
| 40126–40127 | `PFphB` | float32 | cos(φ) |
| 40128–40129 | `PFphC` | float32 | cos(φ) |

## Cumulative Energy

| Reg | Name | Type | Unit | Description |
|---|---|---|---|---|
| 40130–40131 | `TotWhExp` | float32 | Wh | Total energy exported to grid (lifetime counter) |
| 40138–40139 | `TotWhImp` | float32 | Wh | Total energy imported from grid (lifetime counter) |
| 40146–40147 | `TotVAhExp` | float32 | VAh | Total apparent energy exported |
| 40154–40155 | `TotVAhImp` | float32 | VAh | Total apparent energy imported |

Per-phase Wh/VAh/VArh counters and the VArh quadrant registers (`TotWhExpPhA`,
`TotVArhImpQ1`, etc.) are listed in the map but marked **"register not supported"** by
Fronius — they are not populated and were left out above.

## Events

| Reg | Name | Type | Description |
|---|---|---|---|
| 40194–40195 | `Evt` | bitfield32 | Meter event/error flags |

## Sign convention (meter at grid connection, `Meter_Location = 0`)

| | Positive `W`/`WphX` | Negative `W`/`WphX` |
|---|---|---|
| Meaning | Consuming from grid (import) | Feeding into grid (export/surplus) |
| Energy counter that increases | `TotWhImp` | `TotWhExp` |

So for tracking overproduction: watch `W` going negative for instantaneous surplus, or
the growth rate of `TotWhExp` for a running export total.
