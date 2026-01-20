// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassSpatialHashSubsystem.h"

#include "DrawDebugHelpers.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

void FMassSpatialHashGrid::UpdateEntity(FMassEntityHandle EntityHandle, const FIntVector& OldGridCoords, const FVector& NewPosition)
{
	FIntVector NewGridCoords = WorldToGrid(NewPosition);
    
	//if (OldGridCoords == NewGridCoords)
	{
		// Same cell, just update position
		if (TArray<FEntityWithPosition>* Bucket = SpatialBuckets.Find(OldGridCoords))
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
	//else
	//{
	//	// Cell changed, remove and re-add
	//	RemoveEntity(EntityHandle, OldGridCoords);
	//	AddEntity(EntityHandle, NewPosition);
	//}
}

void FMassSpatialHashGrid::QueryEntitiesInRadius(const FVector& Center, float Radius, TArray<FMassEntityHandle>& OutEntityHandles) const
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
							OutEntityHandles.Add(Entry.Entity);
						}
					}
				}
			}
		}
	}
}

void FMassSpatialHashGrid::QueryEntitiesInRadiusWithDistance(const FVector& Center, float Radius
															 , TArray<TPair<FMassEntityHandle, float>>& OutEntitiesWithDistance) const
{
	OutEntitiesWithDistance.Reset();
        
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
							OutEntitiesWithDistance.Add({Entry.Entity, FMath::Sqrt(DistSq)});
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
	// The cone's max radius at its base
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
														   , TArray<TTuple<FMassEntityHandle, float>>& OutEntitiesWithDistance) const
{
	OutEntitiesWithDistance.Reset();
		
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
	const float MinDistSqThreshold = MinDistanceThreshold * MinDistanceThreshold;
		
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
							OutEntitiesWithDistance.Add(MakeTuple(Entry.Entity, FMath::Sqrt(DistSq)));
							continue;
						}
		
						const float Dist = FMath::Sqrt(DistSq);
						
						FVector ToEntityNormalized = ToEntity.GetSafeNormal2D();
						const float DotProduct = FVector::DotProduct(Direction.GetSafeNormal2D(), ToEntityNormalized);
		
						if (DotProduct >= CosHalfAngle)
						{
							OutEntitiesWithDistance.Add(MakeTuple(Entry.Entity, Dist));
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
	Super::Deinitialize();
}

void UArcMassSpatialHashSubsystem::SetSpatialHashSettings(const FMassSpatialHashSettings& NewSettings)
{
	SpatialHashGrid.Settings = NewSettings;
	// Clear existing data since cell size changed
	SpatialHashGrid.Clear();
}

TArray<FMassEntityHandle> UArcMassSpatialHashSubsystem::QueryEntitiesInRadius(const FVector& Center, float Radius) const
{
	TArray<FMassEntityHandle> Results;
	SpatialHashGrid.QueryEntitiesInRadius(Center, Radius, Results);
	return Results;
}

UArcMassSpatialHashUpdateProcessor::UArcMassSpatialHashUpdateProcessor()
	: EntityQuery{*this}
{
	//bAutoRegisterWithProcessingPhases = true;
    ProcessingPhase = EMassProcessingPhase::PrePhysics;
    ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	//bRequiresGameThreadExecution = true;
	
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::SyncWorldToMass);
}

void UArcMassSpatialHashUpdateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FArcMassSpatialHashFragment>(EMassFragmentAccess::ReadWrite);
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
	//if (CVarDebugDrawSpatialHash.GetValueOnGameThread())
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
				DrawDebugSphere(World, Entry.Position, 64.0f, 8, FColor::Yellow, false, -1.0f, 0, 1.0f);
                
				// Draw line from entity to cell center
				DrawDebugLine(World, Entry.Position, CellCenter, FColor::Cyan, false, -1.0f, 0, 0.5f);
			}
			// Draw entity count at cell
			//FString CountText = FString::Printf(TEXT("%d"), Entities.Num());
			//DrawDebugString(World, CellCenter + FVector(0, 0, CellSize * 0.5f), CountText, nullptr, FColor::White, -1.0f, true);
		}
	}
#endif
    EntityQuery.ForEachEntityChunk(EntityManager, Context, 
        [&SpatialHashGrid](FMassExecutionContext& Ctx)
        {
            const TConstArrayView<FTransformFragment> TransformList = Ctx.GetFragmentView<FTransformFragment>();
            TArrayView<FArcMassSpatialHashFragment> SpatialHashList = Ctx.GetMutableFragmentView<FArcMassSpatialHashFragment>();
            
        	for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
        	{
                const FTransformFragment& Transform = TransformList[EntityIt];
                FArcMassSpatialHashFragment& SpatialHash = SpatialHashList[EntityIt];
                
                FVector CurrentPos = Transform.GetTransform().GetLocation();
                FIntVector NewGridCoords = SpatialHashGrid.WorldToGrid(CurrentPos);
                
                // If entity moved to a different cell
                if (NewGridCoords != SpatialHash.GridCoords)
                {
                    SpatialHashGrid.RemoveEntity(Ctx.GetEntity(EntityIt), SpatialHash.GridCoords);
                    // Add to new cell
                    SpatialHashGrid.AddEntity(Ctx.GetEntity(EntityIt), CurrentPos);
                    // Update fragment
                    SpatialHash.GridCoords = NewGridCoords;
                }
				else
				{
					SpatialHashGrid.UpdateEntity(Ctx.GetEntity(EntityIt), SpatialHash.GridCoords, CurrentPos);
				}
            }
        });
}