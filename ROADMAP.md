# Viper Racing Port modernization

The original Viper Racing executable remains the playable product. New player-facing controls belong in its existing menus and
should retain the original visual style. Faithful rendering and simulation behavior are the defaults.

## Direction

1. Preserve the retail v1.0 behavior as the stock reference while matching community patch 1.2.6 where it contains a fix.
2. Make every installed car and track selectable in the existing race setup flow. Use archive identity, not display names, to
   bind the selection to the resource loader. Keep the original limit of eight race participants.
3. Make peer-hosted multiplayer reliable across supported Windows and Linux builds. Require explicit game, patch, and exact
   content matching before a client enters a race.
4. Add a small self-hostable rendezvous service and in-game browser/invites after direct host/join compatibility is sound.
   Keep direct IP as a fallback. Offer missing packages from a catalog and then the host; show package details and request
   confirmation before installing into the managed content library. Verify archive structure, hash, and dependencies.
5. Consider optional filtering, reflections, and shadows after the compatibility and multiplayer work. Each effect must be
   independently disableable and must not affect simulation.

## Work status

### Implemented on `codex/original-game-modernization`

- The prior development line, including its pending source edits, is preserved on
  `archive/progress-through-2026-11-09`.
- This branch starts from HerbFargus upstream `main` at `b72d263`.
- A managed content catalog indexes installed `.car`, `.trk`, and `.tra` packages by SHA-256 identity, with metadata,
  dependency checks, favorites, and a managed library installer.
- Quick Race car and track choices are integrated into the game's existing setup menu. The adapter resolves a selected
  package by exact archive identity and fails closed if the archive changes or disappears.
- Community 1.2.4–1.2.6 executable address maps and patch-specific behavior are tracked in
  [the patch parity ledger](docs/COMMUNITY_PATCH_PARITY.md).

### In progress

- Exercise the native selectors with more than eight packages, duplicate and long names, and exact-package race loading in
  live retail and community builds.
- Complete the patch parity ledger with asset-backed and live-race evidence on the supported builds.
- Add a versioned multiplayer session manifest and enforce game, patch, and exact car/track package matching before race
  setup proceeds.
- Improve host/join failure reporting and reliability before introducing rendezvous discovery or invitations.
- Build and test a self-hostable rendezvous service, in-game browser, package catalog flow, host transfer, and safe managed
  installation.

## Acceptance record

The source and harness evidence currently available is recorded in the parity ledger. The content catalog has x86/x64 unit
coverage and exact archive precedence coverage; the live menu acceptance cases and cross-platform multiplayer matrix remain
open. Do not mark a feature complete from address-table fingerprints alone. Record the game build, installed packages,
reproduction steps, and observed result for each live-game check.
