// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassGameplayTagContainerFragment.h"
#include "MassProcessor.h"
#include "UObject/Object.h"
#include "ArcMassSpatialHashSubsystem.generated.h"

USTRUCT()
struct ARCMASS_API FMassSpatialHashFragment : public FMassFragment
{
    GENERATED_BODY()

    // Grid coordinates this entity is currently in
    FIntVector GridCoords = FIntVector::ZeroValue;
    
    // Hash of the grid cell for quick lookup
    uint32 CellHash = 0;
};

USTRUCT(BlueprintType)
struct ARCMASS_API FMassSpatialHashSettings
{
    GENERATED_BODY()

    // Size of each grid cell (larger = fewer cells but less precision)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CellSize = 1000.0f; // 10 meters per cell
    
    // Optional: limit search radius for performance
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxQueryRadius = 5000.0f;
};

// Simple hash map using TMap for sparse storage
struct FMassSpatialHashGrid
{
    // Cell hash -> array of entity indices
    TMap<uint32, TArray<FMassEntityHandle>> SpatialBuckets;
    FMassSpatialHashSettings Settings;
    
    // Convert world position to grid coordinates
    FIntVector WorldToGrid(const FVector& WorldPos) const
    {
        return FIntVector(
            FMath::FloorToInt(WorldPos.X / Settings.CellSize),
            FMath::FloorToInt(WorldPos.Y / Settings.CellSize),
            FMath::FloorToInt(WorldPos.Z / Settings.CellSize)
        );
    }
    
    // Simple hash function for grid coordinates
    uint32 HashGridCoords(const FIntVector& GridCoords) const
    {
        // Simple hash combining x, y, z
        return HashCombine(HashCombine(GetTypeHash(GridCoords.X), GetTypeHash(GridCoords.Y)), GetTypeHash(GridCoords.Z));
    }
    
    // Add entity to spatial hash
    void AddEntity(FMassEntityHandle EntityIndex, const FVector& Position)
    {
        FIntVector GridCoords = WorldToGrid(Position);
        uint32 CellHash = HashGridCoords(GridCoords);
        SpatialBuckets.FindOrAdd(CellHash).AddUnique(EntityIndex);
    }
    
    // Remove entity from spatial hash
    void RemoveEntity(FMassEntityHandle EntityIndex, uint32 OldCellHash)
    {
        if (TArray<FMassEntityHandle>* Bucket = SpatialBuckets.Find(OldCellHash))
        {
            Bucket->Remove(EntityIndex);
            if (Bucket->IsEmpty())
            {
                SpatialBuckets.Remove(OldCellHash);
            }
        }
    }
    
    // Query entities within radius
    void QueryEntitiesInRadius(const FVector& Center, float Radius, TArray<FMassEntityHandle>& OutEntityIndices) const
    {
        OutEntityIndices.Reset();
        
        // Calculate grid bounds for the query
        FVector MinBounds = Center - FVector(Radius);
        FVector MaxBounds = Center + FVector(Radius);
        
        FIntVector MinGrid = WorldToGrid(MinBounds);
        FIntVector MaxGrid = WorldToGrid(MaxBounds);
        
        // Check all cells in the bounding box
        for (int32 X = MinGrid.X; X <= MaxGrid.X; X++)
        {
            for (int32 Y = MinGrid.Y; Y <= MaxGrid.Y; Y++)
            {
                for (int32 Z = MinGrid.Z; Z <= MaxGrid.Z; Z++)
                {
                    FIntVector GridCoords(X, Y, Z);
                    uint32 CellHash = HashGridCoords(GridCoords);
                    
                    if (const TArray<FMassEntityHandle>* Bucket = SpatialBuckets.Find(CellHash))
                    {
                        OutEntityIndices.Append(*Bucket);
                    }
                }
            }
        }
    }
    
    // Clear all entities
    void Clear()
    {
        SpatialBuckets.Empty();
    }
};

/**
 * 
 */
UCLASS()
class ARCMASS_API UArcMassSpatialHashSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Get the spatial hash grid
	FMassSpatialHashGrid& GetSpatialHashGrid() { return SpatialHashGrid; }
	const FMassSpatialHashGrid& GetSpatialHashGrid() const { return SpatialHashGrid; }
    
	// Configure settings
	void SetSpatialHashSettings(const FMassSpatialHashSettings& NewSettings);
    
	// Query entities near a position
	TArray<FMassEntityHandle> QueryEntitiesInRadius(const FVector& Center, float Radius) const;

private:
	FMassSpatialHashGrid SpatialHashGrid;
};

UCLASS()
class ARCMASS_API UArcMassSpatialHashUpdateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcMassSpatialHashUpdateProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
