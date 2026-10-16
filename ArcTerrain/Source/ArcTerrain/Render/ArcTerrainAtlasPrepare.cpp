// Copyright Lukasz Baran. All Rights Reserved.

#include "Render/ArcTerrainAtlasPrepare.h"

#include "ArcTerrainChannelSet.h"
#include "ArcTerrainTypes.h"

namespace ArcTerrainAtlasPrepareLocal_b4e9
{
	int32 FindChannelIndex(TConstArrayView<FArcTerrainChannelHashIndex> ChannelMap, uint32 PlaneId)
	{
		for (const FArcTerrainChannelHashIndex& Entry : ChannelMap)
		{
			if (Entry.ChannelNameHash == PlaneId)
			{
				return Entry.ChannelIndex;
			}
		}
		return INDEX_NONE;
	}

	bool GrowForBytes(FArcTerrainRuntimeWorkContext& Context, uint64 Bytes,
		FArcTerrainAtlasPrepareStatus& Status)
	{
		if (Context.TryGrow(Bytes))
		{
			return true;
		}
		Status.bBudgetExceeded = true;
		return false;
	}

	bool CutWeightPlanes(
		const FArcTerrainTilePayloadPtr& Payload,
		TConstArrayView<FArcTerrainChannelHashIndex> ChannelMap,
		int32 ExpectedResolution,
		TFunctionRef<bool(int32 PlaneIndex, TArray<uint8>& OutBytes, int32& OutResolution)> ExtractPlane,
		TArray<TArray<uint8>>& OutPlanes,
		TArray<int32>& OutChannels,
		FArcTerrainAtlasPrepareStatus& OutStatus)
	{
		OutPlanes.Reset();
		OutChannels.Reset();
		OutStatus.UnknownPlaneIds.Reset();
		OutStatus.DroppedChannels.Reset();
		OutStatus.bUnresolvable = false;
		if (!Payload.IsValid())
		{
			OutStatus.bNoTile = true;
			return false;
		}
		if (Payload->Planes.IsEmpty())
		{
			OutStatus.bUnresolvable = true;
			return false;
		}

		for (int32 PlaneIndex = 0; PlaneIndex < Payload->Planes.Num(); ++PlaneIndex)
		{
			const uint32 PlaneId = Payload->Header.PlaneIds[PlaneIndex];
			const int32 ChannelIndex = FindChannelIndex(ChannelMap, PlaneId);
			if (ChannelIndex == INDEX_NONE)
			{
				OutStatus.UnknownPlaneIds.Add(PlaneId);
				continue;
			}

			TArray<uint8> Bytes;
			int32 Resolution = 0;
			if (!ExtractPlane(PlaneIndex, Bytes, Resolution) || Resolution != ExpectedResolution)
			{
				OutStatus.bCutFailed = true;
				OutStatus.ObservedResolution = Resolution;
				OutPlanes.Reset();
				OutChannels.Reset();
				return false;
			}
			OutStatus.ObservedResolution = Resolution;
			OutPlanes.Add(MoveTemp(Bytes));
			OutChannels.Add(ChannelIndex);
		}

		if (OutPlanes.IsEmpty())
		{
			OutStatus.bUnresolvable = true;
			return false;
		}

		if (OutPlanes.Num() <= ArcTerrainWeights::GArcTerrainWeightMaxSlices)
		{
			OutStatus.KeptTopK = OutPlanes.Num();
			return true;
		}

		struct FPlaneRank
		{
			int32 PlaneIndex = 0;
			uint8 MaxWeight = 0;
		};
		TArray<FPlaneRank> Ranks;
		Ranks.Reserve(OutPlanes.Num());
		for (int32 PlaneIndex = 0; PlaneIndex < OutPlanes.Num(); ++PlaneIndex)
		{
			uint8 MaxWeight = 0;
			for (const uint8 Texel : OutPlanes[PlaneIndex])
			{
				MaxWeight = FMath::Max(MaxWeight, Texel);
			}
			Ranks.Add(FPlaneRank{ PlaneIndex, MaxWeight });
		}
		Ranks.Sort([&OutChannels](const FPlaneRank& A, const FPlaneRank& B)
		{
			if (A.MaxWeight != B.MaxWeight)
			{
				return A.MaxWeight > B.MaxWeight;
			}
			return OutChannels[A.PlaneIndex] < OutChannels[B.PlaneIndex];
		});

		TArray<int32> KeptRanks;
		KeptRanks.Reserve(ArcTerrainWeights::GArcTerrainWeightMaxSlices);
		for (int32 Rank = 0; Rank < ArcTerrainWeights::GArcTerrainWeightMaxSlices; ++Rank)
		{
			KeptRanks.Add(Ranks[Rank].PlaneIndex);
		}
		KeptRanks.Sort([&OutChannels](int32 A, int32 B) { return OutChannels[A] < OutChannels[B]; });

		OutStatus.DroppedChannels.Reserve(Ranks.Num() - ArcTerrainWeights::GArcTerrainWeightMaxSlices);
		for (int32 Rank = ArcTerrainWeights::GArcTerrainWeightMaxSlices; Rank < Ranks.Num(); ++Rank)
		{
			OutStatus.DroppedChannels.Add(OutChannels[Ranks[Rank].PlaneIndex]);
		}

		TArray<TArray<uint8>> KeptPlanes;
		TArray<int32> KeptChannels;
		KeptPlanes.Reserve(KeptRanks.Num());
		KeptChannels.Reserve(KeptRanks.Num());
		for (const int32 PlaneIndex : KeptRanks)
		{
			KeptPlanes.Add(MoveTemp(OutPlanes[PlaneIndex]));
			KeptChannels.Add(OutChannels[PlaneIndex]);
		}
		OutPlanes = MoveTemp(KeptPlanes);
		OutChannels = MoveTemp(KeptChannels);
		OutStatus.KeptTopK = OutPlanes.Num();
		return true;
	}
}

namespace ArcTerrainAtlasPrepare
{
	void CopyChannelHashIndexMap(const UArcTerrainChannelSet& ChannelSet,
		TArray<FArcTerrainChannelHashIndex>& OutMap)
	{
		OutMap.Reset();
		OutMap.Reserve(ChannelSet.Channels.Num());
		for (int32 Index = 0; Index < ChannelSet.Channels.Num(); ++Index)
		{
			FArcTerrainChannelHashIndex Entry;
			Entry.ChannelNameHash = ArcTerrain::ChannelNameHash(ChannelSet.Channels[Index]);
			Entry.ChannelIndex = Index;
			OutMap.Add(Entry);
		}
	}

	uint64 EstimateHeightStagingBytes(int32 Resolution)
	{
		const int64 Side = FMath::Max(Resolution, 0);
		return static_cast<uint64>(Side) * static_cast<uint64>(Side) * sizeof(float);
	}

	uint64 EstimateWeightStagingBytes(int32 Resolution, int32 PlaneCount)
	{
		const int64 Side = FMath::Max(Resolution, 0);
		const int64 Planes = FMath::Max(PlaneCount, 0);
		return static_cast<uint64>(Side) * static_cast<uint64>(Side) * static_cast<uint64>(Planes);
	}

	uint64 EstimateCbtTopologyBytes(int32 NumFaces)
	{
		const int64 Faces = FMath::Max(NumFaces, 0);
		return static_cast<uint64>(Faces) * (4u * sizeof(uint32) + sizeof(FIntPoint));
	}

	bool CutCellHeightStaging(
		const FArcTerrainTileGrid& Grid,
		const FArcTerrainTilePayloadPtr& Payload,
		FArcTerrainCellKey Key,
		int32 ExpectedResolution,
		TArray<uint8>& OutBytes,
		FVector2f& OutHeightRange,
		FArcTerrainAtlasPrepareStatus& OutStatus)
	{
		OutBytes.Reset();
		OutHeightRange = ArcTerrainTileAtlasRender::InvalidHeightRange();
		if (!Payload.IsValid())
		{
			OutStatus.bNoTile = true;
			return false;
		}
		OutStatus.PayloadTileKey = Payload->TileKey;
		OutStatus.PayloadRail = static_cast<int32>(Payload->Rail);
		OutStatus.PayloadFloats = Payload->Floats.Num();

		TArray<float> Floats;
		int32 Resolution = 0;
		if (!ArcTerrainTileAccess::ExtractCellFloats(Grid, *Payload, 0, Key, Floats, Resolution)
			|| Resolution != ExpectedResolution)
		{
			OutStatus.bCutFailed = Resolution == ExpectedResolution;
			OutStatus.bResolutionMismatch = Resolution != 0 && Resolution != ExpectedResolution;
			OutStatus.ObservedResolution = Resolution;
			if (!OutStatus.bResolutionMismatch)
			{
				OutStatus.bCutFailed = true;
			}
			return false;
		}

		OutStatus.ObservedResolution = Resolution;
		float MinHeight = TNumericLimits<float>::Max();
		float MaxHeight = -TNumericLimits<float>::Max();
		for (const float Height : Floats)
		{
			if (!FMath::IsFinite(Height))
			{
				MinHeight = TNumericLimits<float>::Max();
				MaxHeight = -TNumericLimits<float>::Max();
				break;
			}
			MinHeight = FMath::Min(MinHeight, Height);
			MaxHeight = FMath::Max(MaxHeight, Height);
		}
		OutHeightRange = FVector2f(MinHeight, MaxHeight);
		OutBytes.SetNumUninitialized(Floats.Num() * sizeof(float));
		FMemory::Memcpy(OutBytes.GetData(), Floats.GetData(), Floats.Num() * sizeof(float));
		return true;
	}

	bool CutCoarseHeightStaging(
		const FArcTerrainTilePayloadPtr& Payload,
		int32 SliceSide,
		TArray<uint8>& OutBytes,
		FVector2f& OutHeightRange,
		FArcTerrainAtlasPrepareStatus& OutStatus)
	{
		OutBytes.Reset();
		OutHeightRange = ArcTerrainTileAtlasRender::InvalidHeightRange();
		if (!Payload.IsValid())
		{
			OutStatus.bNoTile = true;
			return false;
		}
		OutStatus.PayloadTileKey = Payload->TileKey;
		OutStatus.PayloadRail = static_cast<int32>(Payload->Rail);
		OutStatus.PayloadFloats = Payload->Floats.Num();
		OutStatus.ObservedResolution = Payload->Resolution();
		const int64 SliceTexels = static_cast<int64>(SliceSide) * SliceSide;
		if (Payload->Resolution() != SliceSide || Payload->Floats.Num() != SliceTexels)
		{
			OutStatus.bResolutionMismatch = true;
			return false;
		}
		OutHeightRange = Payload->Header.PlaneQuantMin.IsValidIndex(0)
			&& Payload->Header.PlaneQuantMax.IsValidIndex(0)
			? FVector2f(Payload->Header.PlaneQuantMin[0], Payload->Header.PlaneQuantMax[0])
			: ArcTerrainTileAtlasRender::InvalidHeightRange();
		OutBytes.SetNumUninitialized(Payload->Floats.Num() * sizeof(float));
		FMemory::Memcpy(OutBytes.GetData(), Payload->Floats.GetData(),
			Payload->Floats.Num() * sizeof(float));
		return true;
	}

	bool CutCellWeightPlanes(
		FArcTerrainCellKey Key,
		const FArcTerrainTileGrid& Grid,
		const FArcTerrainTilePayloadPtr& Payload,
		TConstArrayView<FArcTerrainChannelHashIndex> ChannelMap,
		int32 ExpectedResolution,
		TArray<TArray<uint8>>& OutPlanes,
		TArray<int32>& OutChannels,
		FArcTerrainAtlasPrepareStatus& OutStatus)
	{
		return ArcTerrainAtlasPrepareLocal_b4e9::CutWeightPlanes(Payload, ChannelMap, ExpectedResolution,
			[&Grid, &Payload, Key](int32 PlaneIndex, TArray<uint8>& OutBytes, int32& OutResolution)
			{
				return Payload.IsValid()
					&& ArcTerrainTileAccess::ExtractCellPlane(Grid, *Payload, PlaneIndex, Key, OutBytes,
						OutResolution);
			},
			OutPlanes, OutChannels, OutStatus);
	}

	bool CutGroupWeightPlanes(
		FIntPoint GroupKey,
		const FArcTerrainTileGrid& Grid,
		const FArcTerrainTilePayloadPtr& Payload,
		TConstArrayView<FArcTerrainChannelHashIndex> ChannelMap,
		int32 ExpectedResolution,
		int32 GroupQuads,
		int32 GroupsPerTileEdge,
		TArray<TArray<uint8>>& OutPlanes,
		TArray<int32>& OutChannels,
		FArcTerrainAtlasPrepareStatus& OutStatus)
	{
		return ArcTerrainAtlasPrepareLocal_b4e9::CutWeightPlanes(Payload, ChannelMap, ExpectedResolution,
			[&Grid, &Payload, GroupKey, GroupQuads, GroupsPerTileEdge](int32 PlaneIndex,
				TArray<uint8>& OutBytes, int32& OutResolution)
			{
				return Payload.IsValid()
					&& ArcTerrainTileAccess::ExtractGroupPlane(Grid, *Payload, PlaneIndex, GroupKey,
						GroupQuads, GroupsPerTileEdge, OutBytes, OutResolution);
			},
			OutPlanes, OutChannels, OutStatus);
	}

	FArcTerrainAtlasPrepareResult PrepareAtlasWork(
		const FArcTerrainAtlasPrepareInput& Input,
		FArcTerrainRuntimeWorkContext& Context)
	{
		FArcTerrainAtlasPrepareResult Result;
		Result.Identity = Input;
		Result.Fill.Key = Input.CellKey;
		if (Context.IsAbandoned())
		{
			Result.Status.bBudgetExceeded = false;
			return Result;
		}

		const FArcTerrainTilePayloadPtr& Payload = Input.Source.Payload;
		if (Input.Kind == EArcTerrainAtlasPrepareKind::HeightCell)
		{
			Result.Fill.HeightSlice = Input.Slice;
			Result.Fill.HeightDestOrigin = ArcTerrainAtlas::HeightTileOriginInSlice(
				Input.SectorId, Input.CellKey, Input.Layout);
			Result.Fill.HeightTileRes = Input.ExpectedResolution;
			Result.Fill.HeightWrite = FArcTerrainAtlasCellFill::EHeightWrite::Tier0Cell;
			const uint64 Bytes = EstimateHeightStagingBytes(Input.ExpectedResolution);
			if (!ArcTerrainAtlasPrepareLocal_b4e9::GrowForBytes(Context, Bytes, Result.Status))
			{
				return Result;
			}
			if (!CutCellHeightStaging(Input.Grid, Payload, Input.CellKey, Input.ExpectedResolution,
				Result.Fill.HeightStagingBytes, Result.Fill.HeightRange, Result.Status))
			{
				Result.Fill.HeightStagingBytes.Reset();
			}
			return Result;
		}

		if (Input.Kind == EArcTerrainAtlasPrepareKind::HeightCoarse)
		{
			Result.Fill.HeightSlice = Input.Slice;
			Result.Fill.HeightDestOrigin = FIntPoint::ZeroValue;
			Result.Fill.HeightTileRes = Input.ExpectedResolution;
			Result.Fill.HeightWrite = FArcTerrainAtlasCellFill::EHeightWrite::CoarseWholeSlice;
			const uint64 Bytes = EstimateHeightStagingBytes(Input.ExpectedResolution);
			if (!ArcTerrainAtlasPrepareLocal_b4e9::GrowForBytes(Context, Bytes, Result.Status))
			{
				return Result;
			}
			if (!CutCoarseHeightStaging(Payload, Input.ExpectedResolution, Result.Fill.HeightStagingBytes,
				Result.Fill.HeightRange, Result.Status))
			{
				Result.Fill.HeightStagingBytes.Reset();
			}
			return Result;
		}

		if (Input.Kind == EArcTerrainAtlasPrepareKind::WeightCell)
		{
			Result.Fill.WeightSlice = Input.Slice;
			Result.Fill.WeightTileRes = Input.ExpectedResolution;
			Result.Fill.WeightDestOrigin = FIntPoint(
				(Input.CellKey.X - (Input.GroupId.Key.X << Input.Layout.WeightSliceShift))
					* Input.Layout.WeightQuads,
				(Input.CellKey.Y - (Input.GroupId.Key.Y << Input.Layout.WeightSliceShift))
					* Input.Layout.WeightQuads);
			const uint64 Bytes = EstimateWeightStagingBytes(Input.ExpectedResolution,
				ArcTerrainWeights::GArcTerrainWeightMaxSlices);
			if (!ArcTerrainAtlasPrepareLocal_b4e9::GrowForBytes(Context, Bytes, Result.Status))
			{
				return Result;
			}
			if (!CutCellWeightPlanes(Input.CellKey, Input.Grid, Payload, Input.ChannelMap,
				Input.ExpectedResolution, Result.Fill.WeightStagingPlanes,
				Result.Fill.BakedWeightSliceChannels, Result.Status))
			{
				Result.Fill.WeightStagingPlanes.Reset();
				Result.Fill.BakedWeightSliceChannels.Reset();
			}
			return Result;
		}

		Result.Fill.WeightSlice = Input.Slice;
		Result.Fill.WeightDestOrigin = FIntPoint::ZeroValue;
		Result.Fill.WeightTileRes = Input.ExpectedResolution;
		const uint64 Bytes = EstimateWeightStagingBytes(Input.ExpectedResolution,
			ArcTerrainWeights::GArcTerrainWeightMaxSlices);
		if (!ArcTerrainAtlasPrepareLocal_b4e9::GrowForBytes(Context, Bytes, Result.Status))
		{
			return Result;
		}
		if (!CutGroupWeightPlanes(Input.GroupId.Key, Input.Grid, Payload, Input.ChannelMap,
			Input.ExpectedResolution, Input.GroupQuads, Input.GroupsPerTileEdge,
			Result.Fill.WeightStagingPlanes, Result.Fill.BakedWeightSliceChannels, Result.Status))
		{
			Result.Fill.WeightStagingPlanes.Reset();
			Result.Fill.BakedWeightSliceChannels.Reset();
		}
		return Result;
	}

	FArcTerrainCBTTopologyResult BuildCbtTopology(
		const FArcTerrainCBTTopologyInput& Input,
		FArcTerrainRuntimeWorkContext& Context)
	{
		FArcTerrainCBTTopologyResult Result;
		Result.TopologySignature = Input.TopologySignature;
		Result.LayoutGeneration = Input.LayoutGeneration;
		const int32 NumFaces = Input.FaceGrid.IsValid() ? Input.FaceGrid.NumFaces() : 0;
		Result.NumFaces = NumFaces;
		Result.FaceQuads = Input.FaceGrid.FaceQuads;
		if (NumFaces <= 0)
		{
			return Result;
		}
		const uint64 Bytes = EstimateCbtTopologyBytes(NumFaces);
		if (!Context.TryGrow(Bytes))
		{
			Result.Status = EArcTerrainRuntimeWorkStatus::BudgetExceeded;
			return Result;
		}

		static const FIntPoint SideOffsets[4] = {
			FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1) };
		Result.Neighbors.Init(0xFFFFFFFFu, NumFaces * 4);
		Result.FaceTexelOrigins.SetNumUninitialized(NumFaces);
		for (int32 FaceId = 0; FaceId < NumFaces; ++FaceId)
		{
			const FIntPoint FaceKey = Input.FaceGrid.FaceKeyFromId(FaceId);
			Result.FaceTexelOrigins[FaceId] =
				FIntPoint(FaceKey.X * Input.FaceGrid.FaceQuads, FaceKey.Y * Input.FaceGrid.FaceQuads);
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const int32 NeighborId = Input.FaceGrid.FaceIdFromKey(FaceKey + SideOffsets[Side]);
				if (NeighborId != INDEX_NONE)
				{
					Result.Neighbors[FaceId * 4 + Side] = static_cast<uint32>(NeighborId);
				}
			}
		}
		return Result;
	}
}
