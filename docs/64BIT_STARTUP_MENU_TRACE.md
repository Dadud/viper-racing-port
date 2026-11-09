# Startup-to-single-race source trace

This is the first connected migration slice traced from the current hook source. It follows startup through the
single-player setup menu into resource loading, with the local content selection bound to the exact archive paths.
The community v1.2.6 build is the primary compatibility target; the other supported x86 builds use address maps and
adapters around the same catalog and selection interfaces.

## Current source path

1. **Proxy load and build selection** — Windows loads `dinput.dll` beside the game. `DllMain` calls `install()`,
   which identifies the host image by PE metadata, resolves build-specific addresses, and declines to patch an unknown
   or incompletely mapped executable. `port_install()` applies the verified replacements/adapters. See
   `hook/viperport.cpp` and `hook/port.cpp`.
2. **Main menu dispatch** — `race_c()` calls the game menu and dispatches choice 0 to `GameDoSingle`. The hook
   registration for `race_c` is at guest address `0x00401d70`; `GameDoSingle` is at `0x0040a680` in the v1.0
   address space, translated for each supported executable by the port maps.
3. **Single-race setup** — `GameDoSingle` creates the original-layout `World` (asserted to be `0xcd4` bytes) and
   invokes `GameBeginSingle`. On the latest community build, the adapter calls the original game setup first, then
   offers the library. The browser model owns copied row data and tracks selection by stable ID plus archive path.
   Confirmation stages a revalidated car/track selection; canceling clears the temporary selection.
4. **Race loading** — the game continues to `LoadRace`. Its resource-set requests pass through the loader adapter.
   When a requested set matches the selected car or track name, the adapter revalidates the archive content ID and
   opens that exact path. A package change, open failure, or parse/load failure records an error and fails closed.
   If the exact selected archive is already loaded below a different same-name set, the adapter opens it again so the
   selected package becomes the newest resource set; the engine searches newest first. Unrelated resource requests
   continue through the community build's original loader and existing shared/stock search order.
5. **Lifetime end** — selection state is cleared on setup cancellation, failed loading, successful load after the
   packages are resident, and single-race unload. The selection is process-local and is not written to saves, replays,
   or network state.

Relevant implementations: `hook/content_catalog.{h,cpp}`, `hook/content_game.{h,cpp}`, `hook/menu_race.cpp`,
`hook/wld_cars.cpp`, and `hook/krn_res.cpp`.

## What carries into a native 64-bit game

Reusable parts are the platform-neutral catalog, archive inspection, stable IDs, favorites, browser model, and owned
selection snapshot. The current UI menu adapter and resource-loader adapter are executable-specific integration code.
They rely on the legacy dialog ABI, 32-bit guest addresses, fixed globals, a stack-shaped `World`, and the engine's
resource-set structures. A native build must replace those with host-owned menu/input state, typed native interfaces,
and a native race/resource lifecycle. Reusing the catalog does not make the current game hooks or data layouts safe
for x64.

The new `native/viper_native.cpp` is the first host-owned x64 startup/menu application. It has a real SDL2 event/render
loop and uses the shared catalog to browse, search, favorite, validate, and retain an exact car/track pair as a native
`RaceSelection`. Shared `.res` discovery is recursive, case-insensitive, deterministic, and reports root/iteration
errors; malformed archives remain available to validation as indeterminate resources. The Windows x64 target builds
through `native/build_windows_x64.bat`, and the host-owned responsive layout has x86/x64 geometry tests. On validated
selection, `native_resource::Store` opens the exact car and track archives with `content::ArchiveReader`; bounded
resource reads use the selected package first and then the catalog's ordered shared archives. The model harness checks
selected-package precedence, shared fallback, missing-name behavior, and release on cancellation. This is still a
front-end and resource-service migration slice, not a native game: it does not load `race.exe`/`race.bin`, call the
i386 engine, or start a race. Mouse clicks
select menu items and packages, and a double-click confirms a package; text entry is still keyboard-driven. Linux
build/runtime and Windows interactive behavior remain to be verified.

The SDL car details view opens `<car>0.mod` by selected stable ID, parses its MINF arrays, and renders the mesh. The
track details view opens `track.bpp`, parses its BPPT triangles and checked 2D node indices, and renders a
surface-colored overhead map. The host-owned terrain module includes a point-height query over that splitter tree;
it also includes a segment-versus-triangle query that follows the observed BPP traversal order, but neither the full
collision API nor x87/i386 parity has been established. Synthetic hit/miss and tree-safety cases pass on
x86/x64, and all eight installed tracks parse from each local v1.0/1.2.4/1.2.5/1.2.6 data tree. These previews read
the selected package directly; they do not yet exercise the active race `native_resource::Store` snapshot.

## Evidence and remaining edge

The source call sites were inspected and focused Clang AST dumps were generated for the setup adapter and selected
resource loader under `out/clang-audit/`. `tools/clang_ast_dependencies.py` now uses the real compile database to emit
bounded, machine-readable AST slices for `race_c`, `GameDoSingle`, `GameBeginSingle_content_adapter`,
`MenuDoRaceSetup_content_adapter`, and `load_selected_resource_set`. The report separates named direct calls, function
pointer calls, member calls whose virtual status is not established, and the `ccall` address wrappers with their
`F_*` targets. The observed path includes main-menu dispatch to `GameDoSingle`, the `World_ctor` / `PreRaceDo` /
`DoRace` function-pointer calls, selection staging in the setup adapter, and selected-archive lookup in the resource
loader. It leaves the `A(address)` build-map calls, unresolved expression callees, virtual/member dispatch, and the
callee graph behind those wrappers explicit; this is a bounded AST slice, not a complete graph or an x64 parse. Re-run
it after regenerating the compilation database, for example:

```powershell
python tools/clang_ast_dependencies.py `
  --database out/clang-audit/compile_commands.json `
  --clang out/llvm-toolchain/unpacked/clang+llvm-23.1.5-x86_64-pc-windows-msvc/bin/clang-cl.exe `
  --source root_main.cpp --function '::root_main::race_c' `
  --report out/clang-audit/ast-native-startup-menu.json
```

The synthetic `world_krn_res` selected-package case loads two different
archives with the same set name, verifies that the selected exact path is promoted above the shadowing set, and confirms
a subsequent request reuses that top node. It also removes the selected path while a same-name game-folder archive is
available, then verifies that the load fails, preserves the diagnostic, and does not fall through to that other file.
The regular v1.0 resource harness passes 108,108 comparisons with the local game data: all 4,164 entries in 52 real
sets were found and verified. The 128/128 Clang result is parser coverage for the existing i686 hook
translation units only, as detailed in [Clang parser coverage](CLANG_PARSER_COVERAGE.md). It does not establish x64
compilation or native runtime behavior.

The next native step should trace and implement a host-owned startup-to-menu slice that can run without mapping the
original PE image. Keep physics behind its separate x87/ABI parity gate. Asset-backed races and the hook-based local
mod browser remain useful x86 acceptance evidence, but neither is a native 64-bit milestone.
