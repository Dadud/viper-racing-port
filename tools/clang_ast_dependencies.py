#!/usr/bin/env python3
"""Extract a bounded call/dependency slice from real compile-database ASTs.

This reports what Clang parsed for explicitly selected function filters. It is
not a whole-program call graph: indirect calls and the legacy ccall/tcall/vcall
wrappers remain classified separately instead of being guessed.
"""

from __future__ import annotations

import argparse
import bisect
import json
import subprocess
import sys
from pathlib import Path

from clang_parse_coverage import command_for


CALL_KINDS = {"CallExpr", "CXXMemberCallExpr", "CXXOperatorCallExpr"}
FUNCTION_KINDS = {"FunctionDecl", "CXXMethodDecl", "CXXConstructorDecl", "CXXDestructorDecl"}


def iter_json_objects(output: str):
    decoder = json.JSONDecoder()
    offset = 0
    while True:
        start = output.find("{", offset)
        if start < 0:
            return
        try:
            value, offset = decoder.raw_decode(output, start)
        except json.JSONDecodeError:
            offset = start + 1
            continue
        yield value


def children(node):
    return node.get("inner", ()) if isinstance(node, dict) else ()


def walk(node):
    if not isinstance(node, dict):
        return
    yield node
    for child in children(node):
        yield from walk(child)


def referenced_declarations(node):
    result = []
    for item in walk(node):
        ref = item.get("referencedDecl")
        if not isinstance(ref, dict):
            continue
        if ref.get("kind") in FUNCTION_KINDS | {"FunctionTemplateDecl", "VarDecl", "ParmVarDecl"}:
            result.append({"kind": ref.get("kind"), "name": ref.get("name"), "type": ref.get("type", {}).get("qualType")})
    return result


def call_site(node, line_starts):
    parts = children(node)
    callee = parts[0] if parts else {}
    refs = referenced_declarations(callee)
    member = next((item for item in walk(callee) if item.get("kind") == "MemberExpr"), None)
    name = member.get("name") if member else refs[0]["name"] if refs else None
    ref_kind = "MemberExpr" if member else refs[0]["kind"] if refs else None
    args = parts[1:]
    dispatch = sorted({
        ref.get("name") for arg in args for item in walk(arg)
        for ref in [item.get("referencedDecl", {})]
        if isinstance(ref, dict) and isinstance(ref.get("name"), str) and
        (ref["name"].startswith("F_") or ref["name"].startswith("S_"))
    })
    literals = sorted({
        item.get("value") for arg in args for item in walk(arg)
        if item.get("kind") in {"IntegerLiteral", "CXXBoolLiteralExpr"} and item.get("value") is not None
    })
    location = node.get("range", {}).get("begin", {})
    offset = location.get("offset")
    if offset is None:
        offset = location.get("expansionLoc", {}).get("offset")
    line = bisect.bisect_right(line_starts, offset) if offset is not None else None
    if member:
        dispatch_kind = "member-method (virtual dispatch unresolved)"
    elif ref_kind in {"VarDecl", "ParmVarDecl"}:
        dispatch_kind = "function-pointer"
    elif name in {"ccall", "tcall", "vcall"}:
        dispatch_kind = "legacy-address-wrapper"
    elif name is None:
        dispatch_kind = "unresolved-expression"
    else:
        dispatch_kind = "direct-function"
    return {
        "line": line,
        "expression_kind": node.get("kind"),
        "callee": name,
        "callee_declaration_kind": ref_kind,
        "indirect": ref_kind in {"VarDecl", "ParmVarDecl"} or name is None,
        "dispatch_kind": dispatch_kind,
        "legacy_dispatch_symbols": dispatch,
        "integer_arguments": literals,
    }


def extract_function(root, line_starts):
    name = root.get("name", "<unnamed>")
    calls = []

    def visit(node):
        if not isinstance(node, dict):
            return
        if node is not root and node.get("kind") in FUNCTION_KINDS:
            return
        if node.get("kind") in CALL_KINDS:
            calls.append(call_site(node, line_starts))
        for child in children(node):
            visit(child)

    visit(root)
    unique = {}
    for call in calls:
        key = json.dumps(call, sort_keys=True)
        unique[key] = call
    calls = sorted(unique.values(), key=lambda item: (item["line"] or 0, item["callee"] or ""))
    defined = {node.get("name") for node in walk(root) if node.get("kind") in FUNCTION_KINDS and node.get("name")}
    return {
        "name": name,
        "declaration_kind": root.get("kind"),
        "signature": root.get("type", {}).get("qualType"),
        "calls": calls,
        "defined_functions_inside_slice": sorted(defined),
        "direct_callees_outside_slice": sorted({
            call["callee"] for call in calls
            if call["callee"] and call["dispatch_kind"] == "direct-function" and call["callee"] not in defined
        }),
        "indirect_calls": [call for call in calls if call["indirect"]],
        "member_calls": [call for call in calls if call["dispatch_kind"] == "member-method (virtual dispatch unresolved)"],
        "legacy_dispatch_calls": [call for call in calls if call["callee"] in {"ccall", "tcall", "vcall"}],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", required=True, type=Path)
    parser.add_argument("--clang", required=True)
    parser.add_argument("--source", required=True, help="source path substring from the compile database")
    parser.add_argument("--function", required=True, action="append", help="AST dump filter; repeat for multiple functions")
    parser.add_argument("--report", required=True, type=Path)
    ns = parser.parse_args()

    entries = json.loads(ns.database.read_text(encoding="utf-8-sig"))
    matches = [entry for entry in entries if ns.source.lower() in entry["file"].lower()]
    if len(matches) != 1:
        parser.error(f"--source must match one compile-database entry; matched {len(matches)}")

    entry = matches[0]
    source_text = Path(entry["file"]).read_text(encoding="utf-8-sig")
    line_starts = [0]
    line_starts.extend(index + 1 for index, char in enumerate(source_text) if char == "\n")
    records = []
    for function_filter in ns.function:
        command = command_for(entry, ns.clang)
        command.extend(["-Xclang", "-ast-dump=json", "-Xclang", f"-ast-dump-filter={function_filter}"])
        result = subprocess.run(command, cwd=entry["directory"], text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        roots = list(iter_json_objects(result.stdout))
        records.append({
            "function_filter": function_filter,
            "returncode": result.returncode,
            "diagnostics": result.stderr.splitlines(),
            "matched_definitions": [extract_function(root, line_starts) for root in roots
                                    if isinstance(root, dict) and root.get("kind") in FUNCTION_KINDS],
        })
        print(f"AST {function_filter}: {len(records[-1]['matched_definitions'])} definitions, exit {result.returncode}", flush=True)

    report = {
        "compiler": subprocess.run([ns.clang, "--version"], text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT).stdout.splitlines()[0],
        "database": str(ns.database.resolve()),
        "source": entry["file"],
        "target": "i686-pc-windows-msvc (parser settings derived from the actual x86 compile command)",
        "scope_note": "Selected AST definitions only. Direct function names, wrapper dispatch symbols, and indirect calls are separate; this is not a complete call graph.",
        "slices": records,
    }
    ns.report.parent.mkdir(parents=True, exist_ok=True)
    ns.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if any(record["returncode"] != 0 or not record["matched_definitions"] for record in records):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
