// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTypes.h"
#include "MassProcessor.h"
#include "Engine/DeveloperSettings.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcMassInfluenceMapping.generated.h"

class UBoxComponent;
class USphereComponent;

// ---------------------------------------------------------------------------
// Developer Settings
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct ARCMASS_API FArcInfluenceGridConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Influence", meta = (ClampMin = "1.0"))
	float CellSize = 1000.f;

	UPROPERTY(EditAnywhere, Category = "Influence", meta = (ClampMin = "1"))
	int32 NumChannels = 1;

	UPROPERTY(EditAnywhere, Category = "Influence", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PropagationRate = 0.1f;

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
};

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Arc Influence Mapping"))
class ARCMASS_API UArcInfluenceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(config, EditAnywhere, Category = "Grids")
	TArray<FArcInfluenceGridConfig> Grids;

	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }
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
	const FArcInfluenceEntry* GetStrongestSource(int32 Channel) const;
	bool IsEmpty() const;
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
	int32 PropagationCursor = 0;
	int32 DecayCursor = 0;
	float DecayAccumulator = 0.f;

	FIntVector WorldToGrid(const FVector& WorldPos) const;
	FArcInfluenceCell& FindOrAddCell(const FIntVector& GridCoords);

	void AddInfluence(const FIntVector& GridCoords, int32 Channel, float Strength, FMassEntityHandle Source);
	void AddInfluenceBatch(TConstArrayView<FArcPendingInfluence> Batch);
	void RemoveInfluenceBySource(FMassEntityHandle Source);

	FArcInfluenceCell* QueryCell(const FIntVector& GridCoords);
	const FArcInfluenceCell* QueryCell(const FIntVector& GridCoords) const;

	float QueryInfluence(const FVector& WorldPos, int32 Channel) const;
	float QueryInfluenceInRadius(const FVector& Center, float Radius, int32 Channel) const;

	void SetCellInfluence(const FIntVector& GridCoords, int32 Channel, float Strength);

	void PropagateStep(int32 MaxCells);
	void DecayStep(int32 MaxCells);
	void TickDecay(float DeltaTime);
};

// ---------------------------------------------------------------------------
// Subsystem
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcInfluenceMappingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	int32 GetGridCount() const { return Grids.Num(); }
	FArcInfluenceGrid& GetGrid(int32 GridIndex);

	void AddInfluence(int32 GridIndex, const FVector& WorldPos, int32 Channel, float Strength, FMassEntityHandle Source);
	void RemoveInfluenceBySource(int32 GridIndex, FMassEntityHandle Source);

	UFUNCTION(BlueprintCallable, Category = "ArcMass|Influence")
	float QueryInfluence(int32 GridIndex, const FVector& WorldPos, int32 Channel) const;

	UFUNCTION(BlueprintCallable, Category = "ArcMass|Influence")
	float QueryInfluenceInRadius(int32 GridIndex, const FVector& Center, float Radius, int32 Channel) const;

	void UpdateSemiStaticGrid(int32 GridIndex);
	void SetCellInfluence(int32 GridIndex, const FIntVector& GridCoords, int32 Channel, float Strength);

	TConstArrayView<FArcInfluenceGrid> GetAllGrids() const { return Grids; }

private:
	TArray<FArcInfluenceGrid> Grids;
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


UCLASS(BlueprintType, EditInlineNew, CollapseCategories)
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
