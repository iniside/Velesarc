// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassGameplayTagContainerFragment.h"
#include "MassEntityTraitBase.h"
#include "MassObserverProcessor.h"
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

/** Tag for entities that move and need per-frame spatial hash updates.
 *  Static entities (without this tag) are added/removed via observer only. */
USTRUCT()
struct ARCMASS_API FArcMassSpatialHashMoveableTag : public FMassTag
{
	GENERATED_BODY()
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

/** Hashing utilities for spatial hash index keys.
 *  Produces uint32 keys from gameplay tags, struct types, or combinations thereof.
 *  Combination hashes are order-independent. */
namespace ArcSpatialHash
{
	/** Hash a single gameplay tag. */
	inline uint32 HashKey(const FGameplayTag& Tag)
	{
		return GetTypeHash(Tag);
	}

	/** Hash a single struct type (FMassTag or FMassFragment). */
	inline uint32 HashKey(const UScriptStruct* StructType)
	{
		return PointerHash(StructType);
	}

	/** Combine two hashes in an order-independent way (commutative). */
	inline uint32 CombineHashes(uint32 A, uint32 B)
	{
		// Sort so CombineHashes(a,b) == CombineHashes(b,a)
		if (A > B) { Swap(A, B); }
		return HashCombineFast(A, B);
	}

	/** Compute a combination key from multiple individual keys. Order-independent. */
	inline uint32 CombineKeys(TArrayView<const uint32> Keys)
	{
		TArray<uint32, TInlineAllocator<8>> Sorted(Keys);
		Sorted.Sort();
		uint32 Result = 0;
		for (const uint32 Key : Sorted)
		{
			Result = HashCombineFast(Result, Key);
		}
		return Result;
	}
}

/** Const shared fragment that specifies which indexed grids this entity should be
 *  registered in. Precomputed uint32 keys allow a single unified map in the subsystem.
 *  Keys can represent individual gameplay tags, struct types, or combinations thereof. */
USTRUCT()
struct ARCMASS_API FArcMassSpatialHashIndexConfig : public FMassConstSharedFragment
{
	GENERATED_BODY()

	// Precomputed index keys — each key maps to a separate spatial grid in the subsystem.
	TArray<uint32> IndexKeys;
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

	// --- Legacy query methods (kept for backward compatibility) ---

	void QueryEntitiesInRadius(const FVector& Center, float Radius, TArray<FArcMassEntityInfo>& OutEntityHandles) const;
    void QueryEntitiesInRadiusWithDistance(const FVector& Center, float Radius, TArray<FArcMassEntityInfo>& OutEntities) const;
    void QueryEntitiesInCone(const FVector& Origin, const FVector& Direction, float Length, float HalfAngleRadians, TArray<FMassEntityHandle>& OutEntityHandles) const;
    void QueryEntitiesInConeWithDistance(const FVector& Origin, const FVector& Direction, float Length, float HalfAngleRadians, TArray<FArcMassEntityInfo>& OutEntities) const;

	template<typename PredicateType>
	void QueryEntitiesInRadiusFiltered(const FVector& Center, float Radius, PredicateType&& Predicate, TArray<FArcMassEntityInfo>& OutEntities) const
	{
		OutEntities.Reset();
		const float RadiusSq = Radius * Radius;

		const FVector MinBounds = Center - FVector(Radius);
		const FVector MaxBounds = Center + FVector(Radius);

		const FIntVector MinGrid = WorldToGrid(MinBounds);
		const FIntVector MaxGrid = WorldToGrid(MaxBounds);

		for (int32 X = MinGrid.X; X <= MaxGrid.X; X++)
		{
			for (int32 Y = MinGrid.Y; Y <= MaxGrid.Y; Y++)
			{
				for (int32 Z = MinGrid.Z; Z <= MaxGrid.Z; Z++)
				{
					if (const TArray<FEntityWithPosition>* Bucket = SpatialBuckets.Find(FIntVector(X, Y, Z)))
					{
						for (const FEntityWithPosition& Entry : *Bucket)
						{
							const float DistSq = FVector::DistSquared(Center, Entry.Position);
							if (DistSq <= RadiusSq && Predicate(Entry.Entity, Entry.Position))
							{
								OutEntities.Add({Entry.Entity, Entry.Position, FMath::Sqrt(DistSq)});
							}
						}
					}
				}
			}
		}
	}

	// --- Spatial queries (all return FArcMassEntityInfo with distance) ---

	// Sphere query: all entities within Radius of Center.
	void QuerySphere(const FVector& Center, float Radius, TArray<FArcMassEntityInfo>& OutEntities) const;

	// Cone query: all entities within a cone defined by apex Origin, Direction, Length and HalfAngleRadians.
	void QueryCone(const FVector& Origin, const FVector& Direction, float Length, float HalfAngleRadians, TArray<FArcMassEntityInfo>& OutEntities) const;

	// Box query: all entities within an axis-aligned box.
	void QueryBox(const FVector& Center, const FVector& HalfExtents, TArray<FArcMassEntityInfo>& OutEntities) const;

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

	// Get the main spatial hash grid (contains all entities)
	FMassSpatialHashGrid& GetSpatialHashGrid() { return SpatialHashGrid; }
	const FMassSpatialHashGrid& GetSpatialHashGrid() const { return SpatialHashGrid; }

	// Get an indexed grid by precomputed hash key. Returns nullptr if no entities use this index.
	FMassSpatialHashGrid* GetIndexedGrid(uint32 Key);
	const FMassSpatialHashGrid* GetIndexedGrid(uint32 Key) const;

	// Convenience: get indexed grid by gameplay tag
	FMassSpatialHashGrid* GetIndexedGrid(const FGameplayTag& Tag) { return GetIndexedGrid(ArcSpatialHash::HashKey(Tag)); }
	const FMassSpatialHashGrid* GetIndexedGrid(const FGameplayTag& Tag) const { return GetIndexedGrid(ArcSpatialHash::HashKey(Tag)); }

	// Convenience: get indexed grid by struct type
	FMassSpatialHashGrid* GetIndexedGrid(const UScriptStruct* StructType) { return GetIndexedGrid(ArcSpatialHash::HashKey(StructType)); }
	const FMassSpatialHashGrid* GetIndexedGrid(const UScriptStruct* StructType) const { return GetIndexedGrid(ArcSpatialHash::HashKey(StructType)); }

	// Templated shorthand: GetIndexedGrid<FArcMassSightPerceivableTag>()
	template<typename T>
	FMassSpatialHashGrid* GetIndexedGrid() { return GetIndexedGrid(T::StaticStruct()); }
	template<typename T>
	const FMassSpatialHashGrid* GetIndexedGrid() const { return GetIndexedGrid(T::StaticStruct()); }

	// Get or create an indexed grid by key (used internally by observers/processor)
	FMassSpatialHashGrid& GetOrCreateIndexedGrid(uint32 Key);

	// Convenience overloads
	FMassSpatialHashGrid& GetOrCreateIndexedGrid(const FGameplayTag& Tag) { return GetOrCreateIndexedGrid(ArcSpatialHash::HashKey(Tag)); }
	FMassSpatialHashGrid& GetOrCreateIndexedGrid(const UScriptStruct* StructType) { return GetOrCreateIndexedGrid(ArcSpatialHash::HashKey(StructType)); }

	// Configure settings (applies to main grid; indexed grids share the same settings)
	void SetSpatialHashSettings(const FMassSpatialHashSettings& NewSettings);

	// --- Legacy convenience queries (main grid) ---
	TArray<FArcMassEntityInfo> QueryEntitiesInRadius(const FVector& Center, float Radius) const;
	TArray<FArcMassEntityInfo> QueryEntitiesInRadius(const FGameplayTag& IndexTag, const FVector& Center, float Radius) const;

	// --- Spatial queries on main grid ---
	void QuerySphere(const FVector& Center, float Radius, TArray<FArcMassEntityInfo>& OutEntities) const;
	void QueryCone(const FVector& Origin, const FVector& Direction, float Length, float HalfAngleRadians, TArray<FArcMassEntityInfo>& OutEntities) const;
	void QueryBox(const FVector& Center, const FVector& HalfExtents, TArray<FArcMassEntityInfo>& OutEntities) const;

	// --- Spatial queries on indexed grids (by hash key) ---
	void QuerySphere(uint32 IndexKey, const FVector& Center, float Radius, TArray<FArcMassEntityInfo>& OutEntities) const;
	void QueryCone(uint32 IndexKey, const FVector& Origin, const FVector& Direction, float Length, float HalfAngleRadians, TArray<FArcMassEntityInfo>& OutEntities) const;
	void QueryBox(uint32 IndexKey, const FVector& Center, const FVector& HalfExtents, TArray<FArcMassEntityInfo>& OutEntities) const;

	// --- Convenience: gameplay tag queries ---
	void QuerySphere(const FGameplayTag& Tag, const FVector& Center, float Radius, TArray<FArcMassEntityInfo>& OutEntities) const { QuerySphere(ArcSpatialHash::HashKey(Tag), Center, Radius, OutEntities); }
	void QueryCone(const FGameplayTag& Tag, const FVector& Origin, const FVector& Direction, float Length, float HalfAngleRadians, TArray<FArcMassEntityInfo>& OutEntities) const { QueryCone(ArcSpatialHash::HashKey(Tag), Origin, Direction, Length, HalfAngleRadians, OutEntities); }
	void QueryBox(const FGameplayTag& Tag, const FVector& Center, const FVector& HalfExtents, TArray<FArcMassEntityInfo>& OutEntities) const { QueryBox(ArcSpatialHash::HashKey(Tag), Center, HalfExtents, OutEntities); }

	// --- Convenience: struct type queries ---
	void QuerySphere(const UScriptStruct* Type, const FVector& Center, float Radius, TArray<FArcMassEntityInfo>& OutEntities) const { QuerySphere(ArcSpatialHash::HashKey(Type), Center, Radius, OutEntities); }
	void QueryCone(const UScriptStruct* Type, const FVector& Origin, const FVector& Direction, float Length, float HalfAngleRadians, TArray<FArcMassEntityInfo>& OutEntities) const { QueryCone(ArcSpatialHash::HashKey(Type), Origin, Direction, Length, HalfAngleRadians, OutEntities); }
	void QueryBox(const UScriptStruct* Type, const FVector& Center, const FVector& HalfExtents, TArray<FArcMassEntityInfo>& OutEntities) const { QueryBox(ArcSpatialHash::HashKey(Type), Center, HalfExtents, OutEntities); }

	// --- Templated struct-type queries ---
	template<typename T> void QuerySphere(const FVector& Center, float Radius, TArray<FArcMassEntityInfo>& OutEntities) const { QuerySphere(ArcSpatialHash::HashKey(T::StaticStruct()), Center, Radius, OutEntities); }
	template<typename T> void QueryCone(const FVector& Origin, const FVector& Direction, float Length, float HalfAngleRadians, TArray<FArcMassEntityInfo>& OutEntities) const { QueryCone(ArcSpatialHash::HashKey(T::StaticStruct()), Origin, Direction, Length, HalfAngleRadians, OutEntities); }
	template<typename T> void QueryBox(const FVector& Center, const FVector& HalfExtents, TArray<FArcMassEntityInfo>& OutEntities) const { QueryBox(ArcSpatialHash::HashKey(T::StaticStruct()), Center, HalfExtents, OutEntities); }

private:
	FMassSpatialHashGrid SpatialHashGrid;

	// Unified indexed grids — keyed by uint32 hash of gameplay tags, struct types, or combinations
	TMap<uint32, FMassSpatialHashGrid> IndexedGrids;
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

/** Observer that adds/removes static entities from the spatial hash
 *  when FArcMassSpatialHashFragment is added or removed. */
UCLASS()
class ARCMASS_API UArcMassSpatialHashObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMassSpatialHashObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

/** Observer that removes entities from the spatial hash when
 *  FArcMassSpatialHashFragment is removed (entity destruction). */
UCLASS()
class ARCMASS_API UArcMassSpatialHashRemoveObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMassSpatialHashRemoveObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

/** Editor-friendly description of a combination index.
 *  All tags and struct types in one entry produce a single combined hash key.
 *  Entities are inserted into that grid only if they match the full combination. */
USTRUCT(BlueprintType)
struct ARCMASS_API FArcMassSpatialHashCombinationIndex
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "SpatialHash")
	FGameplayTagContainer Tags;

	UPROPERTY(EditAnywhere, Category = "SpatialHash", meta = (BaseStruct = "/Script/MassEntity.MassTag"))
	TArray<FInstancedStruct> MassTags;

	UPROPERTY(EditAnywhere, Category = "SpatialHash", meta = (BaseStruct = "/Script/MassEntity.MassFragment"))
	TArray<FInstancedStruct> Fragments;
};

/** Trait that adds spatial hash tracking to an entity.
 *  Set bMoveable to true for entities that move (per-frame updates),
 *  false for static entities (observer-only add/remove). */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Spatial Hash"))
class ARCMASS_API UArcMassSpatialHashTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	/** Whether this entity moves and needs per-frame spatial hash updates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialHash")
	bool bMoveable = true;

	/** Gameplay tags to index this entity under for fast filtered queries.
	 *  Each tag creates a separate spatial grid containing only matching entities. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpatialHash|Index")
	FGameplayTagContainer IndexTags;

	/** Mass tags to index by (e.g. FArcMassSightPerceivableTag).
	 *  Each tag type creates a separate spatial grid. */
	UPROPERTY(EditAnywhere, Category = "SpatialHash|Index", meta = (BaseStruct = "/Script/MassEntity.MassTag"))
	TArray<FInstancedStruct> IndexMassTags;

	/** Fragment types to index by (e.g. FArcSmartObjectOwnerFragment).
	 *  Each fragment type creates a separate spatial grid. */
	UPROPERTY(EditAnywhere, Category = "SpatialHash|Index", meta = (BaseStruct = "/Script/MassEntity.MassFragment"))
	TArray<FInstancedStruct> IndexFragments;

	/** Combination indices — each entry produces a single hash key from all its
	 *  tags/structs combined. Allows querying by specific combinations. */
	UPROPERTY(EditAnywhere, Category = "SpatialHash|Index")
	TArray<FArcMassSpatialHashCombinationIndex> CombinationIndices;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
