// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcTerrainInvokers.h"
#include "ArcTerrainTypes.h"
#include "CoreMinimal.h"
#include "Foliage/ArcEcotopeFoliageFields.h"
#include "Foliage/ArcEcotopeFoliagePrimitive.h"
#include "Foliage/ArcEcotopeGroundFoliage.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"

#include "ArcEcotopeFoliageSubsystem.generated.h"

class AActor;
class AArcEcotopeVolume;
class AArcTerrain;
class UArcEcotopeGroundFoliage;
class UArcTerrainSubsystem;
class ULevel;
class UMaterialInterface;
class UStaticMesh;

/**
 * Identity of the BYTES one terrain cell contributed. PayloadId, not the payload's address: a released
 * payload's address goes straight back to the next one, so an address-keyed signature would compare
 * equal across a reload and the patch would keep drawing the tile it no longer holds.
 *
 * A zero weight payload id means the cell's weight tile was not loaded — a state the patch has to
 * re-dispatch out of once it is.
 */
struct FArcEcotopeFoliageCellSignature
{
	FArcTerrainCellKey Key = FArcTerrainCellKey::ZeroValue;
	FIntPoint HeightTileKey = FIntPoint::ZeroValue;
	uint32 HeightPayloadId = 0;
	FIntPoint WeightTileKey = FIntPoint::ZeroValue;
	uint32 WeightPayloadId = 0;

	bool operator==(const FArcEcotopeFoliageCellSignature& Other) const
	{
		return Key == Other.Key && HeightTileKey == Other.HeightTileKey
			&& HeightPayloadId == Other.HeightPayloadId && WeightTileKey == Other.WeightTileKey
			&& WeightPayloadId == Other.WeightPayloadId;
	}
};

/**
 * The dispatch inputs a patch re-compares every tick: the terrain, the lattice, the authored scatter
 * parameters the kernel reads and the resident cell list. That is what makes a terrain cell streaming
 * in — or an edited property — visible to a patch whose observer is standing still. A changed mesh,
 * material override or render setting is handled by patch recreation in FindOrCreatePatch, not here.
 */
struct FArcEcotopeFoliageSignature
{
	FObjectKey Terrain;
	FIntPoint RegionOriginCell = FIntPoint::ZeroValue;
	int32 GridX = 0;
	int32 GridY = 0;
	float GridStepCm = 0.0f;
	int32 MeshCount = 0;

	/** Authored, not the clamped values passed to the kernel: bitwise inequality is "the author edited it". */
	float MinScale = 0.0f;
	float MaxScale = 0.0f;
	float MaxYawDeg = 0.0f;
	float MaxSlopeDegrees = 0.0f;
	float MinLayerWeight = 0.0f;
	bool bAlignToNormal = false;
	FName TerrainLayerChannel;

	float UnderstoryShade = 0.0f;
	float ShadePreference = 0.0f;
	float ShadeStrength = 0.0f;
	float MinMoisture = 0.0f;
	float MinFertility = 0.0f;

	/**
	 * Compared for inequality only. UArcForestSimulationState::SnapshotGeneration is Transient but
	 * transactional, so undoing a Clear State restores an older value and the counter moves backwards.
	 */
	uint64 SnapshotGeneration = 0;

	/** Revision of the volume's forest field texture; zero when the volume has no valid snapshot. */
	uint32 FieldRevision = 0;

	/** Ascending by cell key, so two gathers of the same residency compare equal. */
	TArray<FArcEcotopeFoliageCellSignature> Cells;

	bool operator==(const FArcEcotopeFoliageSignature& Other) const
	{
		return Terrain == Other.Terrain
			&& RegionOriginCell == Other.RegionOriginCell
			&& GridX == Other.GridX
			&& GridY == Other.GridY
			&& GridStepCm == Other.GridStepCm
			&& MeshCount == Other.MeshCount
			&& MinScale == Other.MinScale
			&& MaxScale == Other.MaxScale
			&& MaxYawDeg == Other.MaxYawDeg
			&& MaxSlopeDegrees == Other.MaxSlopeDegrees
			&& MinLayerWeight == Other.MinLayerWeight
			&& bAlignToNormal == Other.bAlignToNormal
			&& TerrainLayerChannel == Other.TerrainLayerChannel
			&& UnderstoryShade == Other.UnderstoryShade
			&& ShadePreference == Other.ShadePreference
			&& ShadeStrength == Other.ShadeStrength
			&& MinMoisture == Other.MinMoisture
			&& MinFertility == Other.MinFertility
			&& SnapshotGeneration == Other.SnapshotGeneration
			&& FieldRevision == Other.FieldRevision
			&& Cells == Other.Cells;
	}
};

/** One GPU-only primitive, its lattice and the signature it was last dispatched for. */
struct FArcEcotopeFoliagePatch
{
	TWeakObjectPtr<AArcEcotopeVolume> Volume;
	TWeakObjectPtr<UArcEcotopeGroundFoliage> Foliage;
	int32 EntryIndex = INDEX_NONE;
	int32 MeshIndex = INDEX_NONE;

	FObjectKey Mesh;
	TArray<FObjectKey> MaterialOverrides;
	FArcEcotopeFoliageRenderSettings RenderSettings;

	TUniquePtr<FArcEcotopeFoliagePrimitive> Primitive;

	/** Shared with the owning volume's cache entry; null while the volume carries no valid snapshot. */
	FArcEcotopeFoliageFieldRef Field;

	FArcEcotopeFoliageSignature Signature;

	/**
	 * "Never dispatched". A separate flag rather than a zero Signature: PIE duplication skips
	 * DuplicateTransient and PostDuplicate skips ResetToEmpty, so a duplicated volume carries a live
	 * snapshot at generation 0.
	 */
	bool bHasSignature = false;

	bool bTouchedThisTick = false;

	bool bCapWarningLogged = false;
};

/**
 * Drives one GPU-scattered ground foliage patch per (volume, foliage entry) around the invoker
 * positions, in editor and play worlds alike.
 */
UCLASS()
class ARCECOTOPE_API UArcEcotopeFoliageSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem / FTickableGameObject interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool IsTickableInEditor() const override { return true; }
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End USubsystem / FTickableGameObject interface

	int32 GetNumPatches() const { return Patches.Num(); }

private:
	void HandleLevelChanged(ULevel* Level, UWorld* InWorld);
#if WITH_EDITOR
	void HandleLevelActorChanged(AActor* Actor);
	void HandleLevelActorListChanged();
	void HandleMaterialCompilationFinished(UMaterialInterface* Material);

	/** Drops every primitive so the next tick rebuilds it against the now-complete materials. */
	void RebuildPatchPrimitives();
#endif

	void RefreshActorCache(UWorld& World);
	void MarkActorCacheDirty();

	AArcTerrain* SelectTerrain(const FBox& RegionWorldBounds) const;

	FArcEcotopeFoliagePatch& FindOrCreatePatch(AArcEcotopeVolume& Volume, int32 EntryIndex, int32 MeshIndex,
		UArcEcotopeGroundFoliage& Foliage);

	void UpdatePatch(FArcEcotopeFoliagePatch& Patch, AArcEcotopeVolume& Volume,
		const UArcEcotopeGroundFoliage& Foliage, AArcTerrain& Terrain,
		const FIntPoint& RegionOriginCell, const FIntPoint& GridSize, float GridStepCm, const FBox& RegionWorldBounds);

	/**
	 * The field is per volume, not per foliage entry, so several entries on one volume share one raster
	 * and one upload. Never derived here: the payload is produced when a snapshot is committed, and a
	 * volume without one draws no ground foliage.
	 */
	FArcEcotopeFoliageFieldRef FindField(AArcEcotopeVolume& Volume);

	void DestroyUntouchedPatches();
	void ReleaseFieldsOfDeadVolumes();
	void DestroyAllPatches();
	void ReleaseAllFields();

	TArray<TUniquePtr<FArcEcotopeFoliagePatch>> Patches;

	struct FArcEcotopeFoliageVolumeField
	{
		TWeakObjectPtr<AArcEcotopeVolume> Volume;
		FArcEcotopeFoliageFieldRef Field;

		/** Identity of the payload this field was uploaded from. */
		uint64 SourceSiteSnapshotHash = 0;
		uint64 SourceStateChecksum = 0;

		/** The transform the bound matrices were split from; a moved volume re-splits, never re-uploads. */
		FTransform UploadedTransform = FTransform::Identity;

		/** Compared for inequality, so "never uploaded" cannot be a payload identity. */
		bool bHasField = false;

		/** One line per volume, not one per tick. */
		bool bMissingPayloadLogged = false;
	};

	TMap<FObjectKey, FArcEcotopeFoliageVolumeField> VolumeFields;

	uint32 NextFieldRevision = 1;

	TArray<TWeakObjectPtr<AArcEcotopeVolume>> CachedVolumes;
	TArray<TWeakObjectPtr<AArcTerrain>> CachedTerrains;
	bool bActorCacheDirty = true;

	TArray<FVector> InvokerPositions;
	FArcTerrainInvokerCache InvokerCache;

	FDelegateHandle LevelAddedHandle;
	FDelegateHandle LevelRemovedHandle;

#if WITH_EDITOR
	FDelegateHandle ActorAddedHandle;
	FDelegateHandle ActorDeletedHandle;
	FDelegateHandle ActorListChangedHandle;
	FDelegateHandle MaterialCompilationFinishedHandle;

	/** Set from the compilation delegate and consumed in Tick: the rebuild is game-thread work. */
	bool bMaterialRebuildPending = false;

	/** Seconds spent waiting for the shader compiler to go idle before that pending rebuild runs. */
	float MaterialRebuildWaitSeconds = 0.0f;
#endif
};
