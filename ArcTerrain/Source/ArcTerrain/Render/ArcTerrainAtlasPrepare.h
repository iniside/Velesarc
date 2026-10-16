// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTerrainSubsystemInternal.h"
#include "ArcTerrainTileAccess.h"
#include "Render/ArcTerrainTileAtlas.h"
#include "Runtime/ArcTerrainRuntimeWorkCoordinator.h"
#include "Weights/ArcTerrainWeightSlicePacking.h"

struct FArcTerrainChannelHashIndex
{
	uint32 ChannelNameHash = 0;
	int32 ChannelIndex = 0;
};

enum class EArcTerrainAtlasPrepareKind : uint8
{
	HeightCell = 0,
	HeightCoarse = 1,
	WeightCell = 2,
	WeightCoarse = 3,
	CbtTopology = 4
};

struct FArcTerrainAtlasPrepareStatus
{
	bool bNoTile = false;
	bool bResolutionMismatch = false;
	bool bCutFailed = false;
	bool bBudgetExceeded = false;
	bool bUnresolvable = false;
	int32 ObservedResolution = 0;
	int32 PayloadFloats = 0;
	int32 PayloadRail = 0;
	FIntPoint PayloadTileKey = FIntPoint::ZeroValue;
	TArray<uint32> UnknownPlaneIds;
	TArray<int32> DroppedChannels;
	int32 KeptTopK = 0;
};

struct FArcTerrainAtlasPrepareInput
{
	FArcTerrainRuntimeOwnerId OwnerId;
	uint64 TerrainId = 0;
	uint64 LayoutGeneration = 0;
	EArcTerrainAtlasPrepareKind Kind = EArcTerrainAtlasPrepareKind::HeightCell;
	FArcTerrainSectorId SectorId;
	FArcTerrainWeightGroupId GroupId;
	FArcTerrainCellKey CellKey = FArcTerrainCellKey::ZeroValue;
	int32 Slice = INDEX_NONE;
	uint64 SliceGeneration = 0;
	uint64 CellPublicationVersion = 0;
	uint32 ExpectedPayloadId = 0;
	FArcTerrainTileGrid Grid;
	FArcTerrainTileAtlasLayout Layout;
	int32 ExpectedResolution = 0;
	int32 GroupQuads = 0;
	int32 GroupsPerTileEdge = 0;
	FTileSourceCapture Source;
	TArray<FArcTerrainChannelHashIndex> ChannelMap;
};

struct FArcTerrainAtlasPrepareResult
{
	FArcTerrainAtlasPrepareInput Identity;
	FArcTerrainAtlasPrepareStatus Status;
	FArcTerrainAtlasCellFill Fill;
};

struct FArcTerrainCBTTopologyInput
{
	FArcTerrainFaceGrid FaceGrid;
	uint64 TopologySignature = 0;
	uint64 LayoutGeneration = 0;
};

struct FArcTerrainCBTTopologyResult
{
	uint64 TopologySignature = 0;
	uint64 LayoutGeneration = 0;
	int32 NumFaces = 0;
	int32 FaceQuads = 0;
	TArray<uint32> Neighbors;
	TArray<FIntPoint> FaceTexelOrigins;
	EArcTerrainRuntimeWorkStatus Status = EArcTerrainRuntimeWorkStatus::Succeeded;
};

struct FArcTerrainAtlasLanePayload
{
	TSharedPtr<FArcTerrainAtlasPrepareResult, ESPMode::ThreadSafe> Prepare;
	TSharedPtr<FArcTerrainCBTTopologyResult, ESPMode::ThreadSafe> Topology;
};

struct FArcTerrainAtlasInFlightKey
{
	EArcTerrainAtlasPrepareKind Kind = EArcTerrainAtlasPrepareKind::HeightCell;
	FArcTerrainSectorId SectorId;
	FArcTerrainWeightGroupId GroupId;
	FArcTerrainCellKey CellKey = FArcTerrainCellKey::ZeroValue;

	bool operator==(const FArcTerrainAtlasInFlightKey& Other) const
	{
		return Kind == Other.Kind && SectorId == Other.SectorId && GroupId == Other.GroupId
			&& CellKey == Other.CellKey;
	}
};

inline uint32 GetTypeHash(const FArcTerrainAtlasInFlightKey& Key)
{
	return HashCombine(
		HashCombine(GetTypeHash(Key.SectorId), GetTypeHash(Key.GroupId)),
		HashCombine(GetTypeHash(Key.CellKey), static_cast<uint32>(Key.Kind)));
}

namespace ArcTerrainAtlasPrepare
{
	ARCTERRAIN_API void CopyChannelHashIndexMap(const UArcTerrainChannelSet& ChannelSet,
		TArray<FArcTerrainChannelHashIndex>& OutMap);

	ARCTERRAIN_API uint64 EstimateHeightStagingBytes(int32 Resolution);
	ARCTERRAIN_API uint64 EstimateWeightStagingBytes(int32 Resolution, int32 PlaneCount);
	ARCTERRAIN_API uint64 EstimateCbtTopologyBytes(int32 NumFaces);

	ARCTERRAIN_API bool CutCellHeightStaging(
		const FArcTerrainTileGrid& Grid,
		const FArcTerrainTilePayloadPtr& Payload,
		FArcTerrainCellKey Key,
		int32 ExpectedResolution,
		TArray<uint8>& OutBytes,
		FVector2f& OutHeightRange,
		FArcTerrainAtlasPrepareStatus& OutStatus);

	ARCTERRAIN_API bool CutCoarseHeightStaging(
		const FArcTerrainTilePayloadPtr& Payload,
		int32 SliceSide,
		TArray<uint8>& OutBytes,
		FVector2f& OutHeightRange,
		FArcTerrainAtlasPrepareStatus& OutStatus);

	ARCTERRAIN_API bool CutCellWeightPlanes(
		FArcTerrainCellKey Key,
		const FArcTerrainTileGrid& Grid,
		const FArcTerrainTilePayloadPtr& Payload,
		TConstArrayView<FArcTerrainChannelHashIndex> ChannelMap,
		int32 ExpectedResolution,
		TArray<TArray<uint8>>& OutPlanes,
		TArray<int32>& OutChannels,
		FArcTerrainAtlasPrepareStatus& OutStatus);

	ARCTERRAIN_API bool CutGroupWeightPlanes(
		FIntPoint GroupKey,
		const FArcTerrainTileGrid& Grid,
		const FArcTerrainTilePayloadPtr& Payload,
		TConstArrayView<FArcTerrainChannelHashIndex> ChannelMap,
		int32 ExpectedResolution,
		int32 GroupQuads,
		int32 GroupsPerTileEdge,
		TArray<TArray<uint8>>& OutPlanes,
		TArray<int32>& OutChannels,
		FArcTerrainAtlasPrepareStatus& OutStatus);

	ARCTERRAIN_API FArcTerrainAtlasPrepareResult PrepareAtlasWork(
		const FArcTerrainAtlasPrepareInput& Input,
		FArcTerrainRuntimeWorkContext& Context);

	ARCTERRAIN_API FArcTerrainCBTTopologyResult BuildCbtTopology(
		const FArcTerrainCBTTopologyInput& Input,
		FArcTerrainRuntimeWorkContext& Context);
}
