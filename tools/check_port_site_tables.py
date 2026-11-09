#!/usr/bin/env python3
"""Verify generated per-build hook-site hashes against their executable images.

Each nonzero hash in a sites_*.inc row fingerprints the four bytes at the
mapped address (a function prologue or a translated patch operand). Zero means
that the row only maps an address and has no byte fingerprint to check.

Example:
    python tools/check_port_site_tables.py \
      --case v1.2.4 path/to/race.bin hook/sites_v12_4.inc \
      --case v1.2.5 path/to/race.bin hook/sites_v12_5.inc
"""
import argparse
import json
import re
import struct
import sys
from pathlib import Path

from match_builds import load


ROW = re.compile(r"\{\s*0x([0-9a-fA-F]+),\s*0x([0-9a-fA-F]+),\s*0x([0-9a-fA-F]+)\s*\}")
ADAPTER_PREFIXES = {
    0x004067C0: (5, 0x6ADE35FF),  # RaceEnd
    0x00419720: (5, 0x29A128C7),  # resource-set loader
    0x004871E0: (6, 0x4B4E4B93),  # single-race setup
    0x0040A3F0: (7, 0xCCBD15AB),  # single-race load lifecycle
}


def fnv1a32(data: bytes) -> int:
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def pe_sections(image: bytes) -> tuple[int, list[tuple[int, int]]]:
    pe_offset = struct.unpack_from("<I", image, 0x3C)[0]
    if image[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise ValueError("invalid PE signature")
    section_count = struct.unpack_from("<H", image, pe_offset + 6)[0]
    optional_size = struct.unpack_from("<H", image, pe_offset + 20)[0]
    optional = pe_offset + 24
    if struct.unpack_from("<H", image, optional)[0] != 0x10B:
        raise ValueError("expected a 32-bit PE image")
    image_base = struct.unpack_from("<I", image, optional + 28)[0]
    sections_start = optional + optional_size
    sections = []
    for index in range(section_count):
        offset = sections_start + index * 40
        virtual_size, virtual_address, raw_size = struct.unpack_from("<III", image, offset + 8)
        span = max(virtual_size, raw_size)
        sections.append((image_base + virtual_address, image_base + virtual_address + span))
    return image_base, sections


def verify(binary: Path, table: Path) -> dict:
    image, image_base, text_va, text = load(str(binary))
    parsed_base, sections = pe_sections(image)
    if image_base != parsed_base:
        raise ValueError("image base differs between PE readers")
    checked = 0
    address_only = 0
    outside_text = 0
    in_sections = 0
    mismatches = []
    rows = 0
    mapped_addresses = {}
    for line_no, line in enumerate(table.read_text(encoding="utf-8").splitlines(), 1):
        match = ROW.search(line)
        if not match:
            continue
        rows += 1
        source_va, mapped_va, expected_hash = (int(value, 16) for value in match.groups())
        mapped_addresses[source_va] = mapped_va
        if not any(start <= mapped_va < end for start, end in sections):
            mismatches.append({"line": line_no, "source_va": f"0x{source_va:08x}",
                               "mapped_va": f"0x{mapped_va:08x}", "error": "outside mapped PE sections"})
            continue
        in_sections += 1
        if expected_hash == 0:
            address_only += 1
            continue
        offset = mapped_va - text_va
        if offset < 0 or offset + 4 > len(text):
            outside_text += 1
            mismatches.append({"line": line_no, "source_va": f"0x{source_va:08x}",
                               "mapped_va": f"0x{mapped_va:08x}", "error": "outside mapped text section"})
            continue
        checked += 1
        actual_hash = fnv1a32(text[offset:offset + 4])
        if actual_hash != expected_hash:
            mismatches.append({"line": line_no, "source_va": f"0x{source_va:08x}",
                               "mapped_va": f"0x{mapped_va:08x}",
                               "expected": f"0x{expected_hash:08x}", "actual": f"0x{actual_hash:08x}"})
    adapter_checks = 0
    for source_va, (length, expected_hash) in ADAPTER_PREFIXES.items():
        mapped_va = mapped_addresses.get(source_va)
        offset = -1 if mapped_va is None else mapped_va - text_va
        if offset < 0 or offset + length > len(text):
            mismatches.append({"source_va": f"0x{source_va:08x}",
                               "mapped_va": None if mapped_va is None else f"0x{mapped_va:08x}",
                               "error": "adapter prefix is missing or outside mapped text section"})
            continue
        adapter_checks += 1
        actual_hash = fnv1a32(text[offset:offset + length])
        if actual_hash != expected_hash:
            mismatches.append({"source_va": f"0x{source_va:08x}", "mapped_va": f"0x{mapped_va:08x}",
                               "expected_adapter_prefix": f"0x{expected_hash:08x}",
                               "actual": f"0x{actual_hash:08x}"})
    return {"binary": str(binary), "table": str(table), "rows": rows,
            "fingerprints_checked": checked, "address_only": address_only,
            "addresses_in_sections": in_sections, "adapter_prefixes_checked": adapter_checks,
            "mismatches": mismatches, "ok": rows > 0 and not mismatches}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", nargs=3, action="append", metavar=("BUILD", "BINARY", "TABLE"), required=True,
                        help="build label, executable image, and generated sites include; may be repeated")
    parser.add_argument("--json", type=Path, help="also write a machine-readable report")
    args = parser.parse_args()

    reports = []
    failed = False
    for label, binary_arg, table_arg in args.case:
        binary, table = Path(binary_arg), Path(table_arg)
        if not binary.is_file() or not table.is_file():
            report = {"build": label, "binary": str(binary), "table": str(table),
                      "ok": False, "error": "binary or table does not exist"}
        else:
            try:
                report = verify(binary, table)
                report["build"] = label
            except (OSError, ValueError, struct.error) as error:
                report = {"build": label, "binary": str(binary), "table": str(table),
                          "ok": False, "error": str(error)}
        reports.append(report)
        if report["ok"]:
            print(f"{label}: {report['fingerprints_checked']} fingerprints match; "
                  f"{report['adapter_prefixes_checked']} adapter prefixes match; "
                  f"{report['addresses_in_sections']} mapped addresses in sections; "
                  f"{report['address_only']} address-only rows; {report['rows']} rows")
        else:
            failed = True
            detail = report.get("error") or f"{len(report.get('mismatches', []))} mismatches"
            print(f"{label}: FAILED ({detail})")
            for mismatch in report.get("mismatches", [])[:12]:
                print(f"  line {mismatch['line']}: {mismatch}")

    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps({"reports": reports}, indent=2) + "\n", encoding="utf-8")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
