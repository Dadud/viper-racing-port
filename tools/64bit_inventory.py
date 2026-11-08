#!/usr/bin/env python3
"""Read-only lexical inventory of guest addresses and PORT_FN registrations.

No game executable, build artifacts, compiler, or external packages are required.
This is not a C++ parser: macros and conditional compilation are not evaluated,
references are not typed, and equal address literals do not prove a call edge.
"""
from __future__ import annotations

import argparse
import bisect
import ast
import json
import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REGISTRATION = re.compile(r'\b(PORT_FN(?:_BUILDS|_GL|_AUDIO)?)\s*\(')
ADDRESS = re.compile(r'\b0[xX][0-9a-fA-F]+(?:[uUlL]+)?\b')
LITERAL = re.compile(r'0[xX][0-9a-fA-F]+(?:[uUlL]+)?\Z')


def masked(source: str, *, strings: bool) -> str:
    """Blank comments (and optionally literals/directives), preserving offsets and lines."""
    chars = list(source)
    n, i = len(source), 0

    def blank(start: int, end: int) -> None:
        for j in range(start, min(end, n)):
            if source[j] not in '\r\n':
                chars[j] = ' '

    while i < n:
        if source.startswith('//', i):
            end = source.find('\n', i + 2)
            end = n if end < 0 else end
            blank(i, end); i = end
        elif source.startswith('/*', i):
            end = source.find('*/', i + 2)
            end = n if end < 0 else end + 2
            blank(i, end); i = end
        elif source.startswith('R"', i):
            op = source.find('(', i + 2, min(n, i + 19))
            delim = source[i + 2:op] if op >= 0 else ''
            if op >= 0 and not any(c.isspace() or c in '\\()' for c in delim):
                close = source.find(')' + delim + '"', op + 1)
                end = n if close < 0 else close + len(delim) + 2
                if strings: blank(i, end)
                i = end
            else:
                i += 1
        elif source[i] in ('"', "'"):
            quote, start = source[i], i
            i += 1
            while i < n:
                if source[i] == '\\': i += 2
                elif source[i] == quote:
                    i += 1; break
                else: i += 1
            if strings: blank(start, i)
        else:
            i += 1
    # Ignore directives, including continued macro definitions; this deliberately
    # does not evaluate #if, so inventory includes text in inactive branches.
    in_directive = False
    offset = 0
    for line in source.splitlines(keepends=True):
        directive = in_directive or line.lstrip().startswith('#')
        if directive:
            blank(offset, offset + len(line))
            in_directive = line.rstrip('\r\n').rstrip().endswith('\\')
        else:
            in_directive = False
        offset += len(line)
    return ''.join(chars)


def first_args(mask: str, start: int, count: int = 3) -> list[tuple[int, int]] | None:
    """Get first macro arguments, respecting nested parens/brackets/braces and literals."""
    depth = []
    result = []
    begin = start
    pairs = {')': '(', ']': '[', '}': '{'}
    for pos in range(start, len(mask)):
        ch = mask[pos]
        if ch in '([{': depth.append(ch)
        elif ch in ')]}':
            if ch == ')' and not depth:
                result.append((begin, pos))
                return result if len(result) >= count else None
            if not depth or depth.pop() != pairs[ch]: return None
        elif ch == ',' and not depth:
            result.append((begin, pos)); begin = pos + 1
            if len(result) == count: return result
    return None


def parse_integer(text: str) -> int | None:
    s = text.strip()
    return int(re.sub(r'[uUlL]+$', '', s), 16) if LITERAL.fullmatch(s) else None


def scan_source(path: str, source: str) -> tuple[list[dict], list[dict], list[dict]]:
    code = masked(source, strings=True)
    # Keep original literal contents for extracting the name, but comments and
    # directives must remain blank for macro parsing.
    visible = masked(source, strings=False)
    regs, unresolved, refs = [], [], []
    registration_spans = []
    lines = [i for i, ch in enumerate(source) if ch == "\n"]
    def line_number(pos: int) -> int: return bisect.bisect_left(lines, pos) + 1
    for m in REGISTRATION.finditer(code):
        pieces = first_args(code, m.end())
        line = line_number(m.start())
        if pieces is None:
            unresolved.append({'file': path, 'line': line, 'macro': m.group(1), 'reason': 'cannot parse first three arguments'})
            continue
        registration_spans.append(pieces[0])
        args = [visible[a:b].strip() for a, b in pieces]
        address = parse_integer(args[0])
        try:
            label = ast.literal_eval(args[1])
            if not isinstance(label, str): label = None
        except (ValueError, SyntaxError):
            label = None
        entry = {'file': path, 'line': line, 'macro': m.group(1), 'address_expression': args[0],
                 'address': f'0x{address:08x}' if address is not None else None,
                 'name': label, 'name_expression': args[1], 'implementation_expression': args[2]}
        regs.append(entry)
        if address is None:
            unresolved.append({'file': path, 'line': line, 'macro': m.group(1), 'reason': 'non-literal registration address'})
    starts = [a for a, _ in registration_spans]
    for m in ADDRESS.finditer(code):
        value = parse_integer(m.group(0))
        if value is not None and 0x400000 <= value < 0x600000:
            idx = bisect.bisect_right(starts, m.start()) - 1
            in_registration = idx >= 0 and m.start() < registration_spans[idx][1]
            refs.append({'file': path, 'line': line_number(m.start()), 'address': f'0x{value:08x}',
                         'category': 'registration_argument' if in_registration else 'unclassified_literal'})
    return regs, unresolved, refs


def inventory(directory: Path) -> dict:
    registrations, unresolved, addresses = [], [], []
    files = sorted(directory.glob('*.cpp'))
    for path in files:
        # Replacement preserves the ability to inventory source files with legacy bytes.
        text = path.read_text(encoding='utf-8', errors='replace')
        regs, warnings, refs = scan_source(path.name, text)
        registrations.extend(regs); unresolved.extend(warnings); addresses.extend(refs)
    targets = {r['address'] for r in registrations if r['address'] is not None}
    for r in addresses:
        r['matches_registration'] = r['address'] in targets
        if r['category'] != 'registration_argument' and r['matches_registration']:
            r['category'] = 'matching_registered_literal'
    by_file = Counter(r['file'] for r in registrations)
    return {
        'schema_version': 1,
        'method': 'lexical; preprocessor branches are not evaluated; addresses and signatures are not resolved',
        'summary': {'source_files': len(files), 'registrations': len(registrations),
                    'unique_literal_registration_addresses': len(targets),
                    'unresolved_registrations': len(unresolved), 'guest_range_literal_occurrences': len(addresses),
                    'address_categories': dict(sorted(Counter(r['category'] for r in addresses).items())),
                    'registration_count_by_file': dict(sorted(by_file.items()))},
        'registrations': registrations, 'unresolved': unresolved, 'address_occurrences': addresses,
    }


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-dir', type=Path, default=ROOT / 'hook', help='directory of .cpp files (default: hook/)')
    p.add_argument('--output', type=Path, help='JSON output file (default: stdout)')
    args = p.parse_args()
    if not args.source_dir.is_dir():
        p.error(f'not a directory: {args.source_dir}')
    data = json.dumps(inventory(args.source_dir), indent=2, ensure_ascii=True) + '\n'
    if args.output:
        args.output.write_text(data, encoding='utf-8')
    else:
        sys.stdout.write(data)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
