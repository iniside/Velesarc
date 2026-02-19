// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InstancedStruct.h"
#include "Engine/DeveloperSettings.h"
#include "StructUtils/InstancedStruct.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcSpatialWorldData.generated.h"

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct ARCMASS_API FArcSpatialDataLayerConfig
{
	GENERATED_BODY()

	/** Tag that uniquely identifies this layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialData")
	FGameplayTag LayerTag;

	/** World units per grid cell. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialData", meta = (ClampMin = "1.0"))
	float CellSize = 2000.f;

	/** If true, Z is flattened to 0 for a 2D grid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialData")
	bool bIs2D = true;

#if WITH_EDITORONLY_DATA
	/** Hint: the struct type stored in cells (editor display only). */
	UPROPERTY(EditAnywhere, Category = "SpatialData")
	TObjectPtr<UScriptStruct> DataType = nullptr;
#endif
};

// ---------------------------------------------------------------------------
// Query result
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct ARCMASS_API FArcSpatialDataQueryResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SpatialData")
	FIntVector GridCoords = FIntVector::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "SpatialData")
	FVector WorldCenter = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "SpatialData")
	FInstancedStruct Data;
};

// ---------------------------------------------------------------------------
// Data layer (internal, not a USTRUCT)
// ---------------------------------------------------------------------------

struct ARCMASS_API FArcSpatialDataLayer
{
	FArcSpatialDataLayerConfig Config;
	TMap<FIntVector, FInstancedStruct> Cells;

	FIntVector WorldToGrid(const FVector& WorldPos) const;
	FVector GridToWorld(const FIntVector& GridCoords) const;

	void SetCellData(const FIntVector& GridCoords, const FInstancedStruct& Data);
	const FInstancedStruct* GetCellData(const FIntVector& GridCoords) const;
	bool RemoveCellData(const FIntVector& GridCoords);

	TArray<FIntVector> GetAllOccupiedCells() const;
	TArray<FIntVector> QueryCellsInRadius(const FVector& Center, float Radius) const;
	TArray<FIntVector> QueryCellsInBox(const FBox& Box) const;
};

// ---------------------------------------------------------------------------
// Developer Settings
// ---------------------------------------------------------------------------

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Arc Spatial World Data"))
class ARCMASS_API UArcSpatialWorldDataSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(config, EditAnywhere, Category = "Layers")
	TArray<FArcSpatialDataLayerConfig> Layers;

	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }
	virtual FName GetSectionName() const override { return FName(TEXT("Arc Spatial World Data")); }
};

// ---------------------------------------------------------------------------
// Subsystem
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcSpatialWorldDataSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// -- Layer management ---------------------------------------------------

	FArcSpatialDataLayer* CreateLayer(const FArcSpatialDataLayerConfig& Config);
	FArcSpatialDataLayer* GetLayer(FGameplayTag LayerTag);
	const FArcSpatialDataLayer* GetLayer(FGameplayTag LayerTag) const;
	void RemoveLayer(FGameplayTag LayerTag);

	// -- Templated C++ API --------------------------------------------------

	template<typename T>
	void SetData(FGameplayTag LayerTag, const FVector& Location, const T& Data);

	template<typename T>
	const T* GetData(FGameplayTag LayerTag, const FVector& Location) const;

	// -- Blueprint-callable API ---------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "ArcMass|SpatialData")
	void SetDataFromStruct(FGameplayTag LayerTag, FVector Location, const FInstancedStruct& Data);

	UFUNCTION(BlueprintCallable, Category = "ArcMass|SpatialData")
	FInstancedStruct GetDataFromStruct(FGameplayTag LayerTag, FVector Location) const;

	UFUNCTION(BlueprintCallable, Category = "ArcMass|SpatialData")
	bool HasData(FGameplayTag LayerTag, FVector Location) const;

	UFUNCTION(BlueprintCallable, Category = "ArcMass|SpatialData")
	bool RemoveData(FGameplayTag LayerTag, FVector Location);

	// -- Query API ----------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "ArcMass|SpatialData")
	void QueryDataInRadius(FGameplayTag LayerTag, FVector Center, float Radius, TArray<FArcSpatialDataQueryResult>& OutResults) const;

	UFUNCTION(BlueprintCallable, Category = "ArcMass|SpatialData")
	void QueryDataInBox(FGameplayTag LayerTag, FBox Box, TArray<FArcSpatialDataQueryResult>& OutResults) const;

	/** C++ only — returns cells whose data satisfies the predicate. */
	void FindCellsMeetingPredicate(FGameplayTag LayerTag, TFunctionRef<bool(const FIntVector&, const FInstancedStruct&)> Predicate, TArray<FArcSpatialDataQueryResult>& OutResults) const;

	// -- Bulk / utility -----------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "ArcMass|SpatialData")
	void ClearLayer(FGameplayTag LayerTag);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ArcMass|SpatialData")
	int32 GetOccupiedCellCount(FGameplayTag LayerTag) const;

private:
	TMap<FGameplayTag, FArcSpatialDataLayer> Layers;

#if !UE_BUILD_SHIPPING
	void DebugDraw() const;
#endif
};

// ---------------------------------------------------------------------------
// Template implementations
// ---------------------------------------------------------------------------

template<typename T>
void UArcSpatialWorldDataSubsystem::SetData(FGameplayTag LayerTag, const FVector& Location, const T& Data)
{
	FArcSpatialDataLayer* Layer = GetLayer(LayerTag);
	if (!Layer)
	{
		return;
	}
	const FIntVector GridCoords = Layer->WorldToGrid(Location);
	FInstancedStruct Instance;
	Instance.InitializeAs<T>(Data);
	Layer->SetCellData(GridCoords, Instance);
}

template<typename T>
const T* UArcSpatialWorldDataSubsystem::GetData(FGameplayTag LayerTag, const FVector& Location) const
{
	const FArcSpatialDataLayer* Layer = GetLayer(LayerTag);
	if (!Layer)
	{
		return nullptr;
	}
	const FIntVector GridCoords = Layer->WorldToGrid(Location);
	const FInstancedStruct* Cell = Layer->GetCellData(GridCoords);
	if (!Cell)
	{
		return nullptr;
	}
	return Cell->GetPtr<T>();
}
