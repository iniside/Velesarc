#include "ArcMassSightPerception.h"

#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"
#include "ArcMass/ArcMassSpatialHashSubsystem.h"

#include "MassEntitySubsystem.h"

#include "Engine/World.h"

void UArcPerceptionSightPerceiverTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.AddFragment<FArcMassSightPerceptionResult>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(SightConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}

void UArcPerceptionSightPerceivableTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FArcMassSpatialHashFragment>();
	BuildContext.AddTag<FArcMassSightPerceivableTag>();
}

void UArcMassSightPerceptionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UArcMassSpatialHashSubsystem>();
}

void UArcMassSightPerceptionSubsystem::Deinitialize()
{
	OnEntityPerceived.Empty();
	OnEntityLostFromPerception.Empty();
	OnPerceptionUpdated.Empty();

	Super::Deinitialize();
}

UMassEntitySubsystem* UArcMassSightPerceptionSubsystem::GetEntitySubsystem() const
{
	if (CachedEntitySubsystem.IsValid())
	{
		return CachedEntitySubsystem.Get();
	}

	UWorld* World = GetWorld();
	if (World)
	{
		const_cast<UArcMassSightPerceptionSubsystem*>(this)->CachedEntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
		return CachedEntitySubsystem.Get();
	}
	return nullptr;
}

void UArcMassSightPerceptionSubsystem::BroadcastEntityPerceived(FMassEntityHandle Perceiver, FMassEntityHandle Perceived, FGameplayTag SenseTag)
{
	if (FArcPerceptionEntityAddedNative* Delegate = OnEntityPerceived.Find(Perceiver))
	{
		Delegate->Broadcast(Perceiver, Perceived, SenseTag);
	}
}

void UArcMassSightPerceptionSubsystem::BroadcastEntityLostFromPerception(FMassEntityHandle Perceiver, FMassEntityHandle Perceived, FGameplayTag SenseTag)
{
	if (FArcPerceptionEntityAddedNative* Delegate = OnEntityLostFromPerception.Find(Perceiver))
	{
		Delegate->Broadcast(Perceiver, Perceived, SenseTag);
	}
}

//----------------------------------------------------------------------
// UArcMassSightPerceptionProcessor
//----------------------------------------------------------------------

UArcMassSightPerceptionProcessor::UArcMassSightPerceptionProcessor()
	: PerceptionQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	ExecutionOrder.ExecuteAfter.Add(TEXT("ArcMassSpatialHashUpdateProcessor"));
}

void UArcMassSightPerceptionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    PerceptionQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    PerceptionQuery.AddRequirement<FArcMassSightPerceptionResult>(EMassFragmentAccess::ReadWrite);
	PerceptionQuery.AddConstSharedRequirement<FArcPerceptionSightSenseConfigFragment>(EMassFragmentPresence::All);
}

void UArcMassSightPerceptionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = EntityManager.GetWorld();
    if (!World)
    {
        return;
    }

    UArcMassSpatialHashSubsystem* SpatialHash = World->GetSubsystem<UArcMassSpatialHashSubsystem>();
    UArcMassSightPerceptionSubsystem* PerceptionSubsystem = World->GetSubsystem<UArcMassSightPerceptionSubsystem>();

    if (!SpatialHash || !PerceptionSubsystem)
    {
        return;
    }

    const float DeltaTime = Context.GetDeltaTimeSeconds();

    PerceptionQuery.ForEachEntityChunk(Context,
        [this, &EntityManager, SpatialHash, PerceptionSubsystem, DeltaTime](FMassExecutionContext& Ctx)
        {
        	const FArcPerceptionSightSenseConfigFragment& Config = Ctx.GetConstSharedFragment<FArcPerceptionSightSenseConfigFragment>();

            const TConstArrayView<FTransformFragment> TransformList = Ctx.GetFragmentView<FTransformFragment>();
            TArrayView<FArcMassSightPerceptionResult> ResultList = Ctx.GetMutableFragmentView<FArcMassSightPerceptionResult>();

            ProcessPerceptionChunk(EntityManager, Ctx, SpatialHash
            	, PerceptionSubsystem, DeltaTime, TransformList
            	, Config, ResultList);
        });
}

void UArcMassSightPerceptionProcessor::ProcessPerceptionChunk(
    FMassEntityManager& EntityManager,
    FMassExecutionContext& Context,
    UArcMassSpatialHashSubsystem* SpatialHash,
    UArcMassSightPerceptionSubsystem* PerceptionSubsystem,
    float DeltaTime,
    const TConstArrayView<FTransformFragment>& TransformList,
    const FArcPerceptionSightSenseConfigFragment& Config,
    TArrayView<FArcMassSightPerceptionResult> ResultList)
{
    const FMassSpatialHashGrid& Grid = SpatialHash->GetSpatialHashGrid();

	UWorld* World = EntityManager.GetWorld();
	double CurrentTime = World->GetTimeSeconds();

    for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
    {
        const FTransformFragment& Transform = TransformList[EntityIt];
        FArcMassSightPerceptionResult& Result = ResultList[EntityIt];

        const FMassEntityHandle Entity = Context.GetEntity(EntityIt);

    	// Check update interval
        Result.TimeSinceLastUpdate += DeltaTime;
        if (Config.UpdateInterval > 0.0f && Result.TimeSinceLastUpdate < Config.UpdateInterval)
        {
            continue;
        }
        Result.TimeSinceLastUpdate = 0.0f;

        FVector Location = Transform.GetTransform().GetLocation();
        Location.Z += Config.EyeOffset;
        const FVector Forward = Transform.GetTransform().GetRotation().GetForwardVector();

        // Query based on shape type from config
    	TArray<FArcMassEntityInfo> QueriedEntities;

        if (Config.ShapeType == EArcPerceptionShapeType::Radius)
        {
            Grid.QueryEntitiesInRadiusWithDistance(Location, Config.Radius, QueriedEntities);
        }
        else
        {
            const float HalfAngleRadians = FMath::DegreesToRadians(Config.ConeHalfAngleDegrees);
            Grid.QueryEntitiesInConeWithDistance(Location, Forward, Config.ConeLength, HalfAngleRadians, QueriedEntities);
        }

        // Build new perceived list
        TSet<FMassEntityHandle> NewlyPerceivedSet;
        TArray<FArcPerceivedEntity> NewPerceivedList;
    	TMap<FMassEntityHandle, int32> CurrentEntities;
    	TArray<FMassEntityHandle> RemovedEntities;

    	CurrentEntities.Reserve(Result.PerceivedEntities.Num());
    	for (int32 Idx = Result.PerceivedEntities.Num() - 1; Idx >= 0; --Idx)
    	{
    		const float TimeSinceLastSeen = CurrentTime - Result.PerceivedEntities[Idx].LastTimeSeen;
    		if (TimeSinceLastSeen > Config.ForgetTime)
    		{
    			RemovedEntities.Add(Result.PerceivedEntities[Idx].Entity);
    			Result.PerceivedEntities.RemoveAt(Idx);
    		}
    	}

    	for (int32 Idx = Result.PerceivedEntities.Num() - 1; Idx >= 0; --Idx)
    	{
    		CurrentEntities.Add(Result.PerceivedEntities[Idx].Entity, Idx);
    	}

        for (const FArcMassEntityInfo& EntityInfo : QueriedEntities)
        {
            FMassEntityHandle QueriedEntity = EntityInfo.Entity;
            float Distance = EntityInfo.Distance;

            if (QueriedEntity == Entity)
            {
                continue;
            }

        	if (int32* ExistingIdx = CurrentEntities.Find(QueriedEntity))
        	{
        		const FTransformFragment& TargetTransform = EntityManager.GetFragmentDataChecked<FTransformFragment>(QueriedEntity);
        		Result.PerceivedEntities[*ExistingIdx].LastTimeSeen = CurrentTime;
        		Result.PerceivedEntities[*ExistingIdx].TimeSinceLastPerceived = 0.0f;
        		Result.PerceivedEntities[*ExistingIdx].Distance = Distance;
        		Result.PerceivedEntities[*ExistingIdx].TimePerceived += DeltaTime;
        		Result.PerceivedEntities[*ExistingIdx].LastKnownLocation = TargetTransform.GetTransform().GetLocation();
        		continue;
        	}

            if (!EntityManager.IsEntityValid(QueriedEntity))
            {
                continue;
            }

            if (!EntityManager.DoesEntityHaveElement(QueriedEntity, FArcMassSightPerceivableTag::StaticStruct()))
            {
                continue;
            }

            if (!ArcPerception::PassesFilters(EntityManager, QueriedEntity, Config))
            {
                continue;
            }

            NewlyPerceivedSet.Add(QueriedEntity);

            FArcPerceivedEntity PerceivedData;
            PerceivedData.Entity = QueriedEntity;
            PerceivedData.Distance = Distance;
            PerceivedData.TimeSinceLastPerceived = 0.0f;
        	PerceivedData.LastTimeSeen = CurrentTime;
			PerceivedData.TimeFirstPerceived = CurrentTime;
        	PerceivedData.TimePerceived = 0.f;

            if (const FTransformFragment* TargetTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(QueriedEntity))
            {
                PerceivedData.LastKnownLocation = TargetTransform->GetTransform().GetLocation();
            }
			NewPerceivedList.Add(PerceivedData);
        }

    	// Broadcast events before modifying the list
    	for (const FMassEntityHandle& NewEntity : NewlyPerceivedSet)
    	{
    		PerceptionSubsystem->BroadcastEntityPerceived(Entity, NewEntity, TAG_AI_Perception_Sense_Sight);
    	}

    	for (const FMassEntityHandle& OldEntity : RemovedEntities)
    	{
    		if (!NewlyPerceivedSet.Contains(OldEntity))
    		{
    			PerceptionSubsystem->BroadcastEntityLostFromPerception(Entity, OldEntity, TAG_AI_Perception_Sense_Sight);
    		}
    	}

    	// Append newly perceived entities to the result
    	Result.PerceivedEntities.Append(NewPerceivedList);

    	const bool bChanged = NewPerceivedList.Num() > 0 || RemovedEntities.Num() > 0;
    	if (bChanged)
    	{
    		if (FArcPerceptionEntityList* Delegate = PerceptionSubsystem->OnPerceptionUpdated.Find(Entity))
    		{
    			Delegate->Broadcast(Entity, Result.PerceivedEntities, TAG_AI_Perception_Sense_Sight);
    		}
    	}
    }
}


UArcMassSightPerceptionObserver::UArcMassSightPerceptionObserver()
    : ObserverQuery(*this)
{
    ObservedType = FArcMassSightPerceivableTag::StaticStruct();
    ObservedOperations = EMassObservedOperationFlags::Remove;
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
}

void UArcMassSightPerceptionObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcMassSightPerceptionObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
   	UWorld* World = EntityManager.GetWorld();
   	if (!World)
   	{
   	    return;
   	}

    UArcMassSightPerceptionSubsystem* PerceptionSubsystem = World->GetSubsystem<UArcMassSightPerceptionSubsystem>();
    if (!PerceptionSubsystem)
    {
        return;
    }

    TArray<FMassEntityHandle> DestroyedEntities;

    ObserverQuery.ForEachEntityChunk(Context,
        [&DestroyedEntities](FMassExecutionContext& Ctx)
        {
            for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
            {
                DestroyedEntities.Add(Ctx.GetEntity(EntityIt));
            }
        });
//
    if (DestroyedEntities.IsEmpty())
    {
        return;
    }

    // Clean up sight results
    {
        FMassEntityQuery CleanupQuery;
        CleanupQuery.AddRequirement<FArcMassSightPerceptionResult>(EMassFragmentAccess::ReadWrite);
	
        CleanupQuery.ForEachEntityChunk(Context,
            [&DestroyedEntities, PerceptionSubsystem](FMassExecutionContext& Ctx)
            {
                TArrayView<FArcMassSightPerceptionResult> ResultList = Ctx.GetMutableFragmentView<FArcMassSightPerceptionResult>();
	
                for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
                {
                    FArcMassSightPerceptionResult& Result = ResultList[EntityIt];
                    FMassEntityHandle PerceiverEntity = Ctx.GetEntity(EntityIt);
	
                    for (int32 i = Result.PerceivedEntities.Num() - 1; i >= 0; --i)
                    {
                        if (DestroyedEntities.Contains(Result.PerceivedEntities[i].Entity))
                        {
                            PerceptionSubsystem->BroadcastEntityLostFromPerception(
                                PerceiverEntity,
                                Result.PerceivedEntities[i].Entity,
                                TAG_AI_Perception_Sense_Sight);
	
                            Result.PerceivedEntities.RemoveAtSwap(i);
                        }
                    }
                }
            });
    }
}
