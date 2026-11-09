# Community 1.2.6 compatibility baseline

The port's reference behavior is the cumulative community 1.2.6 engine. The content library is an added feature; it does
not replace patch behavior. The 1.2.4 notes list high-poly track and car support, broader mirror visibility, the AI reset
fix, longer replays, and a larger texture-file allowance. 1.2.5 adds modern-GPU startup and sound fixes. 1.2.6 is based
on 1.2.5 and corrects the extra rear wing on the stock Viper and AI Vipers. See the [1.2.4 release notes](https://community.pcgamingwiki.com/files/file/943-viper-racing-unofficial-patch-124/)
and [community patch history](https://github.com/HerbFargus/viper-racing-modding/blob/main/docs/history/modding-history.md).

## Feature parity ledger

| Patch behavior | Current source evidence | Status and remaining proof |
| --- | --- | --- |
| 95,000-polygon tracks | No explicit polygon cap is present in the traced model-load path. Model index buffers are allocated from each model's triangle count; the model, surface-info, and deferred-draw pools are separately raised by M1. | **Source capacity is present; playability is unverified.** Race a legal 95,000-polygon track and verify its model/surface/deferred peaks stay within the lifted pools. |
| 20,000 vertices per model object, with a separate scene-wide budget | `mr_model_begin` allocates a reusable 32,768-vertex lit buffer, and `direct_model_build` uses the model's surface vertex ranges. `model_copy_transform` duplicates vertex, surface, triangle, and record arrays for each transformed copy. No aggregate scene-vertex counter is present in the traced source path. `MemAlloc_rw` logs a failed heap allocation but then fills through the null pointer, so allocation pressure remains fatal. | **Per-object capacity is present; aggregate memory behavior remains unverified.** Measure a high-detail car with a full AI field; the history documents crashes around 14 AI cars at 20,000 vertices each, so do not infer a safe scene budget from the per-object buffer. A safe guard needs a failure path through the game allocator and its model-copy callers, not just a guessed scene cap. |
| Rear spoiler model visible without a manual options edit (1.2.4) | The `<car>S.mod` eligibility branch is NOPed in v1.2.4 and v1.2.5, so the model loads whenever the resource exists. The v1.2.6 fix restores the branch; the port follows that latest-patch behavior. | **Latest-patch baseline implemented.** Check actual visibility with stock and modded cars and supported option settings. |
| Rear-view mirror in F1-F8 when enabled | `should_draw_mirror_rw` preserves v1.0's camera range and enables camera types 0-7 for patched builds. The `world_wld_world` harness calls the actual rewrite across camera indices -2 through 10, with the mirror option enabled and disabled under both retail and community policies; all cases passed. | **Source and harness behavior verified.** Confirm all eight modes in an actual patched-build race, including the disabled option. |
| Hacks-screen Vehicle list moved up and enlarged (community builds) | `HackOptionsControl::Added` reads the installed executable's four list-geometry immediates, so retail and community layouts remain distinct. The x86 `world_menu_options` fix-mode run passes 5,260 differential checks and 515 dedicated checks across stock 300×200, community 390×124/111×255, and custom geometries, including scrolling, selection visibility, and tab hiding. | **Implemented and harness-verified.** Confirm the tab on retail v1.0, official v1.1, and community v1.2.4–1.2.6; the community history records the 76-pixel upward move and 55-pixel height increase. |
| AI Reset option works | `AIResetCars` now refreshes every AI car's Proxer position and runs `Proxer::update`. The 1.2.4–1.2.6 binaries make the same call after the reset loop. | **Implemented and binary-traced.** The 2,000-world AI harness passes against a composed v1.0-plus-patch reference; exercise the actual menu action in game. |
| Replays can record up to one hour | Local v1.2.4 and v1.2.5 executables both allocate `0x2000000` bytes in their replay setup routine. `world_replay` runs the actual `PhysReplayDoneCreating` rewrite under both build policies and verifies the allocator receives 2 MiB for retail and 32 MiB for community, with matching frame-table counts. Its 10-world retail comparison also passed all 53 functions with no differences or footprint misses. | **Source and setup-harness behavior verified.** Confirm the patch's frame cap, record/load a long replay, and retain compatibility with retail-sized replay files in an actual race. |
| More simultaneous texture files | The hook relocates the 250-entry texture table to 1,000 entries and raises associated lookups. `world_gx_tex` passed 1,914 differential checks against the retail v1.0 engine using 35 real `.tex` resources (1,623,460 bytes): 1,007 worlds used the relocated table and 782 chained rewrite worlds, with no mismatches or footprint violations. The harness reported 86 randomized setup failures, which it excludes from comparisons. | **Source and harness behavior verified.** The harness simulates the lifted table over the retail engine; stress with a real 1.2.6 build and a high-texture-content race remains outstanding. |
| Modern GPU video-memory startup fix | `hook/gx_dx.cpp` skips the legacy 600 KiB addition only when it would wrap a 32-bit DirectDraw total; the OpenGL facade reports 64 MiB local and zero nonlocal memory. The x86 `world_gx_dx /DFIX_TESTS` run against the local retail image passed the six boundary/high-total inputs (24 wrapping cases and 23 nonwrapping cases executed; one nonwrap attempt lacked a DirectDraw object and was skipped). The `world_gx_dd` harness explicitly checks both OpenGL facade results; its updated protocol assertion allows the renderer's cache to suppress redundant driver calls while still rejecting unrecorded calls. Four rounds passed 2,360 checks and 21,370 OpenGL-call comparisons with no differences, protocol failures, or footprint misses. | **Arithmetic and bounded OpenGL report are harness-verified.** Hardware validation on a real >4 GiB GPU remains outstanding. |
| Scratchy sound fix | The port routes the game's software mixer through an SDL-backed DirectSound facade (`hook/dsound_sdl.cpp`). The asset-backed `world_snd_mix` harness compares the software mix and DirectSound-core operations against the original. | **Port audio bypasses the legacy DirectSound device path; audible parity is unverified.** The 100-episode harness passed 5,208 checks and 1.2 million leaf calls with zero differences across 35 real `.sfx` resources and 12 synthetic resources; `SoftSound::PlayLooped` was not reached. This does not measure audio emitted by a real SDL device. |
| 1.2.6 rear-wing correction | `CarObject_load_car_models` in `hook/wld_carobj.cpp` only loads `<car>S.mod` when the per-car eligibility flag is set and the resource exists. The 1.2.6 binary restores this guard around the lookup and `mrModelLoadRemap` call; 1.2.5 has two NOPs at the branch. | **Implemented in source; binary behavior traced.** Confirm with asset-backed player and AI Vipers at eligible and ineligible settings. |

## Binary observations

The replay rewrite selects 32 MiB only for identified community 1.2.4, 1.2.5, and 1.2.6 executables; retail v1.0 and
official v1.1 keep the 2 MiB cap. The same build predicate gates the mirror change so v1.1 keeps its older camera range.

The ignored local test copy contains v1.2.4 beta, v1.2.5 2016, and v1.2.6 2017 executables. Disassembly of the mapped
replay setup routine at `0x427110` shows `push 0x2000000` in both v1.2.4 and v1.2.5 before allocation, followed by a
frame count bounded by the build's data value. The implementation now follows that observed replay-buffer size for
the identified community builds. This is stronger evidence than the public one-hour note for the buffer allocation, but the
runtime duration and cap remain to be tested.

The 1.2.5-to-1.2.6 file comparison found 46 differing bytes. The meaningful code change is in car-model loading,
not the effects code: v1.2.6 changes the two NOPs at `0x4673d7` to `je 0x467400`, following a comparison of the
per-car eligibility byte at `[esp+0x9c]`. When that byte is set, it calls `ResourceExists` and then `mrModelLoadRemap` for
`<car>S.mod`; the resulting model is stored in the car object's `shadow_model` field. In v1.2.5, the comparison was
present but its conditional branch was NOPed out, so the resource lookup and load always ran. The retail v1.0 loader
has the same eligibility gate, and `hook/wld_carobj.cpp` preserves it as `if (high && ResourceExists(shadow))`.
This matches the reported 1.2.6 correction: cars whose loader eligibility flag is clear do not load the extra
`<car>S.mod` model.
The source and binary paths are now traced; asset-backed verification of the visible result remains outstanding.

The local 1.2.4 and 1.2.5 test installations contain the same launcher and `race.exe`; only `Data/race.bin` differs.
That image has 21 changed `.text` bytes, all inside `$E32(mr.obj)` and graphics/cache routines (`gxCanvasWrite`,
`clear_32`, `clear_32_32`, `paste_zoom_32_16`, and `tc_add`), five `.data` bytes at the build-version string, and 333,323
`.rsrc` bytes. The only changed resource payload is `SPLASH`. No sound code or sound-resource change appears in this
comparison. The community history attributes a scratchy-sound fix to 1.2.5, but this local binary evidence does not
identify a distinct implementation for it. The port's SDL-backed audio path avoids using the DirectSound device
backend; verify audible behavior with real playback before calling the sound issue closed.

The `world_snd_mix` run used 100 episodes and 100,000 iterations per leaf function. It compared 5,208 checks with no
differences or footprint escapes, and 1,200,000 leaf calls with no differences. One function, `SoftSound::PlayLooped`,
was not reached. The harness proves mixer/core output parity for the cases it covers, not the absence of crackle from a
real audio device.

The `world_wld_carobj` differential harness passed against the retail v1.0 executable after this trace (1,725 generated
graf cases, 3,702 model builds, exit 0). It exercises the rewritten car-model loader against the retail implementation,
which has the same detail guard. It does not prove the community build's visible result with player and AI Viper assets.

The AI Reset path is a second 1.2.4 change. The mapped community `AIResetCars` (`0x41b120`) first refreshes each
registered car's position, then loads the Proxer global from `0x4d6804` and calls `Proxer::update` at `0x41ee90`.
Retail v1.0's `AIResetCars` returns after refreshing positions, so the port now reproduces the community sequence by
calling its existing `Proxer_update_o` after the loop. `world_ai` passes 2,000 worlds and 341,015 calls with no
difference or footprint escape against that composed patch reference. The menu action itself remains to be exercised
in a live race.

The isolated v1.2.6 launcher smoke on 8 November 2026 showed the library prompt in Quick Race setup. Selecting
`exotic.car` and `bemidji.trk` was logged as loading those exact files from the test copy's `Data` directory. The game
entered the live race view and logged its renderer shutdown after 23,872 frames with no fault or panic entry. This
validates the content-library path on v1.2.6; it does not exercise the patch-specific limits, mirror cameras, long
replays, or the rear-wing correction.

## Compatibility rule

Keep the retail v1.0 behavior where it is the reference, and select patch-specific behavior by identified executable
build. Keep each patch feature in the parity ledger until a source-level implementation and an asset-backed behavior
check cover it. Address-table fingerprints establish byte matching only; they do not establish feature parity. The
browser and exact archive binding must be verified on top of this same 1.2.6 baseline.
