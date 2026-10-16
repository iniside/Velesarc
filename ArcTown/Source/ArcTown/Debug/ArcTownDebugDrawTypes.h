// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DebugDraw.h"
#include "Simulation/ArcTownSimulationInput.h"
#include "Simulation/ArcTownSiteFields.h"

#include "ArcTownDebugDrawTypes.generated.h"

class AArcTownVolume;

UENUM(BlueprintType)
enum class EArcTownDebugSiteField : uint8
{
	Slope,
	DistanceToWater,
	HeightAboveChannel,
	Buildable,
	ChannelMask,
};

UENUM(BlueprintType)
enum class EArcTownDebugRoadColor : uint8
{
	BuiltPlanned,
	Traffic,
	Hierarchy,
};

UENUM(BlueprintType)
enum class EArcTownDebugBuildingColor : uint8
{
	BirthYear,
	CatalogEntry,
	Frontage,
};

/**
 * Decides when the overlay refreezes the simulation input, which deep-copies a site snapshot of up to a
 * million samples: every field is one the freeze reads, and none is a per-layer draw knob.
 */
struct FArcTownDebugDrawInputKey
{
	FGuid TownId;

	/** Freezing resolves the volume box into town-local bounds, so moving or resizing it moves the frozen world. */
	FTransform BoundsTransform;

	uint64 SiteSnapshotHash = 0;

	/** The freeze copies the simulation state as its source snapshot, so a completed run is a new frozen world. */
	uint64 StateChecksum = 0;

	uint32 SettingsHash = 0;

	uint32 AttractorsHash = 0;

	/** A freeze fails outright on a missing or empty catalog; this lets a fixed catalog clear that failure. */
	uint32 CatalogHash = 0;

	int32 Seed = 0;

	bool MatchesIgnoringBounds(const FArcTownDebugDrawInputKey& Other) const
	{
		return TownId == Other.TownId
			&& SiteSnapshotHash == Other.SiteSnapshotHash
			&& StateChecksum == Other.StateChecksum
			&& SettingsHash == Other.SettingsHash
			&& AttractorsHash == Other.AttractorsHash
			&& CatalogHash == Other.CatalogHash
			&& Seed == Other.Seed;
	}

	bool Matches(const FArcTownDebugDrawInputKey& Other) const
	{
		return MatchesIgnoringBounds(Other) && BoundsTransform.Equals(Other.BoundsTransform);
	}
};

struct FArcTownDebugDrawGeometryKey
{
	FArcTownDebugDrawInputKey Input;
	uint8 Field = 0;

	/** Selector of the layer's colour mode; the colour is baked into the cached geometry, not applied at draw. */
	uint8 ColorMode = 0;

	int32 Stride = 0;
	int32 MaxDrawn = 0;
	float CellScale = 1.0f;
	double SurfaceLiftCm = 0.0;
	float LineThicknessCm = 0.0f;

	/** Alpha of a filled layer is baked into its cached vertex colour, so opacity rebuilds the geometry. */
	float OverlayOpacityPercent = 100.0f;

	bool Matches(const FArcTownDebugDrawGeometryKey& Other) const
	{
		return Input.Matches(Other.Input)
			&& Field == Other.Field
			&& ColorMode == Other.ColorMode
			&& Stride == Other.Stride
			&& MaxDrawn == Other.MaxDrawn
			&& CellScale == Other.CellScale
			&& SurfaceLiftCm == Other.SurfaceLiftCm
			&& LineThicknessCm == Other.LineThicknessCm
			&& OverlayOpacityPercent == Other.OverlayOpacityPercent;
	}
};

struct FArcTownDebugDrawMeshBatch
{
	FColor Color = FColor::White;
	UE::DebugDraw::EDepthTest Depth = UE::DebugDraw::EDepthTest::WorldDepth;
	TArray<FVector> Vertices;
	TArray<int32> Indices;
};

struct FArcTownDebugDrawPoint
{
	FVector Location = FVector::ZeroVector;
	FColor Color = FColor::White;
	float ScreenSizePx = 0.0f;
};

struct FArcTownDebugDrawBox
{
	FVector Center = FVector::ZeroVector;
	FVector Extent = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	FColor Color = FColor::White;
};

namespace ArcTownDebugDrawCache
{
	template <typename EntryType>
	void RemoveEntriesOutside(
		TMap<TWeakObjectPtr<const AArcTownVolume>, EntryType>& Entries,
		const TSet<TWeakObjectPtr<const AArcTownVolume>>& LiveVolumes)
	{
		TArray<TWeakObjectPtr<const AArcTownVolume>> StaleKeys;
		for (const TPair<TWeakObjectPtr<const AArcTownVolume>, EntryType>& Pair : Entries)
		{
			if (!Pair.Key.IsValid() || !LiveVolumes.Contains(Pair.Key))
			{
				StaleKeys.Add(Pair.Key);
			}
		}

		for (const TWeakObjectPtr<const AArcTownVolume>& StaleKey : StaleKeys)
		{
			Entries.Remove(StaleKey);
		}
	}
}

struct FArcTownDebugDrawFrozenCache
{
	struct FEntry
	{
		FArcTownDebugDrawInputKey Key;
		FArcTownSimulationInput Input;
		FArcTownSiteFields Fields;
		FVector2f CoreTownLocalXY = FVector2f::ZeroVector;

		/** Bounds transform the previous tick saw while only the bounds differ from Key. */
		FTransform PendingBoundsTransform;
		bool bBoundsMovePending = false;

		bool bAttempted = false;
		bool bInputFrozen = false;
		bool bFieldsBuilt = false;
		bool bCoreChosen = false;
	};

	TMap<TWeakObjectPtr<const AArcTownVolume>, FEntry> Entries;

	void Prune(const TSet<TWeakObjectPtr<const AArcTownVolume>>& LiveVolumes)
	{
		ArcTownDebugDrawCache::RemoveEntriesOutside(Entries, LiveVolumes);
	}

	void Reset()
	{
		Entries.Reset();
	}
};

struct FArcTownDebugDrawGeometryCache
{
	struct FSiteFieldEntry
	{
		FArcTownDebugDrawGeometryKey Key;
		TArray<FArcTownDebugDrawMeshBatch> Batches;
		TMap<uint64, int32> BatchIndexByKey;
		double MinValue = 0.0;
		double MaxValue = 0.0;
		int32 DrawnCellCount = 0;
	};

	struct FTensorFieldEntry
	{
		FArcTownDebugDrawGeometryKey Key;
		TArray<UE::DebugDraw::FLineSubmission> Lines;
		FVector CoreWorld = FVector::ZeroVector;
		double CoreRadiusCm = 0.0;
		int32 DrawnSampleCount = 0;
	};

	struct FRoadEntry
	{
		FArcTownDebugDrawGeometryKey Key;
		TArray<FArcTownDebugDrawMeshBatch> Batches;
		TMap<uint64, int32> BatchIndexByKey;
		TArray<FArcTownDebugDrawPoint> Nodes;
		double MinTraffic = 0.0;
		double MaxTraffic = 0.0;
		double DrapeSpacingCm = 0.0;
		int32 DrawnEdgeCount = 0;
		int32 BuiltEdgeCount = 0;
		int32 SkippedEdgeCount = 0;
		int32 DroppedEdgeCount = 0;
		int32 DrawnTriangleCount = 0;
		int32 FinishedNodeCount = 0;
	};

	struct FBuildingEntry
	{
		FArcTownDebugDrawGeometryKey Key;
		TArray<FArcTownDebugDrawBox> Boxes;

		/** Only the out-of-range catalog sentinel draws lines; a placed building is a box. */
		TArray<UE::DebugDraw::FLineSubmission> Lines;
		int32 MinBirthYear = 0;
		int32 MaxBirthYear = 0;
		int32 DrawnBuildingCount = 0;
		int32 UnknownCatalogCount = 0;
		int32 FirstUnknownCatalogIndex = INDEX_NONE;
		int32 OrphanedFrontageCount = 0;
		int32 DroppedBuildingCount = 0;
	};

	struct FAttractorEntry
	{
		FArcTownDebugDrawGeometryKey Key;
		TArray<UE::DebugDraw::FLineSubmission> Lines;
		int32 DrawnAttractorCount = 0;
		int32 SkippedAttractorCount = 0;
	};

	struct FBlockEntry
	{
		FArcTownDebugDrawGeometryKey Key;
		TArray<UE::DebugDraw::FLineSubmission> Lines;
		int32 DrawnBlockCount = 0;
		int32 DrawnFrontageCount = 0;
		int32 DroppedBlockCount = 0;
		int32 DroppedFrontageCount = 0;
	};

	TMap<TWeakObjectPtr<const AArcTownVolume>, FSiteFieldEntry> SiteFieldEntries;
	TMap<TWeakObjectPtr<const AArcTownVolume>, FTensorFieldEntry> TensorFieldEntries;
	TMap<TWeakObjectPtr<const AArcTownVolume>, FRoadEntry> RoadEntries;
	TMap<TWeakObjectPtr<const AArcTownVolume>, FBuildingEntry> BuildingEntries;
	TMap<TWeakObjectPtr<const AArcTownVolume>, FAttractorEntry> AttractorEntries;
	TMap<TWeakObjectPtr<const AArcTownVolume>, FBlockEntry> BlockEntries;

	void Prune(const TSet<TWeakObjectPtr<const AArcTownVolume>>& LiveVolumes)
	{
		ArcTownDebugDrawCache::RemoveEntriesOutside(SiteFieldEntries, LiveVolumes);
		ArcTownDebugDrawCache::RemoveEntriesOutside(TensorFieldEntries, LiveVolumes);
		ArcTownDebugDrawCache::RemoveEntriesOutside(RoadEntries, LiveVolumes);
		ArcTownDebugDrawCache::RemoveEntriesOutside(BuildingEntries, LiveVolumes);
		ArcTownDebugDrawCache::RemoveEntriesOutside(AttractorEntries, LiveVolumes);
		ArcTownDebugDrawCache::RemoveEntriesOutside(BlockEntries, LiveVolumes);
	}

	void Reset()
	{
		SiteFieldEntries.Reset();
		TensorFieldEntries.Reset();
		RoadEntries.Reset();
		BuildingEntries.Reset();
		AttractorEntries.Reset();
		BlockEntries.Reset();
	}
};
