# ArcMass Plugin

ArcMass is the project's Mass layer: entity config assets and traits, the spatial hash, navigation invokers, influence mapping, lifecycle, and visualization. It owns storage and simulation state; it does not own AI decisions (ArcAI, ArcTactics) or abilities (ArcMassAbilities).

## Local contracts

### Influence mapping — two storage kinds behind one API

`UArcInfluenceMappingSubsystem` exposes one index space over `UArcInfluenceSettings::Grids`; config index **is** array index. `FArcInfluenceGridConfig::Storage` selects the backing per grid and the subsystem dispatches on it. Consumers use only `QueryInfluence` / `QueryInfluenceInRadius` / `AddInfluence` and never reach into a grid's internals.

- **`Sparse`** — `TMap<FIntVector, FArcInfluenceCell>` of float channels with per-source provenance, decay, and a `MaxCellsPerUpdate` tick budget. For rarely-updated, wide-area data. Written by `UArcInfluenceMappingProcessor` (from `FArcInfluenceSourceFragment`), by `AArcInfluenceVolume`, and by `UArcMassDecayingTagProcessor`.
- **`Tiled`** — fixed `TileDim × TileDim` tiles, ref-counted by invoker coverage, each holding an occluder bitmap and a sparse set of **per-team visibility bitplanes**. Carries **no float channels**: visibility is a bit, so `NumChannels`, `DecayRate` and `DecayInterval` do not apply. `AddInfluence` on a tiled grid is rejected — there is nothing to add to. Volumes and the generic emitter skip tiled grids.

`GetGrid` returns null for a tiled index and `GetTiledGrid` returns null for a sparse one; both are the only way to reach a backing directly, and both are ArcMass-internal by convention.

### Tile lifetime

`UArcInfluenceInvokerProcessor` is the sole owner of tile existence: every `RefCount` change and every `AllocateTile`/`ReleaseTile` call. Tiles are shared by overlapping invokers, acquired at `Radius` and released at `Radius + ReleaseMargin` — that asymmetry is the hysteresis, there is no separate timer. `UArcInfluenceInvokerRemovalObserver` releases an invoker's tiles when its fragment goes away; without it a destroyed invoker's refs leak and its tiles never free.

At the `MaxTiles` cap, allocation is refused and the invoker stays pending so it retries when capacity frees. A missing tile reads as empty everywhere, so refusal degrades to ordinary behaviour rather than to wrong data.

### Absence means transparent

A missing tile, an unrasterized occluder bitmap, and a cell no cast reached all read the same way: nothing there. Consumers must degrade toward their default behaviour, never toward a fabricated obstacle. This is why an unbuilt navmesh boundary is not an occluder and why a tile born mid-rebuild-cycle stays unpublished until it has been through a whole one.

### Tile geometry is a shared lattice

Tile and cell coordinates derive from world origin and `CellSize` alone, with no private offset, so a future flow field can adopt the same lattice without a remap. Keep it that way. `FMath::DivideAndRoundDown` truncates rather than flooring, so negative world coordinates need a real floor-division — see `ArcInfluenceTiledPrivate::FloorDiv`.

### Who writes the visibility bitplanes

Nothing in ArcMass. ArcMass owns the storage; **ArcAI** owns both producers — `UArcNavOccluderRasterizerProcessor` fills the occluder bitmap from navmesh boundary edges, and `UArcVisibilityChannelProcessor` shadowcasts perceiver cones into the team bitplanes. ArcAI depends on ArcMass and not the reverse, so ArcMass exposes `OccluderDirtyTiles` as a queue for ArcAI to drain rather than calling out to it.

Diagnostics: `arc.mass.DebugDrawInfluence` draws both kinds from the subsystem's own tick.
