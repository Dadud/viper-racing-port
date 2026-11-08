"""Asset-free tests of Linux compilation database generation.

Run: python3 test/test_compile_database.py
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


class CompileDatabaseTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="vp-compdb-")
        self.root = Path(self.tmp.name)
        for directory in ("tools", "hook/linux_inc", "loader", "bin"):
            (self.root / directory).mkdir(parents=True)
        source_root = Path(__file__).resolve().parents[1]
        shutil.copy2(source_root / "tools/build_linux.sh", self.root / "tools/build_linux.sh")
        for name in ("viperport.cpp", "net_one.cpp", "net_two.cpp", "msvc_compat.h"):
            (self.root / "hook" / name).write_text("// fixture\n")
        (self.root / "loader/viperport_linux.cpp").write_text("// fixture\n")
        config = self.root / "bin/sdl2-config"
        config.write_text('#!/bin/sh\nprintf "%s\\n" "-I/fake/sdl -DTEST_SDL=1"\n')
        config.chmod(0o755)
        compiler = self.root / "bin/compiler"
        compiler.write_text(f'''#!{sys.executable}
import json, os, sys
with open(os.environ["VP_FAKE_LOG"], "a", encoding="utf-8") as f:
    f.write(json.dumps({{"directory": os.getcwd(), "arguments": sys.argv}}) + "\\n")
args = sys.argv[1:]
if "-o" in args:
    with open(args[args.index("-o") + 1], "w", encoding="utf-8") as f: f.write("fixture")
if "-MF" in args:
    with open(args[args.index("-MF") + 1], "w", encoding="utf-8") as f:
        f.write("fixture.o: " + args[args.index("-c") + 1] + "\\n")
''')
        compiler.chmod(0o755)
        self.log = self.root / "compiler.log"
        self.env = os.environ.copy()
        self.env.update(PATH=str(self.root / "bin") + os.pathsep + self.env.get("PATH", ""),
                        CXX=str(compiler), VP_JOBS="1", VP_FAKE_LOG=str(self.log))

    def tearDown(self):
        self.tmp.cleanup()

    def run_build(self, *args, **env):
        subprocess.run(["bash", str(self.root / "tools/build_linux.sh"), *args],
                       cwd=self.root, env={**self.env, **env}, check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    def test_database_matches_actual_compiler_invocations(self):
        self.run_build("compile-db")
        path = self.root / "out-linux/compile_commands.json"
        database = json.loads(path.read_text())
        self.assertFalse(self.log.exists(), "compile-db must not invoke a compiler")
        self.assertFalse((self.root / "out-linux/flags.txt").exists())
        self.assertEqual(len(database), 4)
        self.run_build(VP_COMPILE_COMMANDS="1")
        database = json.loads(path.read_text())
        actual = [json.loads(line) for line in self.log.read_text().splitlines()]
        for entry in database:
            matches = [call for call in actual if call["directory"] == entry["directory"]
                       and call["arguments"] == entry["arguments"]]
            self.assertEqual(len(matches), 1, entry["file"])
            self.assertIn("-m32", entry["arguments"])
            self.assertIn("-DTEST_SDL=1", entry["arguments"])
            self.assertTrue(Path(entry["file"]).is_file())

    def test_default_build_does_not_generate_database(self):
        self.run_build()
        self.assertFalse((self.root / "out-linux/compile_commands.json").exists())
        self.assertTrue(self.log.exists())


if __name__ == "__main__":
    unittest.main()
