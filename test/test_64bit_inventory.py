"""Asset-free tests for the lexical 64-bit migration inventory."""
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('vp_inventory', ROOT / 'tools' / '64bit_inventory.py')
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class InventoryTest(unittest.TestCase):
    def test_comments_strings_and_directives(self):
        source = r'''// PORT_FN(0x00401234, "comment", fake, fp)
#define PORT_FN(x, y, z, fp) \\
 PORT_FN(0x00400000, "definition", wrong, fp)
const char* msg = "PORT_FN(0x00401111, fake, fake, fake) 0x00408888";
const char* raw = R"xyz(0x00407777 // PORT_FN(0x00402222, x, y, z))xyz";
/* 0x00406666 */
PORT_FN(0x00401000, "Name", actual, fp)
PORT_FN_GL(
  0x00401001u, "Second", (wrap(inner)), fp)
PORT_FN(ADDR, "dynamic", fun, fp)
const auto p = 0x00401000;
const auto q = 0x1234;
'''
        regs, warnings, refs = mod.scan_source('fake.cpp', source)
        self.assertEqual(len(regs), 3)
        self.assertEqual([r['address'] for r in regs], ['0x00401000', '0x00401001', None])
        self.assertEqual(regs[1]['implementation_expression'], '(wrap(inner))')
        self.assertEqual(regs[0]['name'], 'Name')
        self.assertEqual(warnings[0]['reason'], 'non-literal registration address')
        self.assertEqual([r['address'] for r in refs], ['0x00401000', '0x00401001', '0x00401000'])
        self.assertEqual(regs[0]['line'], 7)

    def test_malformed_macro_does_not_fabricate_entry(self):
        regs, warnings, _ = mod.scan_source('bad.cpp', 'PORT_FN(0x00401234, "name"')
        self.assertEqual(regs, [])
        self.assertEqual(warnings[0]['reason'], 'cannot parse first three arguments')

    def test_dispatch_inventory_preserves_unresolved_expressions(self):
        source = '''ccall<void>(F_DoThing, value);\n'''
        source += '''tcall<int>(F_Method, self, value);\n'''
        source += '''vcall<void>(object, slot, argument);\n'''
        source += '''ccall<int>((FunctionType)0x00401234, value);\n'''
        sites = mod.dispatch_sites('calls.cpp', source)
        self.assertEqual([s['wrapper'] for s in sites], ['ccall', 'tcall', 'vcall', 'ccall'])
        self.assertEqual(sites[0]['target_kind'], 'symbol')
        self.assertEqual(sites[1]['target_expression'], 'F_Method')
        self.assertEqual(sites[2]['receiver_expression'], 'object')
        self.assertEqual(sites[2]['slot_expression'], 'slot')
        self.assertEqual(sites[3]['target_kind'], 'expression')

    def test_deterministic_cli_and_source_unchanged(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            cpp = root / 'one.cpp'
            original = 'PORT_FN(0x00401111, "one", impl, fp)\nint x = 0x00401111;\n'
            cpp.write_text(original)
            (root / 'ignored.h').write_text('PORT_FN(0x00401112, "two", f, fp)\n')
            command = [sys.executable, str(ROOT / 'tools' / '64bit_inventory.py'), '--source-dir', str(root)]
            first = subprocess.check_output(command, text=True)
            self.assertEqual(first, subprocess.check_output(command, text=True))
            doc = json.loads(first)
            self.assertEqual(doc['summary']['source_files'], 1)
            self.assertEqual(doc['summary']['registrations'], 1)
            self.assertTrue(all(x['matches_registration'] for x in doc['address_occurrences']))
            self.assertEqual([x['category'] for x in doc['address_occurrences']], ['registration_argument', 'matching_registered_literal'])
            self.assertEqual(cpp.read_text(), original)
            self.assertIn('not resolved', doc['method'])


if __name__ == '__main__':
    unittest.main()
