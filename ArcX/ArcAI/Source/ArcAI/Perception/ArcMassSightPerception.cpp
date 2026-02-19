#include "ArcMassSightPerception.h"

#include "DrawDebugHelpers.h"
#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"
#include "ArcMass/ArcMassSpatialHashSubsystem.h"

#include "MassEntitySubsystem.h"

#include "MassEntityFragments.h"

#include "Engine/World.h"

void UArcPerceptionSightTraitBase::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FArcMassSpatialHashFragment>();
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.AddFragment<FArcMassSightPerceptionResult>();
	
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct StateTreeFragment = EntityManager.GetOrCreateConstSharedFragment(SightConfig);
	BuildContext.AddConstSharedFragment(StateTreeFragment);
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
	OnEntityPerceived.FindOrAdd(Perceiver).Broadcast(Perceiver, Perceived, SenseTag);
}

void UArcMassSightPerceptionSubsystem::BroadcastEntityLostFromPerception(FMassEntityHandle Perceiver, FMassEntityHandle Perceived, FGameplayTag SenseTag)
{
	OnEntityLostFromPerception.FindOrAdd(Perceiver).Broadcast(Perceiver, Perceived, SenseTag);
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

            TArrayView<FArcMassPerceptionResultFragmentBase> BaseResultList(
                reinterpret_cast<FArcMassPerceptionResultFragmentBase*>(ResultList.GetData()),
                ResultList.Num());

            ProcessPerceptionChunk(EntityManager, Ctx, SpatialHash
            	, PerceptionSubsystem, DeltaTime, TransformList
            	, Config, BaseResultList);
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
    TArrayView<FArcMassPerceptionResultFragmentBase> ResultList)
{
    const FMassSpatialHashGrid& Grid = SpatialHash->GetSpatialHashGrid();

	UWorld* World = EntityManager.GetWorld();
	double CurrentTime = World->GetTimeSeconds();
	
    for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
    {
        const FTransformFragment& Transform = TransformList[EntityIt];
        FArcMassPerceptionResultFragmentBase& Result = ResultList[EntityIt];
        
        const FMassEntityHandle Entity = Context.GetEntity(EntityIt);
        
    	// Check update interval
        Result.TimeSinceLastUpdate += DeltaTime;
        if (Config.UpdateInterval > 0.0f && Result.TimeSinceLastUpdate < Config.UpdateInterval)
        {
#if WITH_GAMEPLAY_DEBUGGER
            if (CVarArcDebugDrawPerception.GetValueOnAnyThread())
            {
                FVector Location = Transform.GetTransform().GetLocation();
                Location.Z += Config.EyeOffset;
                DrawDebugPerception(EntityManager.GetWorld(), Location, Transform.GetTransform().GetRotation(), Config, Result);
            }
#endif
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
            TArray<TPair<FMassEntityHandle, float>> RadiusResults;
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
    	for (int32 Idx = Result.PerceivedEntities.Num() - 1; Idx > 0; --Idx)
    	{
    		const float TimeSinceLastSeen = CurrentTime - Result.PerceivedEntities[Idx].LastTimeSeen;
    		if (TimeSinceLastSeen > Config.ForgetTime)
    		{
    			RemovedEntities.Add(Result.PerceivedEntities[Idx].Entity);
    			Result.PerceivedEntities.RemoveAt(Idx);
    		}
    	}
    	
    	for (int32 Idx = Result.PerceivedEntities.Num() - 1; Idx > 0; --Idx)
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

            if (!PassesFilters(EntityManager, QueriedEntity, Config))
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
    	
    	const bool bAnyNewEntities = NewPerceivedList.Num() > 0;
    	
    	Result.PerceivedEntities.Append(NewPerceivedList);
    	
        // Determine added/removed
        TSet<FMassEntityHandle> PreviouslyPerceivedSet;
        for (const FArcPerceivedEntity& PE : Result.PerceivedEntities)
        {
            PreviouslyPerceivedSet.Add(PE.Entity);
        }

        for (const FMassEntityHandle& NewEntity : NewlyPerceivedSet)
        {
            if (!Result.PerceivedEntities.Contains(NewEntity))
            {
                PerceptionSubsystem->BroadcastEntityPerceived(Entity, NewEntity, TAG_AI_Perception_Sense_Sight);
            }
        }

        for (const FMassEntityHandle& OldEntity : RemovedEntities)
        {
            if (!NewlyPerceivedSet.Contains(OldEntity))
            {
                PerceptionSubsystem->BroadcastEntityLostFromPerception(Entity, OldEntity, TAG_AI_Perception_Sense_Sight);
            }
        }

        Result.PerceivedEntities = MoveTemp(NewPerceivedList);

    	if (bAnyNewEntities)
    	{
    		if (FArcPerceptionEntityList* Delegate = PerceptionSubsystem->OnPerceptionUpdated.Find(Entity))
    		{
    			Delegate->Broadcast(Entity, Result.PerceivedEntities, TAG_AI_Perception_Sense_Sight);
    		}	
    	}
    	
#if WITH_GAMEPLAY_DEBUGGER
        if (CVarArcDebugDrawPerception.GetValueOnAnyThread())
        {
            DrawDebugPerception(EntityManager.GetWorld(), Location, Transform.GetTransform().GetRotation(), Config, Result);
        }
#endif
    }
}


bool UArcMassSightPerceptionProcessor::PassesFilters(
    const FMassEntityManager& EntityManager,
    FMassEntityHandle Entity,
    const FArcPerceptionSightSenseConfigFragment& Config)
{
    // Check general tags
    if (!Config.RequiredTags.IsEmpty() || !Config.IgnoredTags.IsEmpty())
    {
        const FArcMassGameplayTagContainerFragment* TagFragment = EntityManager.GetFragmentDataPtr<FArcMassGameplayTagContainerFragment>(Entity);

        if (!TagFragment)
        {
            return Config.RequiredTags.IsEmpty();
        }

        const FGameplayTagContainer& EntityTags = TagFragment->Tags;

        if (!Config.IgnoredTags.IsEmpty() && EntityTags.HasAny(Config.IgnoredTags))
        {
            return false;
        }

        if (!Config.RequiredTags.IsEmpty() && !EntityTags.HasAll(Config.RequiredTags))
        {
            return false;
        }
    }

    return true;
}

#if WITH_GAMEPLAY_DEBUGGER
void UArcMassSightPerceptionProcessor::DrawDebugPerception(
    UWorld* World,
    const FVector& Location,
    const FQuat& Rotation,
    const FArcPerceptionSightSenseConfigFragment& Config,
    const FArcMassPerceptionResultFragmentBase& Result)
{
    if (!World)
    {
        return;
    }

    constexpr float DebugLifetime = -1.0f;
    constexpr uint8 DepthPriority = 0;
    constexpr float Thickness = 2.0f;

    const FVector Forward = Rotation.GetForwardVector();

    if (Config.ShapeType == EArcPerceptionShapeType::Radius)
    {
        DrawDebugSphere(World, Location, Config.Radius, 16, Config.DebugShapeColor, false, DebugLifetime, DepthPriority, Thickness);
    }
    else
    {
    	const float HalfAngleRadians = FMath::DegreesToRadians(Config.ConeHalfAngleDegrees);
        
    	DrawDebugCone(
			World,
			Location,
			Forward,
			Config.ConeLength,
			HalfAngleRadians,
			HalfAngleRadians,
			16,
			Config.DebugShapeColor,
			false,
			DebugLifetime,
			DepthPriority,
			Thickness);
    }

    for (const FArcPerceivedEntity& PE : Result.PerceivedEntities)
    {
        DrawDebugLine(World, Location, PE.LastKnownLocation, Config.DebugPerceivedLineColor, false, DebugLifetime, DepthPriority, Thickness);
        DrawDebugSphere(World, PE.LastKnownLocation, 60.0f, 16, Config.DebugPerceivedLineColor, false, DebugLifetime, DepthPriority);
    }

    //DrawDebugString(World, Location + FVector(0, 0, 50), FString::Printf(TEXT("Perceived: %d"), Result.PerceivedEntities.Num()), nullptr, Config.DebugShapeColor, DebugLifetime, false, 1.0f);
}
#endif


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
