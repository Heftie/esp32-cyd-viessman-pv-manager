#!/usr/bin/env python3
"""Check Fronius Modbus TCP (SunSpec): common block, model 103 and model 160.

Usage: fronius_modbus_check.py [host]   (stdlib only, unit ID 1 = inverter)
"""
import socket
import struct
import sys

HOST = sys.argv[1] if len(sys.argv) > 1 else "192.168.178.115"
UNIT = 1


def read(addr, n, unit=UNIT):
    """Read n holding registers (FC3), zero-based addr."""
    with socket.create_connection((HOST, 502), 3) as s:
        s.settimeout(3)
        s.sendall(struct.pack(">HHHBBHH", 1, 0, 6, unit, 3, addr, n))
        d = s.recv(512)
    if d[7] & 0x80:
        raise IOError(f"modbus exception {d[8]}")
    return struct.unpack(">%dH" % (d[8] // 2), d[9:9 + d[8]])


def text(regs):
    return b"".join(struct.pack(">H", w) for w in regs).rstrip(b"\0").decode(errors="replace")


def s16(v):
    return v - 65536 if v >= 32768 else v


def scaled(v, sf):
    return s16(v) * 10 ** s16(sf)


def find_model(model_id):
    """Walk SunSpec models; return (zero-based data addr, length)."""
    addr = 40002  # first model header (1-based 40003)
    while True:
        mid, ln = read(addr, 2)
        if mid == 0xFFFF:
            raise LookupError(f"model {model_id} not found")
        if mid == model_id:
            return addr + 2, ln
        addr += 2 + ln


def main():
    assert text(read(40000, 2)) == "SunS", "no SunSpec header"
    c = read(40004, 64)
    print(f"Mn {text(c[0:16])} | Md {text(c[16:32])} | Vr {text(c[40:48])} | SN {text(c[48:64])}")

    a, ln = find_model(103)
    r = read(a, ln)
    print(f"[103] AC {scaled(r[12], r[13]):.1f} W, {scaled(r[14], r[15]):.2f} Hz, "
          f"V {[round(scaled(r[i], r[11]), 1) for i in (8, 9, 10)]}, "
          f"{(r[22] << 16 | r[23]) * 10 ** s16(r[24]) / 1000:.1f} kWh, "
          f"{scaled(r[31], r[35]):.1f} C, state {r[36]}")

    a, ln = find_model(160)
    r = read(a, ln)
    sfa, sfv, sfw = s16(r[0]), s16(r[1]), s16(r[2])
    for i in range(r[6]):
        m = r[8 + 20 * i:28 + 20 * i]
        print(f"[160] {text(m[1:9])}: {s16(m[9]) * 10 ** sfa:.3f} A, "
              f"{s16(m[10]) * 10 ** sfv:.1f} V, {s16(m[11]) * 10 ** sfw:.1f} W")


if __name__ == "__main__":
    main()
