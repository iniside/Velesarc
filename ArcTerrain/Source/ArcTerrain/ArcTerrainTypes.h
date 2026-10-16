// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h" // HALF_WORLD_MAX (GCellZHalfExtent)
#include "Math/IntRect.h"
#include "ArcTerrainTypes.generated.h"

class UArcTerrainChannelSet;

/**
 * Integer cell coordinate of a terrain tile in the terrain's local XY grid. A cell spans
 * [Key.X * CellSize, (Key.X + 1) * CellSize] x [Key.Y * CellSize, (Key.Y + 1) * CellSize].
 */
using FArcTerrainCellKey = FIntPoint;

/**
 * Integer coordinate of a collision patch in the terrain's local XY grid. ITS OWN LATTICE, spaced by
 * AArcTerrain::CollisionPatchExtent and unrelated to the cell grid: a patch spans
 * [Key.X * PatchSize, (Key.X + 1) * PatchSize] x [Key.Y * PatchSize, (Key.Y + 1) * PatchSize].
 */
using FArcTerrainCollisionKey = FIntPoint;

namespace ArcTerrain
{
	/** Guards on k in FaceQuads = 2^k. The ceiling is the corner-decode exactness limit (a face of 2^k
	 *  texels reaches LEB depth 2k, and the float32 decode is exact to 49); the floor stops a small face
	 *  from tiling the world with ~1e9 of them. */
	inline constexpr int32 MinFaceQuadsShift = 18;
	inline constexpr int32 MaxFaceQuadsShift = 24;

	/** Ceiling on the face grid, independent of the texel-lattice bound. The grid is uploaded WHOLE every
	 *  frame (4 uint of adjacency + one texel origin per face), so this is a per-frame bandwidth budget,
	 *  not a memory one: 256^2 faces is 1.5 MB a frame. The shipping default (4e9 cm extent, 2^22-texel
	 *  face) needs 10. */
	inline constexpr int32 MaxFacesPerEdgeCount = 256;
}

/**
 * The base mesh's face lattice: a STATIC square grid of faces covering the whole world, each FaceQuads
 * texels on a side, centred on the terrain origin. A face is pure topology — it holds no atlas slice and
 * has no lifecycle, so every face exists from level start and none ever arrives or departs.
 */
struct FArcTerrainFaceGrid
{
	/** Texels per face side, always 2^k: a leaf's corner decodes in [0,1]^2 and Corner * FaceQuads has to
	 *  reproduce the corner's integer exactly, which only a power of two does at these depths. */
	int32 FaceQuads = 0;

	int32 NumFacesPerEdge = 0;

	/** Faces per edge the authored world extent asked for, before the texel-lattice and face-count
	 *  ceilings. Greater than NumFacesPerEdge means the grid covers less than the authored extent. */
	int64 RequestedFacesPerEdge = 0;

	/** floor(N/2). FaceKey spans [-HalfFaces, -HalfFaces + N) on both axes, so an odd N keeps its extra
	 *  row and column instead of losing them to a symmetric [-N/2, N/2). */
	int32 HalfFaces = 0;

	int32 NumFaces() const { return NumFacesPerEdge * NumFacesPerEdge; }

	bool IsValid() const { return FaceQuads > 0 && NumFacesPerEdge > 0; }

	FIntPoint FaceKeyFromId(int32 FaceId) const
	{
		const int32 Row = FaceId / NumFacesPerEdge;
		return FIntPoint(FaceId - Row * NumFacesPerEdge - HalfFaces, Row - HalfFaces);
	}

	/** INDEX_NONE outside the grid — that is what makes the outermost faces' halfedges free boundaries. */
	int32 FaceIdFromKey(FIntPoint FaceKey) const
	{
		const int32 Column = FaceKey.X + HalfFaces;
		const int32 Row = FaceKey.Y + HalfFaces;
		if (Column < 0 || Row < 0 || Column >= NumFacesPerEdge || Row >= NumFacesPerEdge)
		{
			return INDEX_NONE;
		}
		return Row * NumFacesPerEdge + Column;
	}

	/** Global texel index of the face's (0,0) corner — the canonical number every address derives from. */
	FIntPoint FaceTexelOrigin(int32 FaceId) const
	{
		const FIntPoint FaceKey = FaceKeyFromId(FaceId);
		return FIntPoint(FaceKey.X * FaceQuads, FaceKey.Y * FaceQuads);
	}
};

namespace ArcTerrain
{
	/**
	 * THE authority on the face lattice. TexelCm sets the texel, FaceQuads = 2^k the face, and
	 * NumFacesPerEdge = ceil(WorldExtentCm / FaceSizeCm) the coverage — so the grid follows the authored
	 * world extent instead of a hardcoded face count. Two ceilings bound the result — the texel lattice
	 * (below) and ArcTerrain::MaxFacesPerEdgeCount — and AArcTerrain::EnforceFaceGridExtentInvariant
	 * warns when either bites.
	 */
	inline FArcTerrainFaceGrid ResolveFaceGrid(double TexelCm, int32 AuthoredFaceQuadsShift,
		double WorldExtentCm)
	{
		FArcTerrainFaceGrid Grid;

		if (TexelCm <= 0.0 || WorldExtentCm <= 0.0)
		{
			return Grid;
		}

		Grid.FaceQuads = 1 << FMath::Clamp(AuthoredFaceQuadsShift, MinFaceQuadsShift, MaxFaceQuadsShift);

		const double FaceSizeCm = static_cast<double>(Grid.FaceQuads) * TexelCm;
		Grid.RequestedFacesPerEdge = static_cast<int64>(FMath::CeilToDouble(WorldExtentCm / FaceSizeCm));

		// |GlobalTexel| <= MAX_int32 / 2, NOT MAX_int32: the predicate measures an edge as the int32
		// difference GlobalTexel - CameraTexel, and two origins that each merely fit int32 wrap it, which
		// reads the far side of the world as adjacent to the eye.
		const int64 TexelBound = static_cast<int64>(MAX_int32) / static_cast<int64>(Grid.FaceQuads);
		Grid.NumFacesPerEdge = static_cast<int32>(FMath::Clamp<int64>(Grid.RequestedFacesPerEdge, 1,
			FMath::Min<int64>(TexelBound, MaxFacesPerEdgeCount)));
		Grid.HalfFaces = Grid.NumFacesPerEdge / 2;
		return Grid;
	}
}

/**
 * How a stamp composites against the existing surface (and, via sequential overlap compositing,
 * against earlier overlapping stamps). Additive is FIRST so it is the default.
 */
UENUM()
enum class EArcTerrainBlendMode : uint8
{
	Additive UMETA(DisplayName = "Additive", ToolTip = "Stack: adds the source as a signed delta on top of the existing surface."),
	AlphaBlend UMETA(DisplayName = "Alpha Blend", ToolTip = "Conform: blends the existing surface toward the source's absolute target value."),
	Min UMETA(DisplayName = "Min", ToolTip = "Deepen: lowers toward the target, keeping the lower of the two."),
	Max UMETA(DisplayName = "Max", ToolTip = "Raise: lifts toward the target, keeping the higher of the two.")
};

/** Composited texel spacing. The enumerator's VALUE IS the spacing in cm. A new entry must also be
 *  listed in ArcTerrain::GTexelSizeValues, whose static_assert holds the power-of-two grid. */
UENUM()
enum class EArcTerrainTexelSize : uint16
{
	Cm25 = 25 UMETA(DisplayName = "25 cm / texel"),
	Cm50 = 50 UMETA(DisplayName = "50 cm / texel"),
	Cm100 = 100 UMETA(DisplayName = "1 m / texel"),
	Cm200 = 200 UMETA(DisplayName = "2 m / texel"),
	Cm400 = 400 UMETA(DisplayName = "4 m / texel"),
};

/** Square extent of a cell or an on-disk tile. The enumerator's VALUE IS the extent in METRES — every
 *  consumer multiplies it by 100 for cm. A new entry must also be listed in ArcTerrain::GExtentValues,
 *  whose static_assert holds the power-of-two grid. */
UENUM()
enum class EArcTerrainExtent : uint32
{
	M64 = 64 UMETA(DisplayName = "64 m"),
	M128 = 128 UMETA(DisplayName = "128 m"),
	M256 = 256 UMETA(DisplayName = "256 m"),
	M512 = 512 UMETA(DisplayName = "512 m"),
	M1024 = 1024 UMETA(DisplayName = "1 km"),
	M2048 = 2048 UMETA(DisplayName = "2 km"),
	M4096 = 4096 UMETA(DisplayName = "4 km"),
	M8192 = 8192 UMETA(DisplayName = "8 km"),
};

namespace ArcTerrain
{
	inline constexpr int32 MaxTileQuads = 8192;

	/** Side, in texels, of the largest weight atlas slice. The apron costs 3 of it. */
	inline constexpr int32 MaxWeightSliceSide = 4096;

	/** Every enumerator of EArcTerrainExtent / EArcTerrainTexelSize, in the same order as the enums.
	 *  GridQuadsAreAllPowerOfTwo below is the reason both lists exist. */
	inline constexpr EArcTerrainExtent GExtentValues[] = {
		EArcTerrainExtent::M64, EArcTerrainExtent::M128, EArcTerrainExtent::M256, EArcTerrainExtent::M512,
		EArcTerrainExtent::M1024, EArcTerrainExtent::M2048, EArcTerrainExtent::M4096,
		EArcTerrainExtent::M8192,
	};

	inline constexpr EArcTerrainTexelSize GTexelSizeValues[] = {
		EArcTerrainTexelSize::Cm25, EArcTerrainTexelSize::Cm50, EArcTerrainTexelSize::Cm100,
		EArcTerrainTexelSize::Cm200, EArcTerrainTexelSize::Cm400,
	};

	/** True when extent / texel is an exact, non-zero power-of-two texel count for EVERY pair of the two
	 *  enums. The shift-based residency and composite keys index the grid by FloorLog2 of that count, so
	 *  a pair that lands off a power of two makes the keys address a different lattice than the composite
	 *  writes, with no runtime diagnostic. */
	constexpr bool GridQuadsAreAllPowerOfTwo()
	{
		for (EArcTerrainExtent Extent : GExtentValues)
		{
			for (EArcTerrainTexelSize TexelSize : GTexelSizeValues)
			{
				const uint32 TexelCm = static_cast<uint32>(TexelSize);
				const uint32 ExtentCm = static_cast<uint32>(Extent) * 100u;
				if (TexelCm == 0u || ExtentCm % TexelCm != 0u)
				{
					return false;
				}
				const uint32 Quads = ExtentCm / TexelCm;
				if (Quads == 0u || (Quads & (Quads - 1u)) != 0u)
				{
					return false;
				}
			}
		}
		return true;
	}

	static_assert(GridQuadsAreAllPowerOfTwo(),
		"Every EArcTerrainExtent / EArcTerrainTexelSize pair must divide into a power-of-two texel count. "
		"Add the new enumerator to GExtentValues / GTexelSizeValues, and pick a value that keeps the "
		"quotient a power of two (extent = 2^a metres, texel = 100 * 2^k cm).");
}

/**
 * Spatial layout of a terrain: texel density, cell extent, on-disk tile extents, and the channel set
 * that names the weight layers. The terrain is an ALWAYS-INFINITE plane — there is no bounded cell
 * rectangle; cells are seeded on demand by invoker radius + stamp footprints. All distances in cm; all
 * cell math done in double (LWC-safe).
 *
 * AArcTerrain::ValidateGridInvariants REPORTS the cross-field constraints (tile >= cell, atlas side
 * limits, MaxTileQuads) and corrects nothing — the accessors below stay pure functions of the fields
 * and resolve a violating configuration to whatever it actually is, never to a plausible substitute.
 */
USTRUCT(BlueprintType)
struct ARCTERRAIN_API FArcTerrainConfig
{
	GENERATED_BODY()

	/** THE authored density: one number for the height composite, the render tile and the collision
	 *  raster's floor. Every extent below divides by it into a whole power-of-two texel count. */
	UPROPERTY(EditAnywhere, Category = "Terrain")
	EArcTerrainTexelSize TexelSize = EArcTerrainTexelSize::Cm100;

	/** Edge of one square cell. The render tile is QuadsPerCell() + 3 texels per edge (1-texel apron per
	 *  side for edge-correct gradients); adjacent cells SHARE their border texel column/row. */
	UPROPERTY(EditAnywhere, Category = "Terrain")
	EArcTerrainExtent CellExtent = EArcTerrainExtent::M256;

	/** Ordered weight channels shared with the material expression nodes. */
	UPROPERTY(EditAnywhere, Category = "Terrain")
	TObjectPtr<UArcTerrainChannelSet> ChannelSet;

	/** World size (cm) of one composited weight texel, decoupling the weight resolution from the mesh
	 *  quad count. Capped at the largest native weight-source dimension present on the terrain and kept
	 *  UNIFORM across every cell (the cross-cell weight seam, plan I1). Need NOT be a power of two. */
	UPROPERTY(EditAnywhere, Category = "Terrain", meta = (ClampMin = "1.0", DisplayName = "Weight Texel Size (cm)"))
	float WeightTexelSizeWorld = 100.0f;

	/** Edge of one on-disk HEIGHT tile. At least CellExtent: a cell's data has to land in ONE file. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Tiles")
	EArcTerrainExtent HeightTileExtent = EArcTerrainExtent::M1024;

	UPROPERTY(EditAnywhere, Category = "Terrain|Tiles")
	EArcTerrainExtent WeightTileExtent = EArcTerrainExtent::M1024;

	UPROPERTY(EditAnywhere, Category = "Terrain|Tiles")
	EArcTerrainExtent DataTileExtent = EArcTerrainExtent::M1024;

	/** Resolutions and byte sizes the fields above resolve to, rebuilt by
	 *  AArcTerrain::PostEditChangeProperty. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Terrain", meta = (DisplayName = "Derived"))
	FString DerivedGridSummary;

	int32 TexelSizeCm() const { return static_cast<int32>(TexelSize); }

	double CellSize() const { return static_cast<double>(static_cast<uint32>(CellExtent)) * 100.0; }

	/** Height texels along one edge of a square of this extent. 0 when TexelSize holds an enumerator this
	 *  build does not know (a UEnumProperty deserialises an unknown name to 0). */
	int32 TileQuads(EArcTerrainExtent Extent) const
	{
		const uint32 TexelCm = static_cast<uint32>(TexelSize);
		if (TexelCm == 0u)
		{
			return 0;
		}
		return static_cast<int32>(static_cast<uint32>(Extent) * 100u / TexelCm);
	}

	int32 QuadsPerCell() const { return TileQuads(CellExtent); }
	int32 HeightTileQuads() const { return TileQuads(HeightTileExtent); }
	int32 DataTileQuads() const { return TileQuads(DataTileExtent); }

	/** Cells along one edge of a tile of this extent. 0 when the tile is NARROWER than a cell, which is
	 *  the tile >= cell constraint being violated — not floored at 1, because a floor would resolve that
	 *  configuration to one cell's worth of tile and hide the breakage the panel has to show. */
	int32 CellsPerTileEdge(EArcTerrainExtent TileExtent) const
	{
		const uint32 CellMetres = static_cast<uint32>(CellExtent);
		if (CellMetres == 0u)
		{
			return 0;
		}
		return static_cast<int32>(static_cast<uint32>(TileExtent) / CellMetres);
	}

	/** Weight texels along a CELL edge BEFORE the runtime's native-source-dimension cap and slice ceiling
	 *  (both only ever lower it), so this is the upper bound validation reports against. */
	int32 AuthoredCellWeightQuads() const
	{
		const int32 HeightQuads = QuadsPerCell();
		if (WeightTexelSizeWorld <= 0.0f)
		{
			return HeightQuads;
		}
		return FMath::Max(
			FMath::RoundToInt(static_cast<float>(CellSize()) / WeightTexelSizeWorld), HeightQuads);
	}

	int32 AuthoredWeightTileQuads() const
	{
		return AuthoredCellWeightQuads() * CellsPerTileEdge(WeightTileExtent);
	}
};

/**
 * Per-terrain height authoring knobs. NOT part of FArcTerrainConfig — they must stay tunable after
 * the config locks (they only re-bake the height sections, never reshape the cell frame);
 * AArcTerrain::HeightOptions routes an edit through
 * UArcTerrainSubsystem::OnTerrainHeightOptionsChanged.
 */
USTRUCT()
struct ARCTERRAIN_API FArcTerrainHeightOptions
{
	GENERATED_BODY()

	/** Terrain-local height in cm of ground no stamp covers.
	 *
	 *  It is the value the cell composite starts from and the value an atlas slice is cleared to, so
	 *  the two agree on what "unstamped" means.
	 *
	 *  It must be a pure function of authored data: every staleness probe re-derives the key, and the
	 *  interactive single-cell path has no region to measure over.
	 *
	 *  Defaults to 0 so a freshly placed terrain sits on the origin plane. Lower it to author ground
	 *  that descends below sea level without every stamp having to carve down to it. */
	UPROPERTY(EditAnywhere, Category = "Terrain|Height", meta = (DisplayName = "Base Height (cm)"))
	float BaseHeightCm = 0.0f;
};

namespace ArcTerrain
{
	/** Z half-extent of a partitioning AABB — a cell's or a tile's. Both partition the terrain in XY
	 *  only; Z is deliberately left effectively unbounded so a bounds query never rejects on Z.
	 *  HALF_WORLD_MAX is the engine's half-world size, larger than any real terrain vertical span. */
	inline constexpr double GCellZHalfExtent = HALF_WORLD_MAX;

	/** Cell containing a terrain-local position (floor division on X and Y). Not clamped to bounds. */
	ARCTERRAIN_API FArcTerrainCellKey CellKeyFromLocal(const FVector& LocalPos, const FArcTerrainConfig& Config);

	/** Terrain-local AABB of a cell. XY is the cell's [k*CellSize, (k+1)*CellSize] span; Z is left
	 *  effectively unbounded (a large finite extent) — cells partition XY only, never Z. */
	ARCTERRAIN_API FBox CellLocalBounds(FArcTerrainCellKey Key, const FArcTerrainConfig& Config);

	/**
	 * Cells whose closed AABB intersects LocalBounds (XY only), EDGE-INCLUSIVE: an edge landing exactly
	 * on a cell boundary intersects BOTH cells that share that boundary, so a shared border texel
	 * composites identically in adjacent cells. Purely geometric — the terrain is an infinite plane, so
	 * results are the raw [MinCell..MaxCell] rectangle with no bounds clamp (Config supplies CellSize).
	 */
	ARCTERRAIN_API void CellsIntersectingLocalBounds(const FBox& LocalBounds, const FArcTerrainConfig& Config,
		TArray<FArcTerrainCellKey>& OutCells);

	/** The inclusive cell rectangle CellsIntersectingLocalBounds would emit, without materialising the
	 *  keys — the single authority on the edge-inclusive rule for callers that only need the extent.
	 *  False (outputs untouched) when LocalBounds or CellSize is unusable. */
	ARCTERRAIN_API bool CellRangeIntersectingLocalBounds(const FBox& LocalBounds,
		const FArcTerrainConfig& Config, FIntPoint& OutMinCell, FIntPoint& OutMaxCell);

	/** Terrain-local AABB of a collision patch, XY over PatchSize cm; Z unbounded like a cell's. */
	ARCTERRAIN_API FBox CollisionPatchLocalBounds(FArcTerrainCollisionKey Key, double PatchSize);

	/** Collision patches whose closed AABB intersects LocalBounds, on the same edge-inclusive rule as
	 *  CellsIntersectingLocalBounds — neighbouring patches share their boundary sample row, so an edit
	 *  landing on the boundary belongs to both. */
	ARCTERRAIN_API void CollisionPatchesIntersectingLocalBounds(const FBox& LocalBounds, double PatchSize,
		TArray<FArcTerrainCollisionKey>& OutKeys);
}
