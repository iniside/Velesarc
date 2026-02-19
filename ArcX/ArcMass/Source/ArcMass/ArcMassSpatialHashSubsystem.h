// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassGameplayTagContainerFragment.h"
#include "MassProcessor.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/Object.h"
#include "ArcMassSpatialHashSubsystem.generated.h"

USTRUCT()
struct ARCMASS_API FArcMassSpatialHashFragment : public FMassFragment
{
    GENERATED_BODY()

    // Grid coordinates this entity is currently in
    FIntVector GridCoords = FIntVector::ZeroValue;
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

struct FArcMassEntityInfo
{
	FMassEntityHandle Entity;
	FVector Location = FVector::ZeroVector;
	float Distance = 0.0f;
};

// Simple hash map using TMap for sparse storage
struct ARCMASS_API FMassSpatialHashGrid
{
    // Cell coordinates -> array of entities with their positions
    struct FEntityWithPosition
    {
        FMassEntityHandle Entity;
        FVector Position;
    };
    
    // Use grid coordinates directly as key instead of hash to avoid collisions
    TMap<FIntVector, TArray<FEntityWithPosition>> SpatialBuckets;
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
        
    // Add entity to spatial hash
    void AddEntity(FMassEntityHandle EntityHandle, const FVector& Position)
    {
        FIntVector GridCoords = WorldToGrid(Position);
        SpatialBuckets.FindOrAdd(GridCoords).Add({EntityHandle, Position});
    }
    
    // Remove entity from spatial hash
    void RemoveEntity(FMassEntityHandle EntityHandle, const FIntVector& OldGridCoords)
    {
        if (TArray<FEntityWithPosition>* Bucket = SpatialBuckets.Find(OldGridCoords))
        {
            Bucket->RemoveAll([EntityHandle](const FEntityWithPosition& Entry)
            {
                return Entry.Entity == EntityHandle;
            });
            if (Bucket->IsEmpty())
            {
                SpatialBuckets.Remove(OldGridCoords);
            }
        }
    }
    
    // Update entity position (call when entity moves)
    void UpdateEntity(FMassEntityHandle EntityHandle, const FIntVector& OldGridCoords, const FVector& NewPosition);

	// Query entities within radius with precise distance filtering
    void QueryEntitiesInRadius(const FVector& Center, float Radius, TArray<FArcMassEntityInfo>& OutEntityHandles) const;

	// Query with distance information if needed
    void QueryEntitiesInRadiusWithDistance(const FVector& Center, float Radius, TArray<FArcMassEntityInfo>& OutEntities) const;

	// Query entities within a cone
    // Origin: cone apex position
    // Direction: normalized direction the cone is facing
    // Length: how far the cone extends
    // HalfAngleRadians: half of the cone's opening angle (e.g., PI/6 for 60-degree cone)
    void QueryEntitiesInCone(const FVector& Origin, const FVector& Direction, float Length, float HalfAngleRadians, TArray<FMassEntityHandle>& OutEntityHandles) const;

	// Query entities in cone with distance information
    void QueryEntitiesInConeWithDistance(const FVector& Origin, const FVector& Direction, float Length, float HalfAngleRadians, TArray<FArcMassEntityInfo>& OutEntities) const;

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
	TArray<FArcMassEntityInfo> QueryEntitiesInRadius(const FVector& Center, float Radius) const;

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
