# Roadmap

1. Exercise the inventory against legally obtained stock and community archives, compare its findings with vrmod, and harden limits and error reporting. Add metadata and dependency analysis, and connect persisted favorites to the game. SHA-256 identity, byte-confirmed duplicate detection and a favorites persistence API are implemented.
2. Design a resource namespace and safe non-destructive install flow. Account for shared resources and case-insensitive lookups. Support package inspection without trusting archive paths or extracting over original files.
3. Connect content identities to car and track enumeration, resource loading, race setup, saves, replays and multiplayer. Preserve existing behavior and validate with the original game's replay and session corpus before enabling it.
4. Build integrated car and track browsers and cached previews from verified resource formats. Then evaluate gallery integration and workshop features.
5. Test Windows and Linux builds and multiplayer with original assets; keep physics changes behind dedicated regression checks.
