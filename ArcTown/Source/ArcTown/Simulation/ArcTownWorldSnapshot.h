// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcTownWorldSnapshot.generated.h"

struct FArcTownSiteSnapshot;

USTRUCT()
struct ARCTOWN_API FArcTownRoadNode
{
	GENERATED_BODY()

	UPROPERTY()
	FVector2f LocalXY = FVector2f::ZeroVector;

	UPROPERTY()
	uint8 bFinished = 0;

	UPROPERTY()
	int32 FailedExpansions = 0;
};

USTRUCT()
struct ARCTOWN_API FArcTownRoadEdge
{
	GENERATED_BODY()

	UPROPERTY()
	int32 NodeA = INDEX_NONE;

	UPROPERTY()
	int32 NodeB = INDEX_NONE;

	/** The one tracer run that laid this edge; every edge of that road carries it, splits included. */
	UPROPERTY()
	int32 RoadId = INDEX_NONE;

	UPROPERTY()
	uint8 bBuilt = 0;

	UPROPERTY()
	uint8 Hierarchy = 0;

	/** One span of water crossed in a single edge; it carries no terrain cut, so it is never a chain of steps. */
	UPROPERTY()
	uint8 bBridge = 0;

	UPROPERTY()
	float HalfWidth = 160.0f;

	UPROPERTY()
	float Traffic = 0.0f;
};

/** A road end laid down but not yet traced: the node it leaves, the heading it leaves on, and the field that road will follow. */
USTRUCT()
struct ARCTOWN_API FArcTownRoadBranchSeed
{
	GENERATED_BODY()

	UPROPERTY()
	int32 NodeIndex = INDEX_NONE;

	UPROPERTY()
	double HeadingRad = 0.0;

	UPROPERTY()
	uint8 bFollowsMinorField = 0;
};

USTRUCT()
struct ARCTOWN_API FArcTownBuildingRecord
{
	GENERATED_BODY()

	/** Index into FArcTownSimulationInput::Catalog, which is parallel to UArcTownBuildingCatalog::Buildings. */
	UPROPERTY()
	int32 CatalogIndex = INDEX_NONE;

	UPROPERTY()
	FVector2f LocalXY = FVector2f::ZeroVector;

	UPROPERTY()
	float Yaw = 0.0f;

	UPROPERTY()
	int32 BirthYear = 0;

	UPROPERTY()
	int32 EdgeIndex = INDEX_NONE;
};

/** No field is editor-visible or Blueprint-readable: a Details row per record stalls the editor. */
USTRUCT()
struct ARCTOWN_API FArcTownWorldSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 RuleVersion = 0;

	UPROPERTY()
	FGuid SimulationId;

	UPROPERTY()
	int32 Seed = 0;

	UPROPERTY()
	uint64 SiteSnapshotHash = 0;

	UPROPERTY()
	uint64 SettingsHash = 0;

	UPROPERTY()
	int32 Year = 0;

	UPROPERTY()
	uint64 StateChecksum = 0;

	UPROPERTY()
	int32 NextRoadId = 0;

	UPROPERTY()
	TArray<FArcTownRoadNode> Nodes;

	UPROPERTY()
	TArray<FArcTownRoadEdge> Edges;

	UPROPERTY()
	TArray<FArcTownBuildingRecord> Buildings;

	UPROPERTY()
	TArray<FArcTownRoadBranchSeed> PendingBranchSeeds;

	int32 SplitEdgeAtPoint(int32 EdgeIndex, FVector2f LocalXY);

	/** One transaction: each remap consumes the permutation the previous sort produced. */
	void CanonicalizeAndRefreshChecksum();

	bool Validate(const FArcTownSiteSnapshot& Site, FText& OutError) const;
};
