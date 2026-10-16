// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DebugDraw.h"
#include "Simulation/ArcEcotopeFrozenSiteSource.h"
#include "Simulation/ArcEcotopeSiteChunkReader.h"
#include "Simulation/ArcEcotopeSiteMetadata.h"

#include "ArcForestDebugDrawTypes.generated.h"

class AArcEcotopeVolume;
class AArcClimateVolume;
class AArcTerrain;

UENUM(BlueprintType)
enum class EArcForestDebugStandColor : uint8
{
	Species,
	LifeStatus,
	Vigor,
	Age,
};

UENUM(BlueprintType)
enum class EArcForestDisturbancePreviewKind : uint8
{
	Fire,
	Wind,
};

struct FArcForestDebugDrawSiteReaderCache
{
#if WITH_EDITOR
	struct FEntry
	{
		uint64 SiteSnapshotHash = 0;
		FArcEcotopeFrozenSiteSourcePtr FrozenSource;
		FArcEcotopeSiteChunkReader Reader;
		bool bOpenFailed = false;
	};

	TMap<TWeakObjectPtr<const AArcEcotopeVolume>, FEntry> Entries;
#endif

	void Reset()
	{
#if WITH_EDITOR
		Entries.Reset();
#endif
	}
};

struct FArcForestDebugDrawGeometryKey
{
	FGuid VolumeId;
	FTransform VolumeTransform;
	uint64 SiteSnapshotHash = 0;
	int32 MaxDrawn = 0;
	uint8 Field = 0;
	uint8 RecordGridLevel = 0;
	bool bDrawLabels = false;
	int32 Stride = 0;
	float CellScale = 1.0f;
	double SheetLocalZCm = 0.0;
	int32 Seed = 0;
	int32 PreviewYear = 0;
	int32 PreviewSearchYears = 0;
	uint8 PreviewKind = 0;
	uint32 NoiseParamsHash = 0;
	uint32 DisturbanceSettingsHash = 0;
	uint64 StateChecksum = 0;
	uint32 SpeciesFilterHash = 0;
	uint8 StandColorMode = 0;
	bool bDrawIndividualCrowns = false;
	bool bDrawGroundCoverSpecies = false;
	float DebugLineThicknessCm = 0.0f;
	uint8 TerrainDataMap = 0;
	float TerrainDataMapGain = 1.0f;
	bool bSiteFieldGreyscale = false;
	FBox SheetLocalBounds = FBox(EForceInit::ForceInit);
	float SheetCellSizeCm = 0.0f;
	FIntPoint SheetGridDimensions = FIntPoint::ZeroValue;
	TWeakObjectPtr<const AArcTerrain> Terrain;
	FTransform TerrainTransform;

	bool Matches(const FArcForestDebugDrawGeometryKey& Other) const
	{
		return VolumeId == Other.VolumeId
			&& SiteSnapshotHash == Other.SiteSnapshotHash
			&& MaxDrawn == Other.MaxDrawn
			&& Field == Other.Field
			&& RecordGridLevel == Other.RecordGridLevel
			&& bDrawLabels == Other.bDrawLabels
			&& Stride == Other.Stride
			&& CellScale == Other.CellScale
			&& SheetLocalZCm == Other.SheetLocalZCm
			&& Seed == Other.Seed
			&& PreviewYear == Other.PreviewYear
			&& PreviewSearchYears == Other.PreviewSearchYears
			&& PreviewKind == Other.PreviewKind
			&& NoiseParamsHash == Other.NoiseParamsHash
			&& DisturbanceSettingsHash == Other.DisturbanceSettingsHash
			&& StateChecksum == Other.StateChecksum
			&& SpeciesFilterHash == Other.SpeciesFilterHash
			&& StandColorMode == Other.StandColorMode
			&& bDrawIndividualCrowns == Other.bDrawIndividualCrowns
			&& bDrawGroundCoverSpecies == Other.bDrawGroundCoverSpecies
			&& DebugLineThicknessCm == Other.DebugLineThicknessCm
			&& TerrainDataMap == Other.TerrainDataMap
			&& TerrainDataMapGain == Other.TerrainDataMapGain
			&& bSiteFieldGreyscale == Other.bSiteFieldGreyscale
			&& SheetLocalBounds == Other.SheetLocalBounds
			&& SheetCellSizeCm == Other.SheetCellSizeCm
			&& SheetGridDimensions == Other.SheetGridDimensions
			&& Terrain == Other.Terrain
			&& TerrainTransform.Equals(Other.TerrainTransform)
			&& VolumeTransform.Equals(Other.VolumeTransform);
	}
};

struct FArcForestDebugDrawCachedTriangle
{
	FVector V0 = FVector::ZeroVector;
	FVector V1 = FVector::ZeroVector;
	FVector V2 = FVector::ZeroVector;
	FColor Color = FColor::White;
};

struct FArcForestDebugDrawMeshBatch
{
	FColor Color = FColor::White;
	UE::DebugDraw::EDepthTest Depth = UE::DebugDraw::EDepthTest::WorldDepth;
	TArray<FVector> Vertices;
	TArray<int32> Indices;
};

struct FArcForestDebugDrawCachedLabel
{
	FVector WorldLocation = FVector::ZeroVector;
	FColor Color = FColor::White;
	FString Text;
};

struct FArcForestDebugDrawGeometryCache
{
	struct FSiteFieldEntry
	{
		FArcForestDebugDrawGeometryKey Key;
		TArray<FArcForestDebugDrawMeshBatch> Batches;
		TMap<uint64, int32> BatchIndexByKey;
		TArray<FArcForestDebugDrawCachedLabel> Labels;
	};

	struct FDisturbancePreviewEntry
	{
		FArcForestDebugDrawGeometryKey Key;
		TArray<FArcForestDebugDrawCachedTriangle> Triangles;
		TArray<FArcForestDebugDrawCachedLabel> Labels;
	};

	struct FTerrainDataMapEntry
	{
		FArcForestDebugDrawGeometryKey Key;
		TArray<FArcForestDebugDrawMeshBatch> Batches;
		TArray<FArcForestDebugDrawCachedLabel> Labels;
		double LastBuildSeconds = 0.0;
		int32 UnreadCellCount = 0;
		int32 UnreadRetryPlateauCount = 0;
		bool bUnreadRetryStalled = false;
	};

	struct FChunkGridEntry
	{
		FArcForestDebugDrawGeometryKey Key;
		TArray<UE::DebugDraw::FLineSubmission> Lines;
		TArray<FArcForestDebugDrawCachedLabel> Labels;
	};

	struct FStandEntry
	{
		FArcForestDebugDrawGeometryKey Key;
		TArray<FArcForestDebugDrawMeshBatch> Batches;
		TMap<uint64, int32> BatchIndexByKey;
		TArray<FArcForestDebugDrawCachedLabel> Labels;
	};

	TMap<TWeakObjectPtr<const AArcEcotopeVolume>, FSiteFieldEntry> SiteFieldEntries;
	TMap<TWeakObjectPtr<const AArcClimateVolume>, FSiteFieldEntry> ClimateFieldEntries;
	TMap<TWeakObjectPtr<const AArcEcotopeVolume>, FDisturbancePreviewEntry> DisturbancePreviewEntries;
	TMap<TWeakObjectPtr<const AArcEcotopeVolume>, FTerrainDataMapEntry> TerrainDataMapEntries;
	TMap<TWeakObjectPtr<const AArcEcotopeVolume>, FChunkGridEntry> ChunkGridEntries;
	TMap<TWeakObjectPtr<const AArcEcotopeVolume>, FStandEntry> IndividualEntries;
	TMap<TWeakObjectPtr<const AArcEcotopeVolume>, FStandEntry> DeadwoodEntries;
	TMap<TWeakObjectPtr<const AArcEcotopeVolume>, FStandEntry> CohortEntries;
	TMap<TWeakObjectPtr<const AArcEcotopeVolume>, FStandEntry> PatchEntries;

	void Reset()
	{
		SiteFieldEntries.Reset();
		ClimateFieldEntries.Reset();
		DisturbancePreviewEntries.Reset();
		TerrainDataMapEntries.Reset();
		ChunkGridEntries.Reset();
		IndividualEntries.Reset();
		DeadwoodEntries.Reset();
		CohortEntries.Reset();
		PatchEntries.Reset();
	}
};

struct FArcForestDebugDrawContext
{
	const AArcEcotopeVolume* Volume = nullptr;
	FTransform VolumeTransform;
	double UniformScale = 1.0;
	FArcEcotopeSiteMetadata Metadata;
	bool bMetadataValid = false;
};
