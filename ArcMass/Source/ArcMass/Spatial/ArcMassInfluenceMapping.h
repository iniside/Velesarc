// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "MassEntityTraitBase.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Mass/Processor.h"
#include "Engine/DeveloperSettings.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcMassInfluenceMapping.generated.h"

class UBoxComponent;
class USphereComponent;

// ---------------------------------------------------------------------------
// Developer Settings
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EArcInfluenceStorage : uint8
{
	Sparse,
	/** Fixed-size tiles of per-team visibility bits. Carries no float channels and no decay. */
	Tiled
};

USTRUCT(BlueprintType)
struct ARCMASS_API FArcInfluenceGridConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Influence", meta = (ClampMin = "1.0"))
	float CellSize = 1000.f;

	UPROPERTY(EditAnywhere, Category = "Influence", meta = (ClampMin = "1"))
	int32 NumChannels = 1;

	UPROPERTY(EditAnywhere, Category = "Influence", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DecayRate = 0.05f;

	/** Time in seconds between decay ticks. 0 means every frame. */
	UPROPERTY(EditAnywhere, Category = "Influence", meta = (ClampMin = "0.0"))
	float DecayInterval = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Influence", meta = (ClampMin = "1"))
	int32 MaxCellsPerUpdate = 64;

	UPROPERTY(EditAnywhere, Category = "Influence")
	bool bIs2D = true;

	UPROPERTY(EditAnywhere, Category = "Influence")
	bool bIsSemiStatic = false;

	UPROPERTY(EditAnywhere, Category = "Influence")
	EArcInfluenceStorage Storage = EArcInfluenceStorage::Sparse;

	/** Cells per tile side. Tiled storage only. */
	UPROPERTY(EditAnywhere, Category = "Influence", meta = (ClampMin = "8", ClampMax = "256"))
	int32 TileDim = 64;

	/** Live tile cap; allocations past it are refused. Zero disables tiled allocation entirely. */
	UPROPERTY(EditAnywhere, Category = "Influence", meta = (ClampMin = "0"))
	int32 MaxTiles = 1024;
};

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Arc Influence Mapping"))
class ARCMASS_API UArcInfluenceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(config, EditAnywhere, Category = "Grids")
	TArray<FArcInfluenceGridConfig> Grids;

	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return FName(TEXT("Arc Game")); }
	virtual FName GetSectionName() const override { return FName(TEXT("Arc Influence Mapping")); }
};

// ---------------------------------------------------------------------------
// Core data types
// ---------------------------------------------------------------------------

struct ARCMASS_API FArcInfluenceEntry
{
	float Strength = 0.f;
	FMassEntityHandle Source;
};

struct ARCMASS_API FArcInfluenceCell
{
	TArray<TArray<FArcInfluenceEntry>> ChannelEntries;

	void Init(int32 NumChannels);

	float GetTotalInfluence(int32 Channel) const;
};

struct ARCMASS_API FArcPendingInfluence
{
	FIntVector GridCoords;
	int32 Channel = 0;
	float Strength = 0.f;
	FMassEntityHandle Source;
};

struct ARCMASS_API FArcInfluenceGrid
{
	TMap<FIntVector, FArcInfluenceCell> Cells;
	FArcInfluenceGridConfig Settings;
	int32 DecayCursor = 0;
	float DecayAccumulator = 0.f;

	FIntVector WorldToGrid(const FVector& WorldPos) const;
	FArcInfluenceCell& FindOrAddCell(const FIntVector& GridCoords);

	void AddInfluence(const FIntVector& GridCoords, int32 Channel, float Strength, FMassEntityHandle Source);
	void AddInfluenceBatch(TConstArrayView<FArcPendingInfluence> Batch);

	float QueryInfluence(const FVector& WorldPos, int32 Channel) const;
	float QueryInfluenceInRadius(const FVector& Center, float Radius, int32 Channel) const;

	void SetCellInfluence(const FIntVector& GridCoords, int32 Channel, float Strength);

	void DecayStep(int32 MaxCells);
	void TickDecay(float DeltaTime);
};

// ---------------------------------------------------------------------------
// Tiled storage
// ---------------------------------------------------------------------------

struct ARCMASS_API FArcInfluenceBitplane
{
	TArray<uint64> Words;

	void Init(int32 NumWords);
	void ClearBits();
	bool IsEmpty() const;

	bool GetBit(int32 CellIndex) const
	{
		return (Words[CellIndex >> 6] & (uint64(1) << (CellIndex & 63))) != 0;
	}

	void SetBit(int32 CellIndex)
	{
		Words[CellIndex >> 6] |= uint64(1) << (CellIndex & 63);
	}

	void OrWord(int32 WordIndex, uint64 Mask)
	{
		Words[WordIndex] |= Mask;
	}
};

struct ARCMASS_API FArcInfluenceTile
{
	TMap<int32, FArcInfluenceBitplane> FrontPlanes;
	TMap<int32, FArcInfluenceBitplane> BackPlanes;
	FArcInfluenceBitplane Occluders;
	int32 NumWords = 0;
	int32 RefCount = 0;
	/** Set until the tile has lived through one whole rebuild cycle; its front buffer stays unpublished until then. */
	bool bJoinsNextCycle = true;

	void Init(int32 InNumWords);

	const FArcInfluenceBitplane* FindFrontPlane(int32 TeamId) const;
	FArcInfluenceBitplane& FindOrAddBackPlane(int32 TeamId);

	bool IsVisibleToTeam(int32 TeamId, int32 CellIndex) const;
	bool IsVisibleToAnyTeam(int32 CellIndex) const;
	/** True when a team other than TeamId sees the cell; TeamId's own plane and its allies-of-the-same-id are ignored. */
	bool IsVisibleToOtherTeam(int32 TeamId, int32 CellIndex) const;

	void ClearBack();
	void PublishBack();
};

struct ARCMASS_API FArcInfluenceTiledGrid
{
	TMap<FIntPoint, FArcInfluenceTile> Tiles;
	/** Tiles whose occluder bitmap needs rasterizing. Drained outside ArcMass. */
	TArray<FIntPoint> OccluderDirtyTiles;
	FArcInfluenceGridConfig Settings;
	int32 RefusedTileAllocations = 0;

	int32 GetNumWordsPerPlane() const;
	float GetTileWorldSize() const;

	FIntPoint WorldToCell(const FVector& WorldPos) const;
	FIntPoint WorldToTile(const FVector& WorldPos) const;
	FIntPoint CellToTile(const FIntPoint& Cell) const;
	int32 CellIndexInTile(const FIntPoint& Cell) const;
	FVector GetCellCenter(const FIntPoint& Cell, double WorldZ) const;

	FArcInfluenceTile* FindTile(const FIntPoint& TileCoord);
	const FArcInfluenceTile* FindTile(const FIntPoint& TileCoord) const;
	FArcInfluenceTile* AllocateTile(const FIntPoint& TileCoord);
	void ReleaseTile(const FIntPoint& TileCoord);

	bool IsCellVisibleToTeam(const FIntPoint& Cell, int32 TeamId) const;
	bool IsCellVisibleToAnyTeam(const FIntPoint& Cell) const;
	bool IsCellVisibleToOtherTeam(const FIntPoint& Cell, int32 TeamId) const;

	float QueryInfluence(const FVector& WorldPos) const;
	float QueryInfluenceInRadius(const FVector& Center, float Radius) const;
};

struct ARCMASS_API FArcInfluenceGridStorage
{
	EArcInfluenceStorage Kind = EArcInfluenceStorage::Sparse;
	FArcInfluenceGrid Sparse;
	FArcInfluenceTiledGrid Tiled;
	bool bLoggedAddInfluenceRejection = false;

	void Initialize(const FArcInfluenceGridConfig& Config);
	void Reset();

	const FArcInfluenceGridConfig& GetSettings() const;
};

// ---------------------------------------------------------------------------
// Subsystem
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcInfluenceMappingSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	int32 GetGridCount() const { return Grids.Num(); }

	/** Null when the grid at GridIndex is tiled. */
	FArcInfluenceGrid* GetGrid(int32 GridIndex);
	/** Null when the grid at GridIndex is sparse. */
	FArcInfluenceTiledGrid* GetTiledGrid(int32 GridIndex);

	void AddInfluence(int32 GridIndex, const FVector& WorldPos, int32 Channel, float Strength, FMassEntityHandle Source);

	UFUNCTION(BlueprintCallable, Category = "ArcMass|Influence")
	float QueryInfluence(int32 GridIndex, const FVector& WorldPos, int32 Channel) const;

	UFUNCTION(BlueprintCallable, Category = "ArcMass|Influence")
	float QueryInfluenceInRadius(int32 GridIndex, const FVector& Center, float Radius, int32 Channel) const;

private:
	/** Defined only when ENABLE_DRAW_DEBUG; the sole call site is guarded to match. */
	void DebugDrawGrids() const;

	TArray<FArcInfluenceGridStorage> Grids;
};

// ---------------------------------------------------------------------------
// Mass fragment (const shared — one per archetype chunk)
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct ARCMASS_API FArcInfluenceChannelStrength
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Channel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Strength = 1.f;
};

USTRUCT(BlueprintType)
struct ARCMASS_API FArcInfluenceSourceFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TArray<FArcInfluenceChannelStrength> Channels;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0"))
	float Radius = 0.f;
};


/** Base trait for influence mapping sources.
 *  Configures a Mass entity to emit influence into one or more channels within a radius. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Influence Mapping", Category = "Mass"))
class UArcInfluenceMappingTraitBase : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FArcInfluenceSourceFragment InfluenceConfig;
	
	/** Appends items into the entity template required for the trait. */
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

// ---------------------------------------------------------------------------
// Mass processor — abstract base
//
// Subclass in your game module:
//   1. Override GetGridIndex() to return the settings array index
//   2. Override AddGridTagRequirement() to add your FMassTag filter
// ---------------------------------------------------------------------------

UCLASS(Abstract)
class ARCMASS_API UArcInfluenceMappingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcInfluenceMappingProcessor();

protected:
	//virtual int32 GetGridIndex() const PURE_VIRTUAL(UArcInfluenceMappingProcessor::GetGridIndex, return 0;);
	virtual void AddGridTagRequirement(FMassEntityQuery& Query) PURE_VIRTUAL(UArcInfluenceMappingProcessor::AddGridTagRequirement, );

	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	int32 GridIndex = INDEX_NONE;
	FMassEntityQuery EntityQuery;
};

// ---------------------------------------------------------------------------
// Editor volume for semi-static influence
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EArcInfluenceVolumeShape : uint8
{
	Box,
	Sphere
};

UCLASS()
class ARCMASS_API AArcInfluenceVolume : public AActor
{
	GENERATED_BODY()

public:
	AArcInfluenceVolume();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Influence")
	EArcInfluenceVolumeShape Shape = EArcInfluenceVolumeShape::Box;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Influence", meta = (ClampMin = "0"))
	int32 GridIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Influence", meta = (ClampMin = "0"))
	int32 Channel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Influence")
	float Strength = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Influence")
	bool bApplyOnBeginPlay = true;

	UFUNCTION(BlueprintCallable, Category = "Influence")
	void ApplyToGrid();

	UFUNCTION(BlueprintCallable, Category = "Influence")
	void RemoveFromGrid();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(VisibleAnywhere, Category = "Influence")
	TObjectPtr<UBoxComponent> BoxComponent;

	UPROPERTY(VisibleAnywhere, Category = "Influence")
	TObjectPtr<USphereComponent> SphereComponent;

private:
	void UpdateShapeVisibility();
	bool bApplied = false;
};
