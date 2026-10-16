// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTerrainSubsystemInternal.h"

struct FArcTerrainStreamLaneCache
{
	TArray<ArcTerrainSubsystemPrivate::FSectorWindow> InnerWindows;
	TArray<ArcTerrainSubsystemPrivate::FSectorWindow> OuterWindows;
	TSet<FIntPoint> RequestedTiles;
	TSet<FIntPoint> RetainedTiles;
	double TileSize = 0.0;
	double StreamRadiusCm = 0.0;
	double StreamUnloadRadiusCm = 0.0;
	bool bGridValid = false;
};

struct FArcTerrainStreamPendingRelease
{
	EArcTerrainRail Rail = EArcTerrainRail::Height;
	int32 Tier = 0;
	FIntPoint TileKey = FIntPoint::ZeroValue;

	bool operator==(const FArcTerrainStreamPendingRelease& Other) const
	{
		return Rail == Other.Rail && Tier == Other.Tier && TileKey == Other.TileKey;
	}
};

struct FArcTerrainStreamCache
{
	bool bValid = false;
	bool bHadInvokers = false;
	bool bHasTransformKey = false;
	FVector CachedLocation = FVector::ZeroVector;
	FQuat CachedRotation = FQuat::Identity;
	FVector CachedScale = FVector(1.0, 1.0, 1.0);
	uint64 ConsumedStreamLayoutRevision = 0;
	uint64 ConsumedDerivedGridRevision = 0;
	uint64 ConsumedResidentSetRevision = 0;
	uint32 ConsumedManifestCoverageEpoch = 0;
	uint32 ConsumedLoadedCoverageEpoch = 0;
	FArcTerrainStreamLaneCache Lanes[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers];
	TArray<FArcTerrainStreamPendingRelease> PendingReleaseKeys;
	double NextReleaseRetryTime = 0.0;
};
