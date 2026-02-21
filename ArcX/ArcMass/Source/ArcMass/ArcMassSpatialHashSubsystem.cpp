// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassSpatialHashSubsystem.h"

#include "DrawDebugHelpers.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"

TAutoConsoleVariable<bool> CVarArcDebugDrawSpatialHash(
	TEXT("arc.mass.DebugDrawSpatialHash"),
	false,
	TEXT("Toggles debug drawing for the Mass spatial hash grid (0 = off, 1 = on)"));

void FMassSpatialHashGrid::UpdateEntity(FMassEntityHandle EntityHandle, const FIntVector& GridCoords, const FVector& NewPosition)
{
	// Only updates position within the same cell. Cell changes are handled by the caller.
	if (TArray<FEntityWithPosition>* Bucket = SpatialBuckets.Find(GridCoords))
	{
		for (FEntityWithPosition& Entry : *Bucket)
		{
			if (Entry.Entity == EntityHandle)
			{
				Entry.Position = NewPosition;
				break;
			}
		}
	}
}

void FMassSpatialHashGrid::QueryEntitiesInRadius(const FVector& Center, float Radius, TArray<FArcMassEntityInfo>& OutEntityHandles) const
{
	OutEntityHandles.Reset();
        
	const float RadiusSq = Radius * Radius;
        
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
                    
				if (const TArray<FEntityWithPosition>* Bucket = SpatialBuckets.Find(GridCoords))
				{
					for (const FEntityWithPosition& Entry : *Bucket)
					{
						// Precise distance check
						if (FVector::DistSquared(Center, Entry.Position) <= RadiusSq)
						{
							OutEntityHandles.Add({Entry.Entity, Entry.Position, 0});
						}
					}
				}
			}
		}
	}
}

void FMassSpatialHashGrid::QueryEntitiesInRadiusWithDistance(const FVector& Center, float Radius
															 , TArray<FArcMassEntityInfo>& OutEntities) const
{
	OutEntities.Reset(32);
        
	const float RadiusSq = Radius * Radius;
        
	FVector MinBounds = Center - FVector(Radius);
	FVector MaxBounds = Center + FVector(Radius);
        
	FIntVector MinGrid = WorldToGrid(MinBounds);
	FIntVector MaxGrid = WorldToGrid(MaxBounds);
        
	for (int32 X = MinGrid.X; X <= MaxGrid.X; X++)
	{
		for (int32 Y = MinGrid.Y; Y <= MaxGrid.Y; Y++)
		{
			for (int32 Z = MinGrid.Z; Z <= MaxGrid.Z; Z++)
			{
				FIntVector GridCoords(X, Y, Z);
                    
				if (const TArray<FEntityWithPosition>* Bucket = SpatialBuckets.Find(GridCoords))
				{
					for (const FEntityWithPosition& Entry : *Bucket)
					{
						float DistSq = FVector::DistSquared(Center, Entry.Position);
						if (DistSq <= RadiusSq)
						{
							OutEntities.Add({Entry.Entity, Entry.Position, FMath::Sqrt(DistSq)});
						}
					}
				}
			}
		}
	}
}

void FMassSpatialHashGrid::QueryEntitiesInCone(const FVector& Origin, const FVector& Direction, float Length, float HalfAngleRadians
											   , TArray<FMassEntityHandle>& OutEntityHandles) const
{
	OutEntityHandles.Reset();
        
	const float LengthSq = Length * Length;
	const float CosHalfAngle = FMath::Cos(HalfAngleRadians);
        
	// Calculate bounding box for the cone
	// 's max radius at its base
	const float MaxRadius = Length * FMath::Tan(HalfAngleRadians);
        
	// Build an oriented bounding box around the cone
	const FVector ConeEnd = Origin + Direction * Length;
	const FVector ConeCenter = (Origin + ConeEnd) * 0.5f;
        
	// Conservative AABB that encompasses the cone
	const float BoundingRadius = FMath::Max(Length * 0.5f, MaxRadius);
	const FVector MinBounds = ConeCenter - FVector(BoundingRadius);
	const FVector MaxBounds = ConeCenter + FVector(BoundingRadius);
        
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
                    
				if (const TArray<FEntityWithPosition>* Bucket = SpatialBuckets.Find(GridCoords))
				{
					for (const FEntityWithPosition& Entry : *Bucket)
					{
						const FVector ToEntity = Entry.Position - Origin;
						const float DistSq = ToEntity.SizeSquared();
                            
						// Check if within cone length
						if (DistSq > LengthSq || DistSq < KINDA_SMALL_NUMBER)
						{
							continue;
						}
                            
						// Check if within cone angle
						const FVector ToEntityNormalized = ToEntity * FMath::InvSqrt(DistSq);
						const float DotProduct = FVector::DotProduct(Direction, ToEntityNormalized);
                            
						if (DotProduct >= CosHalfAngle)
						{
							OutEntityHandles.Add(Entry.Entity);
						}
					}
				}
			}
		}
	}
}

void FMassSpatialHashGrid::QueryEntitiesInConeWithDistance(const FVector& Origin, const FVector& Direction, float Length, float HalfAngleRadians
														   , TArray<FArcMassEntityInfo>& OutEntities) const
{
	OutEntities.Reset(32);
		
	const float LengthSq = Length * Length;
	const float CosHalfAngle = FMath::Cos(HalfAngleRadians);
		
	// Calculate the cone's tip position
	const FVector ConeEnd = Origin + Direction * Length;
    	
	// Maximum radius at the cone's base
	const float MaxRadius = Length * FMath::Tan(HalfAngleRadians);
		
	// Build proper AABB that encompasses the entire cone
	// Start with origin and cone end
	FVector MinBounds = Origin.ComponentMin(ConeEnd);
	FVector MaxBounds = Origin.ComponentMax(ConeEnd);
    	
	// Expand by the maximum radius to account for the cone's width
	MinBounds -= FVector(MaxRadius);
	MaxBounds += FVector(MaxRadius);
		
	FIntVector MinGrid = WorldToGrid(MinBounds);
	FIntVector MaxGrid = WorldToGrid(MaxBounds);
		
	constexpr float MinDistanceThreshold = 1.0f;
	constexpr float MinDistSqThreshold = MinDistanceThreshold * MinDistanceThreshold;
		
	for (int32 X = MinGrid.X; X <= MaxGrid.X; X++)
	{
		for (int32 Y = MinGrid.Y; Y <= MaxGrid.Y; Y++)
		{
			for (int32 Z = MinGrid.Z; Z <= MaxGrid.Z; Z++)
			{
				FIntVector GridCoords(X, Y, Z);
		
				if (const TArray<FEntityWithPosition>* Bucket = SpatialBuckets.Find(GridCoords))
				{
					for (const FEntityWithPosition& Entry : *Bucket)
					{
						const FVector ToEntity = Entry.Position - Origin;
						const float DistSq = ToEntity.SizeSquared();
		
						if (DistSq > LengthSq)
						{
							continue;
						}
		
						if (DistSq < MinDistSqThreshold)
						{
							OutEntities.Add({Entry.Entity, Entry.Position, FMath::Sqrt(DistSq)});
							continue;
						}
		
						const float Dist = FMath::Sqrt(DistSq);
						
						FVector ToEntityNormalized = ToEntity.GetSafeNormal2D();
						const float DotProduct = FVector::DotProduct(Direction.GetSafeNormal2D(), ToEntityNormalized);
		
						if (DotProduct >= CosHalfAngle)
						{
							OutEntities.Add( {Entry.Entity, Entry.Position, Dist});
						}
					}
				}
			}
		}
	}
}

void FMassSpatialHashGrid::QuerySphere(const FVector& Center, float Radius, TArray<FArcMassEntityInfo>& OutEntities) const
{
	OutEntities.Reset();
	const float RadiusSq = Radius * Radius;

	const FIntVector MinGrid = WorldToGrid(Center - FVector(Radius));
	const FIntVector MaxGrid = WorldToGrid(Center + FVector(Radius));

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
						if (DistSq <= RadiusSq)
						{
							OutEntities.Add({Entry.Entity, Entry.Position, FMath::Sqrt(DistSq)});
						}
					}
				}
			}
		}
	}
}

void FMassSpatialHashGrid::QueryCone(const FVector& Origin, const FVector& Direction, float Length, float HalfAngleRadians, TArray<FArcMassEntityInfo>& OutEntities) const
{
	OutEntities.Reset();

	const float LengthSq = Length * Length;
	const float CosHalfAngle = FMath::Cos(HalfAngleRadians);
	const float MaxRadius = Length * FMath::Tan(HalfAngleRadians);
	const FVector ConeEnd = Origin + Direction * Length;

	// Tight AABB around the cone
	FVector MinBounds = Origin.ComponentMin(ConeEnd) - FVector(MaxRadius);
	FVector MaxBounds = Origin.ComponentMax(ConeEnd) + FVector(MaxRadius);

	const FIntVector MinGrid = WorldToGrid(MinBounds);
	const FIntVector MaxGrid = WorldToGrid(MaxBounds);

	constexpr float MinDistSqThreshold = 1.0f;

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
						const FVector ToEntity = Entry.Position - Origin;
						const float DistSq = ToEntity.SizeSquared();

						if (DistSq > LengthSq)
						{
							continue;
						}

						const float Dist = FMath::Sqrt(DistSq);

						// Very close entities are always included
						if (DistSq < MinDistSqThreshold)
						{
							OutEntities.Add({Entry.Entity, Entry.Position, Dist});
							continue;
						}

						const FVector ToEntityNorm = ToEntity / Dist;
						if (FVector::DotProduct(Direction, ToEntityNorm) >= CosHalfAngle)
						{
							OutEntities.Add({Entry.Entity, Entry.Position, Dist});
						}
					}
				}
			}
		}
	}
}

void FMassSpatialHashGrid::QueryBox(const FVector& Center, const FVector& HalfExtents, TArray<FArcMassEntityInfo>& OutEntities) const
{
	OutEntities.Reset();

	const FVector MinBounds = Center - HalfExtents;
	const FVector MaxBounds = Center + HalfExtents;

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
						if (Entry.Position.X >= MinBounds.X && Entry.Position.X <= MaxBounds.X &&
							Entry.Position.Y >= MinBounds.Y && Entry.Position.Y <= MaxBounds.Y &&
							Entry.Position.Z >= MinBounds.Z && Entry.Position.Z <= MaxBounds.Z)
						{
							OutEntities.Add({Entry.Entity, Entry.Position, (float)FVector::Dist(Center, Entry.Position)});
						}
					}
				}
			}
		}
	}
}

void UArcMassSpatialHashSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
    
	// Initialize with default settings
	FMassSpatialHashSettings DefaultSettings;
	SpatialHashGrid.Settings = DefaultSettings;
}

void UArcMassSpatialHashSubsystem::Deinitialize()
{
	SpatialHashGrid.Clear();
	IndexedGrids.Empty();
	Super::Deinitialize();
}

void UArcMassSpatialHashSubsystem::SetSpatialHashSettings(const FMassSpatialHashSettings& NewSettings)
{
	SpatialHashGrid.Settings = NewSettings;
	SpatialHashGrid.Clear();
	for (auto& Pair : IndexedGrids)
	{
		Pair.Value.Settings = NewSettings;
		Pair.Value.Clear();
	}
}

FMassSpatialHashGrid* UArcMassSpatialHashSubsystem::GetIndexedGrid(uint32 Key)
{
	return IndexedGrids.Find(Key);
}

const FMassSpatialHashGrid* UArcMassSpatialHashSubsystem::GetIndexedGrid(uint32 Key) const
{
	return IndexedGrids.Find(Key);
}

FMassSpatialHashGrid& UArcMassSpatialHashSubsystem::GetOrCreateIndexedGrid(uint32 Key)
{
	FMassSpatialHashGrid& Grid = IndexedGrids.FindOrAdd(Key);
	Grid.Settings = SpatialHashGrid.Settings;
	return Grid;
}

TArray<FArcMassEntityInfo> UArcMassSpatialHashSubsystem::QueryEntitiesInRadius(const FVector& Center, float Radius) const
{
	TArray<FArcMassEntityInfo> Results;
	SpatialHashGrid.QueryEntitiesInRadius(Center, Radius, Results);
	return Results;
}

TArray<FArcMassEntityInfo> UArcMassSpatialHashSubsystem::QueryEntitiesInRadius(const FGameplayTag& IndexTag, const FVector& Center, float Radius) const
{
	TArray<FArcMassEntityInfo> Results;
	if (const FMassSpatialHashGrid* Grid = GetIndexedGrid(IndexTag))
	{
		Grid->QueryEntitiesInRadius(Center, Radius, Results);
	}
	return Results;
}

// --- Main grid spatial queries ---

void UArcMassSpatialHashSubsystem::QuerySphere(const FVector& Center, float Radius, TArray<FArcMassEntityInfo>& OutEntities) const
{
	SpatialHashGrid.QuerySphere(Center, Radius, OutEntities);
}

void UArcMassSpatialHashSubsystem::QueryCone(const FVector& Origin, const FVector& Direction, float Length, float HalfAngleRadians, TArray<FArcMassEntityInfo>& OutEntities) const
{
	SpatialHashGrid.QueryCone(Origin, Direction, Length, HalfAngleRadians, OutEntities);
}

void UArcMassSpatialHashSubsystem::QueryBox(const FVector& Center, const FVector& HalfExtents, TArray<FArcMassEntityInfo>& OutEntities) const
{
	SpatialHashGrid.QueryBox(Center, HalfExtents, OutEntities);
}

// --- Indexed grid spatial queries (by uint32 key) ---

void UArcMassSpatialHashSubsystem::QuerySphere(uint32 IndexKey, const FVector& Center, float Radius, TArray<FArcMassEntityInfo>& OutEntities) const
{
	if (const FMassSpatialHashGrid* Grid = GetIndexedGrid(IndexKey))
	{
		Grid->QuerySphere(Center, Radius, OutEntities);
	}
	else
	{
		OutEntities.Reset();
	}
}

void UArcMassSpatialHashSubsystem::QueryCone(uint32 IndexKey, const FVector& Origin, const FVector& Direction, float Length, float HalfAngleRadians, TArray<FArcMassEntityInfo>& OutEntities) const
{
	if (const FMassSpatialHashGrid* Grid = GetIndexedGrid(IndexKey))
	{
		Grid->QueryCone(Origin, Direction, Length, HalfAngleRadians, OutEntities);
	}
	else
	{
		OutEntities.Reset();
	}
}

void UArcMassSpatialHashSubsystem::QueryBox(uint32 IndexKey, const FVector& Center, const FVector& HalfExtents, TArray<FArcMassEntityInfo>& OutEntities) const
{
	if (const FMassSpatialHashGrid* Grid = GetIndexedGrid(IndexKey))
	{
		Grid->QueryBox(Center, HalfExtents, OutEntities);
	}
	else
	{
		OutEntities.Reset();
	}
}

UArcMassSpatialHashUpdateProcessor::UArcMassSpatialHashUpdateProcessor()
	: EntityQuery{*this}
{
	//bAutoRegisterWithProcessingPhases = true;
    ProcessingPhase = EMassProcessingPhase::PrePhysics;
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	//bRequiresGameThreadExecution = true;
	
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::SyncWorldToMass);
}

void UArcMassSpatialHashUpdateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FArcMassSpatialHashFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddTagRequirement<FArcMassSpatialHashMoveableTag>(EMassFragmentPresence::All);
    EntityQuery.AddConstSharedRequirement<FArcMassSpatialHashIndexConfig>(EMassFragmentPresence::Optional);
}

void UArcMassSpatialHashUpdateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // Get the spatial hash subsystem
    UWorld* World = EntityManager.GetWorld();
    UArcMassSpatialHashSubsystem* SpatialHashSubsystem = World->GetSubsystem<UArcMassSpatialHashSubsystem>();
    
    if (!SpatialHashSubsystem)
    {
        return;
    }
    
    FMassSpatialHashGrid& SpatialHashGrid = SpatialHashSubsystem->GetSpatialHashGrid();
    
#if WITH_GAMEPLAY_DEBUGGER
	
	// Debug draw occupied grid cells and entities
	if (CVarArcDebugDrawSpatialHash.GetValueOnAnyThread())
	{
		const float CellSize = SpatialHashGrid.Settings.CellSize;
        
		// Draw occupied cells
		for (const auto& BucketPair : SpatialHashGrid.SpatialBuckets)
		{
			const FIntVector& GridCoords = BucketPair.Key;
			const TArray<FMassSpatialHashGrid::FEntityWithPosition>& Entities = BucketPair.Value;
            
			// Calculate cell world position (cell center)
			FVector CellCenter = FVector(GridCoords) * CellSize + FVector(CellSize * 0.5f);
            
			// Draw cell box
			DrawDebugBox(World, CellCenter, FVector(CellSize * 0.5f), FColor::Green, false, -1.0f, 0, 2.0f);
            
			// Draw entities in this cell
			for (const FMassSpatialHashGrid::FEntityWithPosition& Entry : Entities)
			{
				// Draw entity position
				DrawDebugSphere(World, Entry.Position, 32.0f, 8, FColor::Yellow, false, -1.0f, 0, 1.0f);
                
				// Draw line from entity to cell center
				DrawDebugLine(World, Entry.Position, CellCenter, FColor::Cyan, false, -1.0f, 0, 0.5f);
			}
			// Draw entity count at cell
			//FString CountText = FString::Printf(TEXT("%d"), Entities.Num());
			//DrawDebugString(World, CellCenter + FVector(0, 0, CellSize * 0.5f), CountText, nullptr, FColor::White, -1.0f, true);
		}
	}
#endif
    EntityQuery.ForEachEntityChunk(Context,
        [SpatialHashSubsystem, &SpatialHashGrid](FMassExecutionContext& Ctx)
        {
            const TConstArrayView<FTransformFragment> TransformList = Ctx.GetFragmentView<FTransformFragment>();
            TArrayView<FArcMassSpatialHashFragment> SpatialHashList = Ctx.GetMutableFragmentView<FArcMassSpatialHashFragment>();
            const FArcMassSpatialHashIndexConfig* IndexConfig = Ctx.GetConstSharedFragmentPtr<FArcMassSpatialHashIndexConfig>();

        	for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
        	{
                const FTransformFragment& Transform = TransformList[EntityIt];
                FArcMassSpatialHashFragment& SpatialHash = SpatialHashList[EntityIt];
                const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

                FVector CurrentPos = Transform.GetTransform().GetLocation();
                FIntVector NewGridCoords = SpatialHashGrid.WorldToGrid(CurrentPos);

                if (NewGridCoords != SpatialHash.GridCoords)
                {
                    SpatialHashGrid.RemoveEntity(Entity, SpatialHash.GridCoords);
                    SpatialHashGrid.AddEntity(Entity, CurrentPos);

                    if (IndexConfig)
                    {
                        for (const uint32 Key : IndexConfig->IndexKeys)
                        {
                            FMassSpatialHashGrid& IndexedGrid = SpatialHashSubsystem->GetOrCreateIndexedGrid(Key);
                            IndexedGrid.RemoveEntity(Entity, SpatialHash.GridCoords);
                            IndexedGrid.AddEntity(Entity, CurrentPos);
                        }
                    }

                    SpatialHash.GridCoords = NewGridCoords;
                }
				else
				{
					SpatialHashGrid.UpdateEntity(Entity, SpatialHash.GridCoords, CurrentPos);

                    if (IndexConfig)
                    {
                        for (const uint32 Key : IndexConfig->IndexKeys)
                        {
                            if (FMassSpatialHashGrid* IndexedGrid = SpatialHashSubsystem->GetIndexedGrid(Key))
                            {
                                IndexedGrid->UpdateEntity(Entity, SpatialHash.GridCoords, CurrentPos);
                            }
                        }
                    }
				}
            }
        });
}

//----------------------------------------------------------------------
// UArcMassSpatialHashObserver — adds entities to the spatial hash on creation
//----------------------------------------------------------------------

UArcMassSpatialHashObserver::UArcMassSpatialHashObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcMassSpatialHashFragment::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Add;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
}

void UArcMassSpatialHashObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddRequirement<FArcMassSpatialHashFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddConstSharedRequirement<FArcMassSpatialHashIndexConfig>(EMassFragmentPresence::Optional);
}

void UArcMassSpatialHashObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UArcMassSpatialHashSubsystem* SpatialHashSubsystem = World ? World->GetSubsystem<UArcMassSpatialHashSubsystem>() : nullptr;
	if (!SpatialHashSubsystem)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context,
		[SpatialHashSubsystem](FMassExecutionContext& Ctx)
		{
			FMassSpatialHashGrid& MainGrid = SpatialHashSubsystem->GetSpatialHashGrid();
			const TConstArrayView<FTransformFragment> TransformList = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FArcMassSpatialHashFragment> SpatialHashList = Ctx.GetMutableFragmentView<FArcMassSpatialHashFragment>();
			const FArcMassSpatialHashIndexConfig* IndexConfig = Ctx.GetConstSharedFragmentPtr<FArcMassSpatialHashIndexConfig>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FVector Position = TransformList[EntityIt].GetTransform().GetLocation();
				FArcMassSpatialHashFragment& SpatialHash = SpatialHashList[EntityIt];
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				SpatialHash.GridCoords = MainGrid.WorldToGrid(Position);
				MainGrid.AddEntity(Entity, Position);

				if (IndexConfig)
				{
					for (const uint32 Key : IndexConfig->IndexKeys)
					{
						SpatialHashSubsystem->GetOrCreateIndexedGrid(Key).AddEntity(Entity, Position);
					}
				}
			}
		});
}

//----------------------------------------------------------------------
// UArcMassSpatialHashRemoveObserver — removes entities from the spatial hash on destruction
//----------------------------------------------------------------------

UArcMassSpatialHashRemoveObserver::UArcMassSpatialHashRemoveObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcMassSpatialHashFragment::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Remove;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
}

void UArcMassSpatialHashRemoveObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcMassSpatialHashFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddConstSharedRequirement<FArcMassSpatialHashIndexConfig>(EMassFragmentPresence::Optional);
}

void UArcMassSpatialHashRemoveObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UArcMassSpatialHashSubsystem* SpatialHashSubsystem = World ? World->GetSubsystem<UArcMassSpatialHashSubsystem>() : nullptr;
	if (!SpatialHashSubsystem)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context,
		[SpatialHashSubsystem](FMassExecutionContext& Ctx)
		{
			FMassSpatialHashGrid& MainGrid = SpatialHashSubsystem->GetSpatialHashGrid();
			const TConstArrayView<FArcMassSpatialHashFragment> SpatialHashList = Ctx.GetFragmentView<FArcMassSpatialHashFragment>();
			const FArcMassSpatialHashIndexConfig* IndexConfig = Ctx.GetConstSharedFragmentPtr<FArcMassSpatialHashIndexConfig>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FArcMassSpatialHashFragment& SpatialHash = SpatialHashList[EntityIt];
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				const FIntVector& GridCoords = SpatialHash.GridCoords;

				MainGrid.RemoveEntity(Entity, GridCoords);

				if (IndexConfig)
				{
					for (const uint32 Key : IndexConfig->IndexKeys)
					{
						if (FMassSpatialHashGrid* IndexedGrid = SpatialHashSubsystem->GetIndexedGrid(Key))
						{
							IndexedGrid->RemoveEntity(Entity, GridCoords);
						}
					}
				}
			}
		});
}

//----------------------------------------------------------------------
// UArcMassSpatialHashTrait
//----------------------------------------------------------------------

void UArcMassSpatialHashTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.AddFragment<FArcMassSpatialHashFragment>();

	if (bMoveable)
	{
		BuildContext.AddTag<FArcMassSpatialHashMoveableTag>();
	}

	const bool bHasIndices = !IndexTags.IsEmpty() || IndexMassTags.Num() > 0 || IndexFragments.Num() > 0 || CombinationIndices.Num() > 0;
	if (bHasIndices)
	{
		FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
		FArcMassSpatialHashIndexConfig IndexConfig;

		// Individual gameplay tag keys
		for (const FGameplayTag& Tag : IndexTags)
		{
			IndexConfig.IndexKeys.AddUnique(ArcSpatialHash::HashKey(Tag));
		}

		// Individual struct type keys (mass tags + fragments)
		for (const FInstancedStruct& TagStruct : IndexMassTags)
		{
			if (const UScriptStruct* ScriptStruct = TagStruct.GetScriptStruct())
			{
				IndexConfig.IndexKeys.AddUnique(ArcSpatialHash::HashKey(ScriptStruct));
			}
		}
		for (const FInstancedStruct& FragStruct : IndexFragments)
		{
			if (const UScriptStruct* ScriptStruct = FragStruct.GetScriptStruct())
			{
				IndexConfig.IndexKeys.AddUnique(ArcSpatialHash::HashKey(ScriptStruct));
			}
		}

		// Combination keys — each entry produces a single combined hash
		for (const FArcMassSpatialHashCombinationIndex& Combo : CombinationIndices)
		{
			TArray<uint32, TInlineAllocator<8>> SubKeys;
			for (const FGameplayTag& Tag : Combo.Tags)
			{
				SubKeys.Add(ArcSpatialHash::HashKey(Tag));
			}
			for (const FInstancedStruct& TagStruct : Combo.MassTags)
			{
				if (const UScriptStruct* ScriptStruct = TagStruct.GetScriptStruct())
				{
					SubKeys.Add(ArcSpatialHash::HashKey(ScriptStruct));
				}
			}
			for (const FInstancedStruct& FragStruct : Combo.Fragments)
			{
				if (const UScriptStruct* ScriptStruct = FragStruct.GetScriptStruct())
				{
					SubKeys.Add(ArcSpatialHash::HashKey(ScriptStruct));
				}
			}
			if (SubKeys.Num() > 0)
			{
				IndexConfig.IndexKeys.AddUnique(ArcSpatialHash::CombineKeys(SubKeys));
			}
		}

		const FConstSharedStruct SharedConfig = EntityManager.GetOrCreateConstSharedFragment(IndexConfig);
		BuildContext.AddConstSharedFragment(SharedConfig);
	}
}