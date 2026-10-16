// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Lighting/ArcTerrainLightingMesh.h"

class UArcTerrainFarOcclusionComponent;
class UArcTerrainLightingComponent;

/**
 * One cell's lighting-representation build. The worker writes NOTHING but this struct, which the task
 * co-owns: teardown drops the patch's handle and the task finishes into a buffer that is still alive.
 * Empty HeightsWithApron == the cell is flat; WeightPlanesWithApron is in TILE order, addressed through
 * PlaneChannelIndices, and its source resolution is the weight rail's own, not the height rail's.
 */
struct FArcTerrainLightingBuild
{
	FArcTerrainCellKey Key = FArcTerrainCellKey::ZeroValue;
	TArray<float> HeightsWithApron;
	TArray<TArray<uint8>> WeightPlanesWithApron;
	TArray<int32> PlaneChannelIndices;
	int32 SourceHeightQuads = 0;
	int32 SourceWeightQuads = 0;
	double CellSize = 0.0;
	float FlatHeightCm = 0.0f;
	float ToleranceCm = 0.0f;
	float SkirtDepthCm[ArcTerrainLightingMesh::GSkirtEdgeCount] = {};
	float SinkCm = 0.0f;

	ArcTerrainLightingMesh::FMesh Mesh;
	TArray<float> HeightRaster;
	TArray<TArray<uint8>> WeightRasters;
	TArray<uint8> PackedHeight;

	uint64 BuiltDirtySerial = 0;
	uint32 BuiltHeightPayload = 0;
	uint32 BuiltWeightPayload = 0;
	uint32 OwnerId = 0;
	uint32 TerrainId = 0;
	uint64 PatchGeneration = 0;
	uint64 ConfigGeneration = 0;
	uint64 SourceRevision = 0;
	uint64 BuiltMaterialRevision = 0;
};

using FArcTerrainLightingBuildPtr = TSharedPtr<FArcTerrainLightingBuild, ESPMode::ThreadSafe>;

/**
 * One cell's lighting representation: the third registry, on the CELL lattice but keyed, built and torn
 * down without any FArcTerrainCellState existing under it — a cell exists only where something shaped
 * it, and flat ground is most of the world and still has to cast a shadow.
 */
struct FArcTerrainLightingPatch
{
	TWeakObjectPtr<UArcTerrainLightingComponent> Component;

	FArcTerrainLightingBuildPtr InFlight;
	uint64 PatchGeneration = 0;
	uint64 SourceRevision = 1;
	FIntPoint HeightTileKey = FIntPoint::ZeroValue;
	FIntPoint WeightTileKey = FIntPoint::ZeroValue;
	bool bHadLoadedHeight = false;
	bool bHadLoadedWeight = false;

	/** Source identity is the payload's PayloadId, never its address: a released payload's address is
	 *  handed straight back to the next one, so an address would miss the swap. Two of them because the
	 *  height and weight rails are separate tiles with separate arrival times. */
	uint32 AppliedHeightPayload = 0;
	uint32 AppliedWeightPayload = 0;

	uint64 AppliedDirtySerial = 0;
	uint64 AppliedMaterialRevision = 0;
	uint64 DirtySerial = 1;
	float AppliedToleranceCm = 0.0f;
	float AppliedSinkCm = 0.0f;

	/** Which way the flat <-> shaped transition went; PayloadId cannot say. */
	bool bAppliedFlat = false;

	bool bBuildFailed = false;
	uint32 FailedHeightPayload = 0;
	uint32 FailedWeightPayload = 0;
	uint64 FailedDirtySerial = 0;
	float FailedToleranceCm = 0.0f;

	/** The resolved source rasters are build inputs too: a weight-lattice or composite-quads change makes
	 *  the same payload a different build, so the failure latch has to name them. */
	int32 FailedHeightQuads = 0;
	int32 FailedWeightQuads = 0;

	/** The cell's OWN band, with hysteresis (INDEX_NONE = none resolved yet). */
	int32 ToleranceBand = INDEX_NONE;

	/** The band WITHOUT hysteresis, a pure function of the key: what both sides of a seam agree on for
	 *  skirt depth. */
	int32 SkirtBand = INDEX_NONE;

	int32 SinkBand = INDEX_NONE;
};

/** Key of one FAR block, not of a cell. */
using FArcTerrainFarBlockKey = FIntPoint;

/**
 * One far block's occlusion-mesh build. Same co-ownership contract as FArcTerrainLightingBuild: teardown
 * drops the patch's handle and the task finishes into a buffer that is still alive.
 */
struct FArcTerrainFarBlockBuild
{
	TArray<ArcTerrainLightingMesh::FFarBlockSource> Sources;
	FArcTerrainFarBlockKey BlockKey = FArcTerrainFarBlockKey::ZeroValue;
	FArcTerrainCellKey BlockOriginCell = FArcTerrainCellKey::ZeroValue;
	int32 SourceQuads = 0;
	double CellSize = 0.0;
	float ToleranceCm = 0.0f;
	float SkirtDepthCm = 0.0f;
	float SinkCm = 0.0f;

	ArcTerrainLightingMesh::FMesh Mesh;

	uint64 BuiltDirtySerial = 0;
	uint32 BuiltSourceHash = 0;
	uint32 OwnerId = 0;
	uint32 TerrainId = 0;
	uint64 PatchGeneration = 0;
	uint64 ConfigGeneration = 0;
	uint64 SourceRevision = 0;
};

using FArcTerrainFarBlockBuildPtr = TSharedPtr<FArcTerrainFarBlockBuild, ESPMode::ThreadSafe>;

/**
 * One far block's occlusion representation: the fourth registry, on the BLOCK lattice. A block whose
 * manifest names a height tile that is not resident yet is deferred WHOLE — a per-cell fallback would
 * leave a hole or a fake-flat field inside the geometry and pop when the tile lands.
 */
struct FArcTerrainFarBlockPatch
{
	TWeakObjectPtr<UArcTerrainFarOcclusionComponent> Component;

	FArcTerrainFarBlockBuildPtr InFlight;
	uint64 PatchGeneration = 0;
	uint64 SourceRevision = 1;
	TSet<FIntPoint> LoadedHeightTiles;

	uint64 AppliedDirtySerial = 0;
	uint64 DirtySerial = 1;

	/** Combined PayloadId of the block's cells at the moment it was applied. A tile landing after the block
	 *  built flat bumps no serial anywhere, so the serial alone would leave that block flat forever. */
	uint32 AppliedSourceHash = 0;

	float AppliedSinkCm = 0.0f;

	int32 SinkBand = INDEX_NONE;

	/** A block whose sources cannot be cut stops competing for the frame's build slot until one of the
	 *  latched inputs moves; a DirtySerial bump is what lets an edit retry it. */
	bool bBuildFailed = false;
	uint64 FailedDirtySerial = 0;
	uint32 FailedSourceHash = 0;
	float FailedToleranceCm = 0.0f;
	int32 FailedSourceQuads = 0;
};

struct FArcTerrainLightingWorkerResult
{
	FArcTerrainLightingBuildPtr Build;
	bool bHeightCutFailed = false;
	bool bWeightCutFailed = false;
	bool bWeightUnresolvable = false;
	int32 ObservedHeightResolution = 0;
	int32 ObservedWeightResolution = 0;
};

struct FArcTerrainFarBlockWorkerResult
{
	FArcTerrainFarBlockBuildPtr Build;
	bool bCutFailed = false;
	FArcTerrainCellKey FailedCell = FArcTerrainCellKey::ZeroValue;
	int32 ObservedResolution = 0;
};
