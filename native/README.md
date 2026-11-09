# Native x64 startup and content menu

This is the first host-owned native x64 window and menu slice. It creates its own SDL2 window, event loop, and renderer.
The platform-neutral `NativeMenuModel` owns browser state and car/track selection; the SDL frontend translates input and
renders the same model exercised by its harness. It never maps the 32-bit `race.exe`/`race.bin` image and does not
call the i386 engine. The selected pair is revalidated by stable ID and exact archive path, then held as a native
`RaceSelection` snapshot. Race execution remains unavailable until the native simulation and resource consumers exist.

The native selection now opens the selected car and track through `content::ArchiveReader`. `native_resource::Store`
keeps those validated archive handles bound to the stable IDs, resolves each request from the chosen package before
the scanned shared `.res` order, and reads bounded payloads. This is a usable archive resource service for native
callers; the port does not yet have a simulation or a consumer that runs an actual race.

The car browser also reads the selected package's `<car>0.mod` by its indexed content ID and decodes it with the
host-owned MINF parser. The details card draws the mesh as a projected, color-coded wireframe and keeps the decoded
model cached while that ID remains selected. The track browser reads and decodes `track.bpp`, then draws a top-down
map colored by the ground surface code. Its host-owned loader also exposes an initial height query over the validated
2D splitter tree and a segment-versus-triangle query that follows the observed legacy traversal order. The query
reproduces the legacy x87 24-bit physics and 53-bit UI arithmetic explicitly, including 24-bit significand rounding
with the x87 extended exponent range. Across 40,000 generated queries on all eight stock tracks, both the x86 build
and a separate x64 build matched the original executable on all 38,434 finite inputs, with exact hit, triangle, and
point agreement in both precision modes. The remaining non-finite inputs are counted and excluded because the
portable API rejects them. These checks validate this query on stock BPP trees; they do not establish complete
collision behavior. These are resource consumers and terrain primitives, not a simulation; texture loading, model
animation, and native race execution remain future work. Preview reads are bounded and the renderer limits meshes to
250,000 triangles.

The menu reads the game directory in place. Pass it explicitly when it is not the executable's parent directory:

```text
viper-native.exe --game-dir "C:\Games\Viper Racing"
```

It scans loose car/track packages through the shared platform-neutral catalog and recursively inspects `.res`
archives in the game's `Data` directory to resolve observed dependencies. Invalid shared archives remain visible to
dependency validation, and directory scan errors appear in the menu status. It does not install, rename, or modify
game content.

## Build on Windows x64

Run `native/build_windows_x64.bat`. The script uses the configured Visual Studio x64 tools and the SDL2 x64 package
under `../../sdl2/SDL2-2.32.10`; it writes the executable and SDL runtime to ignored `out/native-x64/`.
Run `tools/test_native_sdl_smoke.ps1` for a hidden SDL runtime check that injects keyboard, mouse, text, and controller
events through the production event loop. It checks browser entry, favorites filtering, search input, back navigation,
rescan, and clean exit.

Capture repeatable UI review images from the production renderer with `--capture-page home|cars|tracks`, `--capture-output
<file.bmp>`, and optional `--capture-size 1920x1080` (the default capture size). For example:

```powershell
.\out\native-x64\viper-native.exe --game-dir "C:\Games\Viper Racing" --capture-page cars --capture-output .\cars.bmp
```

## Build on Linux x64

Install SDL2 2.0.18 or newer development files (`sdl2-config` or `pkg-config sdl2`), then run:

```sh
native/build_linux_x64.sh
```

## Controls

- Arrow keys move through menus; Enter confirms; Escape goes back.
- Type in the search field; Backspace removes the last character.
- `F` toggles a favorite, `V` filters favorites, and `R` rescans.
- Controller D-pad, A/B, X/Y, and right shoulder provide the same navigation and actions.

Mouse clicks select menu items and packages; double-click confirms a package. Native race execution is not implemented in this initial slice. A Windows x64 build proves
compilation and linkage only; launch and Linux runtime behavior still require separate verification.

Run `tools/test_content_catalog.ps1` to exercise the catalog, responsive layout, and native menu model on Windows x86 and x64.
Run `tools/test_native_terrain_parity.ps1` to compare the native segment query against the original v1.0 routine on
the installed stock tracks (the default reads `out/installed-viper-racing/Data`), then replay the same golden query
corpus against the x64 build. Use `-QueriesPerTrack`, `-Seed`, and `-CorpusDirectory` to change the deterministic run.
