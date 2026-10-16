// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcTerrainTypes.h"
#include "Render/ArcTerrainDensityTiers.h"
#include "ArcTerrainActor.generated.h"

class USceneComponent;
class UMaterialInterface;
class UTexture2D;

/**
 * Placeable terrain actor: owns the spatial FArcTerrainConfig and defines the terrain-local frame
 * (its actor transform) that stamps, cells and preview all resolve against. Not Blueprintable — the
 * terrain is data-driven via Config, not subclassing. The runtime subsystem (Step 4) registers this
 * actor on component registration and drives tile compositing.
 */
UCLASS(NotBlueprintable, placeable)
class ARCTERRAIN_API AArcTerrain : public AActor
{
	GENERATED_BODY()

public:
	AArcTerrain();

	/** Spatial layout of the terrain. ALWAYS editable — a TexelSize/CellExtent/ChannelSet edit fully
	 *  invalidates: the runtime cell set is flushed, and every tile file self-misses because the header
	 *  carries the raster it was written at. Every spatial-frame change is a deliberate,
	 *  fully-invalidating action. */
	UPROPERTY(EditAnywhere, Category = "Terrain")
	FArcTerrainConfig Config;

	/** Material the terrain renders with in editor/PIE. Author with ArcTerrainHeight (WPO) +
	 *  ArcTerrainLayerWeight nodes; detail-texture UVs should come from Absolute World Position
	 *  (TexCoord0 is 0..1 per cell). Cosmetic, not structural — does NOT live inside FArcTerrainConfig.
	 *  When unset, the subsystem falls back to the authored /ArcTerrain path load, then an engine
	 *  material. */
	UPROPERTY(EditAnywhere, Category = "Terrain", DisplayName = "Terrain Material")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	/** Side of the square the base mesh covers, in cm, centred on the terrain origin. It sizes the face
	 *  grid (NumFacesPerEdge = ceil(WorldExtentCm / FaceSizeCm)) and nothing else — the mesh is static and
	 *  global, so this is how far geometry exists, not how far terrain data streams.
	 *  Default 4e9 cm = 40 000 km. DOUBLE, not float or int32: 4e9 does not fit int32. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Mesh", meta = (ClampMin = "100000.0", Units = "cm"))
	double WorldExtentCm = 4.0e9;

	/** log2 of the texels along one face side (FaceQuads = 2^FaceQuadsShift). It picks the base mesh's
	 *  granularity and, with it, the subdivision depth a face needs (2 * FaceQuadsShift) to reach one leaf
	 *  per texel. Clamped to [18, 24]; at the default 22 and a 1 m texel a face is 4194.304 km. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Mesh", meta = (ClampMin = "18", ClampMax = "24"))
	int32 FaceQuadsShift = 22;

	/** Terrain-wide height authoring knobs. An edit re-composites and re-bakes every cell (routed
	 *  through the subsystem's OnTerrainHeightOptionsChanged). Independent of the spatial frame. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Height", DisplayName = "Height Options")
	FArcTerrainHeightOptions HeightOptions;

	// ---- Terrain|Residency ---------------------------------------------------
	//
	// The subsystem re-reads every knob below each Tick, so an edit applies next tick with no delegate.
	// The two Terrain|Collision raster knobs are the ONE exception (see PostEditChangeProperty): they are
	// folded into a built heightfield, which the patch's dirty-serial gate would otherwise never rebuild.

	/** Preview-residency ring radius (cm) around each invoker: cells inside get tiles + preview. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Residency", meta = (ClampMin = "0.0", DisplayName = "Preview Radius", Units = "cm"))
	float PreviewRadius = 200000.0f;

	/** Residency hysteresis (clamped to >= 1; 1 = single-threshold). Gates the LAST tier's residency
	 *  boundary — with HeightTiers empty that is this ring — and scales the coarse tiers' stream radius. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Residency", meta = (ClampMin = "1.0"))
	float PreviewRadiusHysteresis = 1.35f;

	/** Cap on resident SECTORS; beyond it the farthest-from-invoker sectors evict first. The sector
	 *  (1 << SectorShift cells per edge) is the residency unit, so admission is monotone in distance by
	 *  construction. It is a CAP, not a target: PreviewRadius decides how many sectors are wanted, and
	 *  this only has to stay above that or it silently clips the radius. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Residency", meta = (ClampMin = "1"))
	int32 MaxResidentSectors = 32;

	/** Cap on resident WEIGHT SLICES, one slice per weight group. INDEPENDENT of MaxResidentSectors: the
	 *  weight raster is ~4x denser linearly, so a weight slice costs as much as a whole SECTOR of height.
	 *  A cell without a weight slice shades with the base layer; its height is unaffected. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Residency", meta = (ClampMin = "1"))
	int32 MaxResidentWeightCells = 256;

	/** Ring radius (cm) for the WEIGHT rail, independent of PreviewRadius: weights stream on their own
	 *  distance, not the height ring's. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Residency", meta = (ClampMin = "0.0", DisplayName = "Weight Residency Radius", Units = "cm"))
	float WeightResidencyRadius = 200000.0f;

	/** Cells per sector edge = 1 << this. The tile atlas carries one PF_R32_FLOAT height slice per
	 *  sector, side (QuadsPerCell << this) + 3. Raising it coarsens residency granularity and wastes more
	 *  edge memory; lowering it multiplies the slice count. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Residency", meta = (ClampMin = "0", ClampMax = "4"))
	int32 SectorShift = 2;

	UPROPERTY(EditAnywhere, Category = "Terrain|Residency", DisplayName = "Height Tiers",
		meta = (TitleProperty = "RowSummary"))
	TArray<FArcTerrainHeightTier> HeightTiers;

	/** Cells per weight-atlas slice edge = 1 << this. DEFAULT 0 (slice = one cell) is a memory decision:
	 *  at the defaults a per-cell slice costs ~4.2 MB while a per-sector one costs ~67 MB, times
	 *  ceil(NumChannels / 4) arrays. Resolved to at most SectorShift — a weight slice spanning two
	 *  sectors would need a refcount the eviction path does not have. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Residency", meta = (ClampMin = "0", ClampMax = "4"))
	int32 WeightSliceShift = 0;

	UPROPERTY(EditAnywhere, Category = "Terrain|Residency", DisplayName = "Weight Tiers",
		meta = (TitleProperty = "RowSummary"))
	TArray<FArcTerrainWeightTier> WeightTiers;

	/** Collision-residency ring radius (cm) around each invoker (clamped to PreviewRadius). */
	UPROPERTY(EditAnywhere, Category = "Terrain|Residency", meta = (ClampMin = "0.0", DisplayName = "Collision Radius", Units = "cm"))
	float CollisionRadius = 10000.0f;

	/** Collision-ring hysteresis: a patch that already has a heightfield only tears it down beyond
	 *  CollisionRadius * this factor (clamped to >= 1; 1 = single-threshold). */
	UPROPERTY(EditAnywhere, Category = "Terrain|Residency", meta = (ClampMin = "1.0"))
	float CollisionRadiusHysteresis = 1.0f;

	/** Seconds a stamp edit pins its affected cells resident regardless of invoker distance. 0 disables.
	 *  DEFAULT 0: a pin exempts cells from the residency cap, so a pinned set larger than the atlas
	 *  starves camera-driven admission. The preview ring already covers a stamp the author looks at. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Residency", meta = (ClampMin = "0.0"))
	float EditPinSeconds = 0.0f;

	// ---- Terrain|Collision ---------------------------------------------------

	/** Edge of one collision patch — the collision lattice, INDEPENDENT of Config.CellExtent. A patch is
	 *  keyed, built, cached and torn down on this lattice alone, so collision needs no cell to exist
	 *  under it. Enumerated from the same extent list as the rest of the grid, which is what keeps a
	 *  patch a whole number of height texels. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Collision")
	EArcTerrainExtent CollisionPatchExtent = EArcTerrainExtent::M64;

	UPROPERTY(EditAnywhere, Category = "Terrain|Collision", meta = (ClampMin = "8"))
	int32 CollisionPatchQuads = 64;

	/** Edge of one collision patch in cm. */
	double CollisionPatchSize() const
	{
		return static_cast<double>(static_cast<uint32>(CollisionPatchExtent)) * 100.0;
	}

	// ---- Terrain|Lighting ----------------------------------------------------

	/** Decimation tolerance of a band as a fraction of that band's outer camera distance. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Lighting", meta = (ClampMin = "0.0"))
	float LightingDetailPerDistance = 0.002f;

	/** Depth the lighting mesh is lowered by, as a fraction of the band's outer camera distance. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Lighting", meta = (ClampMin = "0.0"))
	float LightingSinkPerDistance = 0.01f;

	/** Ring radius (cm) inside which a cell carries its own static lighting primitive (ray tracing +
	 *  Lumen), independent of every other ring. Keep it in step with r.RayTracing.Culling.Radius
	 *  (default 30000): the culling layer trims instances past that anyway. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Lighting", meta = (ClampMin = "0.0", Units = "cm"))
	float LightingRadius = 30000.0f;

	/** Lighting-ring hysteresis: a cell that already has a primitive is torn down only beyond
	 *  LightingRadius * this factor (clamped to >= 1). A rebuild recreates the proxy and its BLAS. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Lighting", meta = (ClampMin = "1.0"))
	float LightingRadiusHysteresis = 1.35f;

	/** Fraction of LightingRadius inside which a cell decimates at LightingNearToleranceCm. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Lighting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LightingNearBandFraction = 0.4f;

	/** Decimation error tolerance (cm) of the near band. It steers triangle density; it is NOT an error
	 *  bound (the error pyramid saturates by maximum, which bounds one point per node). */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Terrain|Lighting",
		meta = (ClampMin = "0.0", Units = "cm"))
	float LightingNearToleranceCm = 25.0f;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Terrain|Lighting",
		meta = (ClampMin = "0.0", Units = "cm"))
	float LightingMidToleranceCm = 200.0f;

	/** Skirt depth as a multiple of max(own tolerance, neighbour tolerance). */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Terrain|Lighting", meta = (ClampMin = "0.0"))
	float LightingSkirtToleranceMultiplier = 8.0f;

	/** Fraction of a cell's band-quantized camera distance by which its lighting mesh is lowered. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Terrain|Lighting", meta = (ClampMin = "0.0"))
	float LightingNearSinkPerDistance = 0.01f;

	/** Cells whose lighting mesh may be built per frame; the rest wait a tick, nearest first. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Lighting", meta = (ClampMin = "1"))
	int32 MaxLightingBuildsPerFrame = 4;

	/** Cells per edge of one far-occlusion block. The block is the ring's build and primitive unit. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Lighting", meta = (ClampMin = "1"))
	int32 LightingFarBlockCells = 4;

	/** Decimation error tolerance (cm) of the far occlusion ring. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Terrain|Lighting",
		meta = (ClampMin = "0.0", Units = "cm"))
	float LightingFarBlockToleranceCm = 300.0f;

	/** Fraction of a far block's band-quantized camera distance by which its occlusion mesh is lowered. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Terrain|Lighting", meta = (ClampMin = "0.0"))
	float LightingFarSinkPerDistance = 0.01f;

	/** Ring radius (cm) covered by far-occlusion blocks. Covers the near cells too — the far/near trace
	 *  handoff is dithered across a band, so a hole under the near ring would show as a gap. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Lighting", meta = (ClampMin = "0.0", Units = "cm"))
	float LightingFarRadius = 200000.0f;

	/** Far blocks whose mesh may be built per frame; the rest wait a tick, nearest first. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Lighting", meta = (ClampMin = "1"))
	int32 MaxFarBlockBuildsPerFrame = 1;

	/** Shared by every flat lighting cell; a BaseHeightCm change makes a NEW object and repoints them all
	 *  in one pass, never an in-place rewrite — the atlas tile returns to the pool only at refcount zero. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FlatLightingHeightTexture;

	float FlatLightingHeightCm = 0.0f;

	// ---- Terrain|Streaming ---------------------------------------------------

	/** Radius (cm), for the BASE tier only, around each invoker inside which a tile file is read from disk.
	 *  Each coarse HeightTiers entry streams to its own band instead. Governs only what is in
	 *  memory — PreviewRadius governs what the atlas holds and what draws — and no ceiling, budget or LRU
	 *  bounds the resident payloads. FLOORED at PreviewRadius * PreviewRadiusHysteresis: a sector whose
	 *  baked height tiles never arrive is never admitted, so a value below that floor has no effect. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Streaming", meta = (ClampMin = "0.0", Units = "cm"))
	float StreamRadius = 1000000.0f;

	/** Stream hysteresis: a loaded tile is released only beyond StreamRadius * this factor (clamped to
	 *  >= 1; 1 = single-threshold). */
	UPROPERTY(EditAnywhere, Category = "Terrain|Streaming", meta = (ClampMin = "1.0"))
	float StreamHysteresis = 1.35f;

	/** Radius (cm) for the WEIGHT rail's disk read, independent of StreamRadius: weights read from disk
	 *  on their own distance, not the height ladder's. FLOORED at WeightResidencyRadius *
	 *  PreviewRadiusHysteresis. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Streaming", meta = (ClampMin = "0.0", DisplayName = "Weight Stream Radius", Units = "cm"))
	float WeightStreamRadius = 300000.0f;

	// ---- Terrain|Render ------------------------------------------------------

	/** log2 of the bisector pool capacity. The pool costs 2^this * 44 bytes of records plus 2^(this-1)
	 *  bytes of CBT heap (21 => 2097152 bisectors, ~92 MB + 1 MB) of VRAM, shared by every face. A
	 *  change rebuilds the pool from empty on the next published base mesh. Running out is graceful:
	 *  the split is refused and ARC_CBT_COUNTER_ALLOC_REFUSED ticks. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Render", meta = (ClampMin = "8", ClampMax = "21", DisplayName = "CBT Pool Depth"))
	int32 CBTPoolDepth = 21;

	/** Draw only terrain triangles whose footprint is within DistanceCullRadius of the main-view camera.
	 *  This affects draw compaction only; the CBT topology remains live outside the radius. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Render")
	bool bEnableDistanceCulling = false;

	/** Physical draw-culling radius in centimetres along the transformed terrain plane. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Render",
		meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bEnableDistanceCulling"))
	double DistanceCullRadius = 10000000.0;

	/** Conservative terrain-local height interval containing every displacement this terrain can render.
	 *  The tessellation/draw culls and primitive bounds consume the same authored contract. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Render", meta = (Units = "cm"))
	float ConservativeMinHeightCm = -1000000.0f;

	UPROPERTY(EditAnywhere, Category = "Terrain|Render", meta = (Units = "cm"))
	float ConservativeMaxHeightCm = 1000000.0f;

	// ---- Terrain|Tessellation ------------------------------------------------

	/** Subdivision target in SQUARE PIXELS of the main view: split a bisector whose triangle covers more
	 *  than this much of the screen once projected. Foreshortening is part of the measurement, so ground
	 *  seen edge-on costs nothing and a face seen flat-on costs full price. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Tessellation", meta = (ClampMin = "1.0", DisplayName = "Target Triangle Pixels (px^2)"))
	float TargetTrianglePixels = 144.0f;

	/** Screen-space edge envelope on detected relief. Flat or unknown terrain receives the square root of
	 *  FlatAreaRelief on this limit, preserving its cheaper linear density while still probing for features. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Tessellation",
		meta = (ClampMin = "1.0", DisplayName = "Max Triangle Edge Pixels (px)"))
	float MaxTriangleEdgePixels = 24.0f;

	/** Hysteresis, as a FRACTION of the split threshold: a diamond merges when the parent triangle it would
	 *  collapse to measures below threshold * this. Applies independently to area, height error and max-edge.
	 *  At 1.0 the merged parent sits exactly on the split threshold and camera jitter oscillates it. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Tessellation", meta = (ClampMin = "0.05", ClampMax = "0.9"))
	float MergeThresholdFraction = 0.5f;

	/** Subdivision target for the CONTENT term: split a bisector when any edge midpoint or centroid differs
	 *  from the current triangle plane by more than this many viewport pixels (a LENGTH, not an area). The
	 *  term sits beside TargetTrianglePixels, so it can only add splits. The default is high enough to be inert. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Tessellation", meta = (ClampMin = "1.0"))
	float TargetErrorPixels = 1.0e6f;

	/** How many times TargetTrianglePixels a triangle may cover when the sampled surface matches its plane:
	 *  the one way the content term can take triangles AWAY rather than only add them. The relief is
	 *  withdrawn linearly as FlatReliefSagCmPerMeterSq is reached, and it is withdrawn entirely where the
	 *  sampled deviation reaches the threshold. Missing height data keeps the relief because the rendered
	 *  fill slice is flat and extra triangles cannot reconstruct an unloaded heightmap. 1.0 disables it. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Tessellation", meta = (ClampMin = "1.0", ClampMax = "4096.0"))
	float FlatAreaRelief = 64.0f;

	/** Where FlatAreaRelief runs out: sampled deviation in CENTIMETRES PER SQUARE METRE of the longest
	 *  triangle edge — a curvature, so the same surface reads the same at every subdivision level and a
	 *  parent is never held to a tighter area target than its own children. The default 0.1 is the reading
	 *  of a surface curving through a 94 m radius; anything tighter than that pays the full area target. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Tessellation", meta = (ClampMin = "0.0001", ClampMax = "1000.0", DisplayName = "Flat Relief Sag (cm/m^2)"))
	float FlatReliefSagCmPerMeterSq = 0.1f;

	/** How far past the screen edge a triangle is still DRAWN, as a FRACTION of the view's half-width and
	 *  half-height: 0.25 keeps everything inside a frustum a quarter wider and taller than the screen. It
	 *  buys nothing but safety margin against a projection the draw resolved a frame before the camera
	 *  moved, so it belongs well below TessFrustumCullMargin. 0 cuts exactly on the frustum. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Tessellation", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float FrustumCullMargin = 0.35f;

	/** The same widening, applied to the TESSELLATION VERDICT instead of the draw: a triangle wholly outside
	 *  it votes to merge before any other term is read, which is what keeps one finite pool from spending
	 *  itself on ground nobody is looking at. Held ABOVE FrustumCullMargin on purpose — the band between the
	 *  two is kept at full density and not drawn, so a camera turn finds refined ground instead of watching
	 *  it subdivide. At a 90-degree horizontal FOV the default widens the half-angle from 45 to 68 degrees,
	 *  which is ~23 degrees of turn on either side, and costs (1+m)^2 = 6.25 times the on-screen triangle
	 *  budget where the ground fills the widened frustum. 0 tessellates exactly what is on screen. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Tessellation", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float TessFrustumCullMargin = 1.5f;

	// ---- Terrain|Debug|Streaming -----------------------------------------------

	/** RUNTIME RESIDENCY of the HEIGHT tile rail, at that rail's own tile size, over a square key window
	 *  around each invoker: green resident, yellow in flight, red unusable, blue named by the manifest but
	 *  not brought in, near-black nothing baked there, magenta unknown, plus a crimson inner mark on the
	 *  outermost keys when the draw budget cut the window short. Empty ground away from the camera says
	 *  nothing about what is baked; the Terrain|Debug|Data layers answer that. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Debug|Streaming", DisplayName = "Draw Height Tile Residency")
	bool bDrawHeightTileResidency = false;

	/** RUNTIME RESIDENCY of the WEIGHT tile rail, at that rail's own tile size and in the same state
	 *  palette as Draw Height Tile Residency. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Debug|Streaming", DisplayName = "Draw Weight Tile Residency")
	bool bDrawWeightTileResidency = false;

	/** RUNTIME RESIDENCY of the DATA tile rail, at that rail's own tile size and in the same state palette
	 *  as Draw Height Tile Residency. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Debug|Streaming", DisplayName = "Draw Data Tile Residency")
	bool bDrawDataTileResidency = false;

	/** RUNTIME RESIDENCY: live cells, cyan resident and dark teal live but outside residency. An edit's
	 *  footprint creates cells too, so a stamp dragged far from the camera shows up here. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Debug|Streaming", DisplayName = "Draw Cell Residency")
	bool bDrawCellResidency = false;

	/** Draw the residency rings actually used this tick, in world XY (the plane the admission test measures
	 *  in): white the preview radius, and grey the unload radius it only releases beyond — the grey ring
	 *  appears only when hysteresis puts it past the white one. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Debug|Streaming", DisplayName = "Draw Residency Ring")
	bool bDrawResidencyRing = false;

	/** Collision patches BUILT RIGHT NOW: orange built, red holding no height source at all. Pawn probes
	 *  build patches too, so these are not confined to the view. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Debug|Streaming", DisplayName = "Draw Collision Patches")
	bool bDrawCollisionPatches = false;

	/** Draw the tile/cell key as text on top of the tile and cell layers. A modifier: on its own it turns
	 *  no layer on and draws nothing. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Debug|Streaming", DisplayName = "Draw Key Labels")
	bool bDrawKeyLabels = false;

	// ---- Terrain|Debug|Data ----------------------------------------------------

	/** WHAT IS BAKED ON DISK for the HEIGHT rail, drawn at true tile size wherever it is and never mind the
	 *  camera: violet baked and usable, dark violet present but rejected under this configuration's lattice.
	 *  Loads the manifest from disk once when switched on, and on a terrain that has none that first read
	 *  WRITES one: a directory scan plus a manifest save, on the game thread. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Debug|Data", DisplayName = "Draw Baked Height Tiles")
	bool bDrawBakedHeightTiles = false;

	/** WHAT IS BAKED ON DISK for the WEIGHT rail, at true tile size: emerald baked and usable, dark emerald
	 *  present but rejected under this configuration's lattice. Same first-enable manifest read as Draw
	 *  Baked Height Tiles. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Debug|Data", DisplayName = "Draw Baked Weight Tiles")
	bool bDrawBakedWeightTiles = false;

	/** WHAT IS BAKED ON DISK for the DATA rail, at true tile size: rose baked and usable, dark rose present
	 *  but rejected under this configuration's lattice. Same first-enable manifest read as Draw Baked
	 *  Height Tiles. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Debug|Data", DisplayName = "Draw Baked Data Tiles")
	bool bDrawBakedDataTiles = false;

	/** True when any debug overlay layer is on. Checked before the subsystem calls into the overlay at
	 *  all, so an all-false terrain pays for nothing beyond this one OR. bDrawKeyLabels is deliberately
	 *  absent: it labels other layers and enables none. */
	bool HasDebugOverlayEnabled() const
	{
		return bDrawHeightTileResidency || bDrawWeightTileResidency || bDrawDataTileResidency
			|| bDrawCellResidency || bDrawResidencyRing || bDrawCollisionPatches
			|| bDrawBakedHeightTiles || bDrawBakedWeightTiles || bDrawBakedDataTiles;
	}

	/** Channel names from Config.ChannelSet, or empty when no channel set is assigned. */
	TArray<FName> GetTerrainChannels() const;

	/** Editor-assigned terrain material, or null (subsystem then resolves the path/engine fallback). */
	UMaterialInterface* GetTerrainMaterial() const { return TerrainMaterial; }

	/** Terrain-local frame (the actor transform). Cell/stamp math is done in this space. */
	FTransform GetTerrainTransform() const;

	/** World position -> terrain-local position. */
	FVector WorldToTerrainLocal(const FVector& WorldPos) const;

	//~ Begin AActor interface
	virtual void PostLoad() override;
	virtual void PostRegisterAllComponents() override;
	virtual void Destroyed() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End AActor interface

private:
	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void CookAdditionalFilesOverride(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform,
		TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile) override;
#endif // WITH_EDITOR
	//~ End UObject Interface

	/** REPORT the cross-field grid constraints Config cannot express on its own: every tile extent at
	 *  least CellExtent (a cell's data has to land in ONE file), WeightSliceShift at most SectorShift,
	 *  the height and weight atlas slice sides within their texture limits, and every rail's tile within
	 *  ArcTerrain::MaxTileQuads. Logs one error per violation naming the field to change and the value
	 *  that would satisfy it, and fills GridInvariantReport for the panel. Writes no Config field: the
	 *  texture limit it reports against is this machine's, and belongs in a message, not in a map asset. */
	void ValidateGridInvariants();

	/** Append one message per violated grid constraint. Reads nothing but Config and the residency
	 *  shifts, writes nothing — ValidateGridInvariants owns the logging and the panel row. */
	void GatherGridViolations(TArray<FString>& Violations) const;

	/** Rebuild Config.DerivedGridSummary — the read-only panel row naming the resolutions and byte sizes
	 *  the authored enums resolve to, prefixed with GridInvariantReport when it is non-empty. */
	void RefreshDerivedGridSummary();

	/** Clamp FaceQuadsShift into [18, 24], warning once per correction. ArcTerrain::ResolveFaceGrid clamps
	 *  too, but silently — an authored value that would have produced 1.5e9 faces has to be visible. */
	void EnforceFaceGridInvariant();

	/** Warn when the resolved face grid covers less than WorldExtentCm asked for. Nothing to correct —
	 *  the ceilings are the renderer's, not the author's — but silently covering a fraction of the
	 *  authored world is exactly the failure a clamp must not hide. */
	void EnforceFaceGridExtentInvariant();

	/** Plain scene root defining the terrain frame. */
	UPROPERTY(VisibleAnywhere, Category = "Terrain")
	TObjectPtr<USceneComponent> SceneRoot;

	/** One warning per clamped configuration, re-armed when the grid stops being clamped. */
	bool bWarnedFaceGridClamped = false;

	/** Violations found by the last ValidateGridInvariants, joined for the panel. Empty when valid. */
	FString GridInvariantReport;
};
