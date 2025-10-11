// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassSpatialHashSubsystem.h"

#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

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
    ProcessingPhase = EMassProcessingPhase::PrePhysics;
    ExecutionFlags = (int32)EProcessorExecutionFlags::All;

	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Movement);
}

void UArcMassSpatialHashUpdateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassSpatialHashFragment>(EMassFragmentAccess::ReadWrite);
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
    
    EntityQuery.ForEachEntityChunk(EntityManager, Context, 
        [&SpatialHashGrid](FMassExecutionContext& Ctx)
        {
            const TConstArrayView<FTransformFragment> TransformList = Ctx.GetFragmentView<FTransformFragment>();
            TArrayView<FMassSpatialHashFragment> SpatialHashList = Ctx.GetMutableFragmentView<FMassSpatialHashFragment>();
            
        		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
        		{
                const FTransformFragment& Transform = TransformList[EntityIt];
                FMassSpatialHashFragment& SpatialHash = SpatialHashList[EntityIt];
                
                FVector CurrentPos = Transform.GetTransform().GetLocation();
                FIntVector NewGridCoords = SpatialHashGrid.WorldToGrid(CurrentPos);
                uint32 NewCellHash = SpatialHashGrid.HashGridCoords(NewGridCoords);
                
                // If entity moved to a different cell
                if (NewCellHash != SpatialHash.CellHash)
                {
                    // Remove from old cell
                    if (SpatialHash.CellHash != 0)
                    {
                        SpatialHashGrid.RemoveEntity(Ctx.GetEntity(EntityIt), SpatialHash.CellHash);
                    }
                    
                    // Add to new cell
                    SpatialHashGrid.AddEntity(Ctx.GetEntity(EntityIt), CurrentPos);
                    
                    // Update fragment
                    SpatialHash.GridCoords = NewGridCoords;
                    SpatialHash.CellHash = NewCellHash;
                }
            }
        });
}