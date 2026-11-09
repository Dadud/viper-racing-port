#!/usr/bin/env python3
"""Run Clang's parser over compile-database translation units and report coverage."""

import argparse
import concurrent.futures
import json
import os
import shlex
import subprocess
import sys
from pathlib import Path


def command_for(entry, clang):
    args = entry.get("arguments")
    if args is None:
        args = shlex.split(entry["command"], posix=False)
    # Remove the MSVC compiler and code-generation/output switches. Keep the
    # remaining defines, include paths, language flags, and target-sensitive
    # options from the real build command.
    args = list(args[1:])
    filtered = []
    skip_next = False
    for arg in args:
        if skip_next:
            skip_next = False
            continue
        low = arg.lower()
        if low in {"/c", "/ld", "/link"}:
            continue
        # clang-cl's i686 target uses this MSVC STL compatibility define for
        # 64-bit atomics. MSVC's /arch:IA32 switch is codegen-only and its
        # Clang analogue selects a headers-incompatible _M_IX86_FP=0 branch.
        if low == "/arch:ia32":
            continue
        if low in {"/fo", "/fe", "/fd", "/fa", "/fi"}:
            skip_next = True
            continue
        if any(low.startswith(prefix) for prefix in ("/fo", "/fe", "/fd", "/fa")):
            continue
        if low in {"/o1", "/o2", "/ox", "/od", "/mt", "/mtd", "/md", "/mdd", "/w3"}:
            continue
        filtered.append(arg)
    return [clang, "--target=i686-pc-windows-msvc", "/Zs", "-Wno-invalid-offsetof",
            "/D_M_IX86_FP=2", "/D__PRFCHWINTRIN_H", *filtered]


def parse_one(index, entry, clang):
    source = Path(entry["file"])
    command = command_for(entry, clang)
    result = subprocess.run(command, cwd=entry["directory"], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return {
        "index": index,
        "source": str(source),
        "ok": result.returncode == 0,
        "returncode": result.returncode,
        "diagnostics": result.stderr.splitlines(),
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--database", required=True, type=Path)
    parser.add_argument("--clang", required=True)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--match", help="only parse files whose path contains this string")
    ns = parser.parse_args()

    database = json.loads(ns.database.read_text(encoding="utf-8-sig"))
    entries = [entry for entry in database if not ns.match or ns.match.lower() in entry["file"].lower()]
    if not entries:
        parser.error("the compile database selection contains no translation units")
    if ns.jobs < 1:
        parser.error("--jobs must be positive")

    outcomes = [None] * len(entries)
    with concurrent.futures.ThreadPoolExecutor(max_workers=ns.jobs) as pool:
        futures = [pool.submit(parse_one, i, entry, ns.clang) for i, entry in enumerate(entries)]
        for future in concurrent.futures.as_completed(futures):
            outcome = future.result()
            outcomes[outcome["index"]] = outcome
            state = "parsed" if outcome["ok"] else "failed"
            print(f"[{outcome['index'] + 1}/{len(entries)}] {state}: {Path(outcome['source']).name}", flush=True)

    passed = sum(1 for result in outcomes if result["ok"])
    report = {
        "compiler": subprocess.run([ns.clang, "--version"], text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT).stdout.splitlines()[0],
        "parser_mode": "clang-cl syntax-only, i686 Windows/MSVC; omits code-generation flags and uses parser-only _M_IX86_FP=2 and __PRFCHWINTRIN_H compatibility defines",
        "database": str(ns.database.resolve()),
        "translation_units": len(entries),
        "parsed": passed,
        "failed": len(entries) - passed,
        "coverage_percent": round(100.0 * passed / len(entries), 2),
        "results": outcomes,
    }
    if ns.report:
        ns.report.parent.mkdir(parents=True, exist_ok=True)
        ns.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Clang parser coverage: {passed}/{len(entries)} ({report['coverage_percent']}%)")
    if report["failed"]:
        for result in outcomes:
            if result["ok"]:
                continue
            print(f"\n--- {result['source']} ---")
            for line in result["diagnostics"][:8]:
                print(line)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
