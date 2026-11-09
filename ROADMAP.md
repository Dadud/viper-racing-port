# Project roadmap

## Goal

Deliver one native 64-bit Viper Racing executable for Windows and Linux, with the original driving behavior, a unified library of stock and community cars and tracks, and the game interface for browsing and racing them. Keep the current i386 engine working as the compatibility release and behavioral reference until the native build passes the same acceptance gates. Do not call a portable helper, a 64-bit host test, or an `-m64` compile a native game milestone.

The order below follows technical dependencies. Portable content and native-core work can proceed while the i386 baseline remains available; no user-facing feature is complete until it is connected to the game path that it targets.

## Ordered milestones

### 0. Keep a trustworthy reference build

- Re-establish Linux i386 and Windows x86 builds, headless tests, and a native i386 test host. Keep the current build flags and physics/replay checks recorded.
- With legally obtained game files, capture baseline startup, menu/input behavior, race scenarios, saves, replays, and multiplayer sessions. Store reproducible inputs and semantic state traces; do not commit proprietary assets.
- Keep host-native portable tests separate from i386 engine tests. Run QEMU only as a portability smoke test, not as the oracle for physics or exceptions.
- Record the current cloud gaps: it has lacked the i386 C++ headers and SDL2 development config, and it has no original game assets. Close these gaps in a repeatable CI or developer environment before using it for parity claims.

**Exit:** clean i386 builds and repeatable reference captures exist for both supported platforms, with the required asset handling documented.

### 1. Map the real 64-bit conversion work

- Extend the current lexical inventory into a dependency inventory for fixed-address calls and data, `ccall`/`tcall`/`vcall`, function-pointer erasure, guest pointers, vtables, inline assembly, x86 exception state, and x87-dependent functions.
- Generate and use compilation databases from the actual build flags. Trial Clang AST analysis on representative files; report parsing coverage and leave unsupported macros or assembly explicitly unresolved.
- Classify dependencies by subsystem and trace a complete startup-to-input/menu slice before choosing a larger migration boundary. Never infer that matching address literals are call edges or that a registration count equals remaining functions.
- Preserve separate guest types for 32-bit addresses, PE32 layouts, save/network fields, and native host pointers. Audit Windows LLP64 and Linux LP64 separately.

**Exit:** the first migration slice has named entry points, direct and indirect dependencies, state ownership, ABI signatures, unresolved calls, and tests. Inventory limitations are visible in generated output.

### 2. Establish native host and guest boundaries

- Add a native x86-64 build target without changing the i386 target. Make the PE32 file readable as data only; native code must not jump into its instructions or cast its addresses to host function pointers.
- Replace untyped fixed-address dispatch with typed native service interfaces by subsystem. Keep fixed addresses inside i386 adapters only.
- Use checked guest-memory views and explicit serializers for guest addresses, 32-bit object layouts, saves, and network packets. Add stable identities or handles where native pointers cannot be serialized.
- Make platform services, allocation ownership, time, paths, input, logging, and failure reporting available through native implementations on Windows and Linux.
- Use the existing portable pilots (address view, heap handles, file/time layers, CRC/Base64, and host-owned UI string list) as tested building blocks. Connect each to a real native caller; do not count the isolated test as an engine migration.

**Exit:** the native target builds a minimal executable and exercises its service layer on both operating systems. No 32-bit guest pointer is silently widened or used as a host pointer; unresolved legacy calls fail explicitly.

### 3. Reach a real native startup-to-menu path

- Replace the x86 loader and hook startup assumptions with a native entry point, configuration path, error path, window/event loop, display, keyboard/mouse/controller input, and renderer initialization.
- Keep rendering and input behind typed interfaces so existing UI code can migrate in dependency order. Preserve original coordinate and timing behavior until their replacements have parity tests.
- Build host-owned UI state separately from the 0x14-byte legacy `UIStringList` and other ABI-locked structs. Adapt ListBox and DropList to read an explicit list interface, then migrate one menu from creation through input, update, drawing, and teardown.
- Use synthetic UI and resource fixtures until authorized game assets are available.

**Exit:** the native executable reaches and exits a real interactive menu on Windows and Linux, with keyboard, mouse, and controller navigation. A test drives the same menu/model path; no mock screen disconnected from the game loop qualifies.

### 4. Finish the content library and safe legacy import

- Evolve `content_registry` into the engine-owned index for unlimited logical car and track entries, stable IDs, manifests, author/version/description metadata, hashes, favorites, dependencies, search, and validation results.
- Verify each legacy format and parser against source or known fixtures before supporting `.car`, `.trk`, `.tra`, `.mod`, `.tex`, `.cf`, `.stp`, `.ili`, `.ild`, or ZIP/RAR variants. Reuse toolkit code only where its format behavior and license are clear.
- Build a non-destructive importer: inspect archives in place, reject unsafe paths and malformed resources, report duplicates, missing dependencies, filename collisions, and incompatibility, and never replace stock files. Do not require Python for players.
- Add synthetic fixtures for parsing, dependency resolution, duplicate handling, and errors. Keep archives intact and distinguish structural validation from “loads successfully in a race.”
- Connect registry discovery and preferences to both the maintained i386 game and the native game path through platform-neutral APIs.

**Exit:** the in-game library discovers and validates original and community content without overwriting it; IDs and metadata persist across launches, and a player can search and favorite entries.

### 5. Remove content-slot assumptions and load selected resources

- Trace each car/track selection value through menus, resource loading, race initialization, saves, replays, and multiplayer. Keep race participant limits separate from total installed content.
- Add namespace-aware resource resolution and caches. Resolve each selected car or track from its own package plus explicitly declared shared dependencies; define case handling and precedence.
- Carry stable content IDs through selection and race setup. Add compatibility translation for legacy names and slot references where old saves or network peers require them.
- Let the player select any valid installed car and track without renaming or replacing original files. Handle missing or incompatible resources before race startup.

**Exit:** automated fixtures and legal-asset playthroughs initialize races with arbitrary installed cars and tracks on both engines, with no slot cap beyond documented per-race participants.

### 6. Port the simulation and prove driving parity

- Migrate physics, AI, collision, vehicle loading, and race lifecycle by dependency group. Preserve the i386 implementation as the comparator and keep physics changes isolated from UI/content changes.
- Diagnose x87 precision, transcendental instructions, exception state, rounding, denormals, NaNs, compiler behavior, and Windows x64 ABI differences. Compare native i386 and x86-64 corpora first, then real per-tick race traces.
- Compare semantic state, inputs, events, AI choices, and replay outcomes rather than raw pointer-bearing object bytes. Record first divergence and tolerances where bit identity is not attainable.
- Do not ship native racing until stock-car, modded-car, AI, collision, replay, and representative long-session cases pass. Resolve performance regressions before broad content rollout.

**Exit:** native races complete with stock and community content and meet agreed handling, AI, collision, replay, determinism, and performance gates on Windows and Linux.

### 7. Connect the modern garage and track browser

- Build the functional garage and track browser inside the native game using the content registry. Add search, filters, sorting, favorites, author/source, original/modded status, and direct race setup before visual polish.
- Generate and cache thumbnails and overhead track maps from verified model and track geometry. Report length, direction, surface, and AI compatibility only when those values can be derived reliably.
- Add 3D previews using the engine renderer and controller-friendly navigation. Reuse existing UI/rendering code only after the native boundary is in place.
- Add an in-race minimap after track maps and race coordinates are validated.

**Exit:** both browsers enumerate all installed content, show cached previews/maps, and launch the selected race through the dynamic loader.

### 8. Port saves, replays, and multiplayer; add server browsing

- Make saves, replays, and session protocols use explicit-width fields and stable content identities. Maintain a compatibility path for legacy data and clearly report mismatched content.
- Port client/server networking only after the race simulation and content identity path are stable. Test reconnects, incompatible mods, version mismatch, latency, and peer failure.
- Implement in-game LAN discovery and a public browser for player-hosted sessions. Use a small rendezvous/catalog service for public discovery and NAT traversal where needed; peers host the race simulation. Treat optional relay and dedicated servers as fallback modes, because Internet reachability cannot be guaranteed by a serverless design alone.
- Keep the existing multiplayer experience available until native sessions pass cross-version and failure testing.

**Exit:** players can discover and join compatible LAN and Internet sessions from the game, with content/version checks and clear fallback behavior.

### 9. Add workshop and community discovery

- Start with local workshop workflows for inspecting, validating, editing metadata, previewing, packaging, and uninstalling content. Reuse `vrmod` tools and formats after auditing their behavior and licenses.
- Add an in-game community catalog adapter for search, attribution, previews, versions, dependencies, compatibility, and supported downloads. Require permission to redistribute each item and preserve original archives.
- Add advanced vehicle physics, model, texture, track geometry, AI line, and collision editors only after importing and racing existing community content works reliably.

**Exit:** users can find, install, validate, use, update, and remove supported mods from the game, with attribution and dependency checks.

### 10. Modernize and ship

- Finish widescreen/high-resolution layout, UI scaling, refresh-rate behavior, configurable controls, wheels and force feedback, graphics settings, texture filtering, caching, crash reporting, and performance work.
- Run clean Windows and Linux release builds, migration and compatibility suites, asset-free tests, real-asset race/replay tests, multiplayer tests, and security checks for archive handling.
- Keep the i386 compatibility release until the native x86-64 build passes the full acceptance suite. Document supported mod formats, platform limits, and migration of old saves and multiplayer sessions.

**Exit:** one native 64-bit game executable and interface serve as the primary experience on Windows and Linux; the i386 build remains available as a compatibility release until its retirement is deliberate and documented.

## Current position

The ABI-boundary and portable-core groundwork is in place. The multi-root content catalog is connected to the 32-bit single-race setup and exact-path resource loading; its portable tests pass on x86 and x64, and the Windows x86 DLL builds. Community v1.2.6 is the primary compatibility target and latest-patch baseline: a library-selected car and track loaded into a live race by exact archive path, the search and favorite filters worked, the favorite persisted, and skipping the browser preserved stock resource loading. v1.0 also loaded selected community content into a live race. The latest-patch baseline includes the cumulative 1.2.4–1.2.6 engine fixes, not just the library; their source/runtime parity and evidence are tracked in [community patch parity](docs/COMMUNITY_PATCH_PARITY.md). Replay capacity now follows the observed 32 MiB community buffer while retaining the 2 MiB v1.0/v1.1 behavior, and the community mirror camera range is implemented; both still need runtime verification. A v1.2.6 launcher smoke entered a race with `exotic.car` and `bemidji.trk` selected by exact archive path. The 32-bit UI now scales legacy full-screen dialog fonts and stamps, and the content browser switches to a list/details layout on larger screens; its responsive geometry passes the x86 and x64 catalog suites. In-game visual review remains open. Runtime validation of v1.1 and community v1.2.4/.5 remains outstanding; the checked-in community address-table byte fingerprints now match all three local v1.2.4, v1.2.5, and v1.2.6 executable images. There is no native 64-bit game engine. The host-owned SDL x64 startup/content-menu prototype opens selected car and track archives by stable ID and exposes bounded package-first/shared-fallback resource reads; its car selector now loads and draws the selected package MINF mesh. The track selector decodes the selected BPPT ground mesh, displays a surface-colored top-down map, and exposes checked height and segment-intersection queries over its splitter tree; the segment query now matches the original v1.0 routine bit-for-bit across x86 and x64 builds for 38,434 finite queries on eight stock tracks in both 24-bit physics and 53-bit UI precision modes; complete collision-system validation remains open. The native model/resource and SDL event-loop harnesses pass, but the app still does not launch races and has no Linux runtime evidence; WSL could not start because virtualization is disabled on this machine. A Windows MSVC x86 compile database now feeds Clang syntax parsing for all 128 hook translation units. A bounded AST extractor records direct calls, legacy address wrappers, function-pointer calls, and unresolved member dispatch across five startup, setup, and resource-loader slices; it keeps remaining edges explicit instead of treating parser coverage as a full call graph. The source/ABI trace and native/README.md document the current host-owned menu slice; the next port step is growing these readers into the native race lifecycle: texture loading, world and obstacle parsing, full collision APIs, and simulation parity still lie ahead. See [64-bit migration research](docs/64BIT_RESEARCH.md), [Clang parser coverage](docs/CLANG_PARSER_COVERAGE.md), [community map validation](docs/COMMUNITY_BUILD_MAP_VALIDATION.md), and [architecture notes](ARCHITECTURE.md) for evidence and remaining gaps.
