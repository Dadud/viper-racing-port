# Clang parser coverage

The Windows hook build now has a compilation database generated from the source list and compile flags in
`hook/build.bat`. The database is written to ignored `out/clang-audit/compile_commands.json`; it describes 128 MSVC x86
translation units. Generate it with:

```powershell
powershell -ExecutionPolicy Bypass -File tools/compile_commands_windows.ps1
```

`tools/clang_parse_coverage.py` runs Clang's parser against every database entry and writes its machine-readable report
to ignored `out/clang-audit/clang-parser-coverage.json`:

```powershell
python tools/clang_parse_coverage.py `
  --database out/clang-audit/compile_commands.json `
  --clang out/llvm-toolchain/unpacked/clang+llvm-23.1.5-x86_64-pc-windows-msvc/bin/clang-cl.exe `
  --report out/clang-audit/clang-parser-coverage.json `
  --jobs 2
```

The recorded run used Clang 23.1.5 from the [official LLVM release](https://github.com/llvm/llvm-project/releases/tag/llvmorg-23.1.5)
and parsed **128 of 128 translation units (100%)**. This is syntax-parser coverage for the existing i686 Windows hook
sources. It is not a successful Clang code-generation build, an x64 parse, or evidence that the original engine has been
ported. The parser runner drops code-generation/link flags from the MSVC commands and adds `_M_IX86_FP=2` and
`__PRFCHWINTRIN_H` solely to make the MSVC STL atomic declarations and SDL's Clang prefetch workaround parse under
Clang. Those defines must not be copied into a shipping build without a separate ABI and behavior review.

AST dumps were also generated for the community race-setup adapter `MenuDoRaceSetup_content_adapter` and the exact
archive binding function `load_selected_resource_set`. They are in ignored `out/clang-audit/ast-menu-race-adapter.txt`
and `out/clang-audit/ast-resource-binding.txt`. This establishes usable AST input for the next mapping task; the full
startup-to-menu dependency graph, ownership boundaries, and unresolved dispatch edges still need to be traced.
