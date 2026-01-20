// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPerception.h"
#include "ArcMassPerceptionSystem.h"
#include "DrawDebugHelpers.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "ArcMass/ArcMassSpatialHashSubsystem.h"

//----------------------------------------------------------------------
// UArcMassPerceptionSubsystem
//----------------------------------------------------------------------

// UArcMassPerceptionSubsystem
//----------------------------------------------------------------------

void UArcMassPerceptionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UArcMassSpatialHashSubsystem>();
}

void UArcMassPerceptionSubsystem::Deinitialize()
{
    OnEntityPerceived.Empty();
    OnEntityLostFromPerception.Empty();
	
    Super::Deinitialize();
}

UMassEntitySubsystem* UArcMassPerceptionSubsystem::GetEntitySubsystem() const
{
    if (CachedEntitySubsystem.IsValid())
    {
        return CachedEntitySubsystem.Get();
    }
    
    UWorld* World = GetWorld();
    if (World)
    {
        const_cast<UArcMassPerceptionSubsystem*>(this)->CachedEntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
        return CachedEntitySubsystem.Get();
    }
    return nullptr;
}

void UArcMassPerceptionSubsystem::BroadcastEntityPerceived(FMassEntityHandle Perceiver, FMassEntityHandle Perceived, FGameplayTag SenseTag)
{
    OnEntityPerceived.FindOrAdd(Perceiver).Broadcast(Perceiver, Perceived, SenseTag);
}

void UArcMassPerceptionSubsystem::BroadcastEntityLostFromPerception(FMassEntityHandle Perceiver, FMassEntityHandle Perceived, FGameplayTag SenseTag)
{
    OnEntityLostFromPerception.FindOrAdd(Perceiver).Broadcast(Perceiver, Perceived, SenseTag);
}

//----------------------------------------------------------------------
// UArcMassPerceptionProcessorBase
//----------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_AI_Perception_Sense_Sight, "AI.Perception.Sense.Sight");
UE_DEFINE_GAMEPLAY_TAG(TAG_AI_Perception_Sense_Hearing, "AI.Perception.Sense.Hearing");

UE_DEFINE_GAMEPLAY_TAG(TAG_AI_Perception_Stimuli_Sight, "AI.Perception.Stimuli.Sight");
UE_DEFINE_GAMEPLAY_TAG(TAG_AI_Perception_Stimuli_Hearing, "AI.Perception.Stimuli.Hearing");


UArcMassPerceptionProcessorBase::UArcMassPerceptionProcessorBase()
    : PerceptionQuery(*this)
{
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
    ExecutionOrder.ExecuteAfter.Add(TEXT("ArcMassSpatialHashUpdateProcessor"));
}

bool UArcMassPerceptionProcessorBase::PassesFilters(
    const FMassEntityManager& EntityManager,
    FMassEntityHandle Entity,
    const FArcPerceptionSenseConfigFragment& Config)
{
    // Check stimuli tags
    if (!Config.RequiredStimuliTags.IsEmpty())
    {
        const FArcMassPerceivableStimuliFragment* StimuliFragment = EntityManager.GetFragmentDataPtr<FArcMassPerceivableStimuliFragment>(Entity);
        if (!StimuliFragment || !StimuliFragment->StimuliTags.HasAny(Config.RequiredStimuliTags))
        {
            return false;
        }
    }

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
void UArcMassPerceptionProcessorBase::DrawDebugPerception(
    UWorld* World,
    const FVector& Location,
    const FQuat& Rotation,
    const FArcPerceptionSenseConfigFragment& Config,
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

//----------------------------------------------------------------------
// UArcMassSightPerceptionProcessor
//----------------------------------------------------------------------

UArcMassSightPerceptionProcessor::UArcMassSightPerceptionProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
    SenseTag = TAG_AI_Perception_Sense_Sight;
}

void UArcMassSightPerceptionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    PerceptionQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    PerceptionQuery.AddRequirement<FArcMassSightPerceptionResult>(EMassFragmentAccess::ReadWrite);
	PerceptionQuery.AddSharedRequirement<FArcPerceptionSenseConfigFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::All);
}

void UArcMassSightPerceptionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = EntityManager.GetWorld();
    if (!World)
    {
        return;
    }

    UArcMassSpatialHashSubsystem* SpatialHash = World->GetSubsystem<UArcMassSpatialHashSubsystem>();
    UArcMassPerceptionSubsystem* PerceptionSubsystem = World->GetSubsystem<UArcMassPerceptionSubsystem>();

    if (!SpatialHash || !PerceptionSubsystem)
    {
        return;
    }

    const float DeltaTime = Context.GetDeltaTimeSeconds();

    PerceptionQuery.ForEachEntityChunk(Context,
        [this, &EntityManager, SpatialHash, PerceptionSubsystem, DeltaTime](FMassExecutionContext& Ctx)
        {
        	const FArcPerceptionSenseConfigFragment& Config = Ctx.GetConstSharedFragment<FArcPerceptionSenseConfigFragment>();
        		
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
    UArcMassPerceptionSubsystem* PerceptionSubsystem,
    float DeltaTime,
    const TConstArrayView<FTransformFragment>& TransformList,
    const FArcPerceptionSenseConfigFragment& Config,
    TArrayView<FArcMassPerceptionResultFragmentBase> ResultList)
{
    const FMassSpatialHashGrid& Grid = SpatialHash->GetSpatialHashGrid();

    for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
    {
        const FTransformFragment& Transform = TransformList[EntityIt];
        FArcMassPerceptionResultFragmentBase& Result = ResultList[EntityIt];
        
        const FMassEntityHandle Entity = Context.GetEntity(EntityIt);

        // Update time since last seen
        for (FArcPerceivedEntity& PE : Result.PerceivedEntities)
        {
            PE.TimeSinceLastSeen += DeltaTime;
        }

        // Check update interval
        Result.TimeSinceLastUpdate += DeltaTime;
        if (Config.UpdateInterval > 0.0f && Result.TimeSinceLastUpdate < Config.UpdateInterval)
        {
#if WITH_GAMEPLAY_DEBUGGER
            if (Config.bEnableDebugDraw)
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
        TArray<TTuple<FMassEntityHandle, float>> QueriedEntities;

        if (Config.ShapeType == EArcPerceptionShapeType::Radius)
        {
            TArray<TPair<FMassEntityHandle, float>> RadiusResults;
            Grid.QueryEntitiesInRadiusWithDistance(Location, Config.Radius, RadiusResults);
            for (const auto& Pair : RadiusResults)
            {
                QueriedEntities.Add(MakeTuple(Pair.Key, Pair.Value));
            }
        }
        else
        {
            const float HalfAngleRadians = FMath::DegreesToRadians(Config.ConeHalfAngleDegrees);
            Grid.QueryEntitiesInConeWithDistance(Location, Forward, Config.ConeLength, HalfAngleRadians, QueriedEntities);
        }

        // Build new perceived list
        TSet<FMassEntityHandle> NewlyPerceivedSet;
        TArray<FArcPerceivedEntity> NewPerceivedList;

        for (const auto& Tuple : QueriedEntities)
        {
            FMassEntityHandle QueriedEntity = Tuple.Get<0>();
            float Distance = Tuple.Get<1>();

            if (QueriedEntity == Entity)
            {
                continue;
            }

            if (!EntityManager.IsEntityValid(QueriedEntity))
            {
                continue;
            }

            if (!EntityManager.DoesEntityHaveElement(QueriedEntity, FArcMassPerceivableTag::StaticStruct()))
            {
                continue;
            }

        	if (!EntityManager.DoesEntityHaveElement(QueriedEntity, FArcMassPerceivableStimuliSightFragment::StaticStruct()))
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
            PerceivedData.TimeSinceLastSeen = 0.0f;

            if (const FTransformFragment* TargetTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(QueriedEntity))
            {
                PerceivedData.LastKnownLocation = TargetTransform->GetTransform().GetLocation();
            }

            NewPerceivedList.Add(PerceivedData);
        }

        // Determine added/removed
        TSet<FMassEntityHandle> PreviouslyPerceivedSet;
        for (const FArcPerceivedEntity& PE : Result.PerceivedEntities)
        {
            PreviouslyPerceivedSet.Add(PE.Entity);
        }

        for (FMassEntityHandle NewEntity : NewlyPerceivedSet)
        {
            if (!PreviouslyPerceivedSet.Contains(NewEntity))
            {
                PerceptionSubsystem->BroadcastEntityPerceived(Entity, NewEntity, SenseTag);
            }
        }

        for (FMassEntityHandle OldEntity : PreviouslyPerceivedSet)
        {
            if (!NewlyPerceivedSet.Contains(OldEntity))
            {
                PerceptionSubsystem->BroadcastEntityLostFromPerception(Entity, OldEntity, SenseTag);
            }
        }

        Result.PerceivedEntities = MoveTemp(NewPerceivedList);

#if WITH_GAMEPLAY_DEBUGGER
        if (Config.bEnableDebugDraw)
        {
            DrawDebugPerception(EntityManager.GetWorld(), Location, Transform.GetTransform().GetRotation(), Config, Result);
        }
#endif
    }
}

//----------------------------------------------------------------------
// UArcMassHearingPerceptionProcessor
//----------------------------------------------------------------------

UArcMassHearingPerceptionProcessor::UArcMassHearingPerceptionProcessor()
	: HearingQuery(*this)
{
    SenseTag = TAG_AI_Perception_Sense_Hearing;
}

void UArcMassHearingPerceptionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    PerceptionQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    PerceptionQuery.AddRequirement<FArcMassHearingPerceptionResult>(EMassFragmentAccess::ReadWrite);
	PerceptionQuery.AddSharedRequirement<FArcPerceptionSenseConfigFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::All);
	
	HearingQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	HearingQuery.AddRequirement<FArcMassPerceivableStimuliHearingFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcMassHearingPerceptionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = EntityManager.GetWorld();
    if (!World)
    {
        return;
    }

    UArcMassSpatialHashSubsystem* SpatialHash = World->GetSubsystem<UArcMassSpatialHashSubsystem>();
    UArcMassPerceptionSubsystem* PerceptionSubsystem = World->GetSubsystem<UArcMassPerceptionSubsystem>();

    if (!SpatialHash || !PerceptionSubsystem)
    {
        return;
    }

    const float DeltaTime = Context.GetDeltaTimeSeconds();

	
    PerceptionQuery.ForEachEntityChunk(Context,
        [this, &EntityManager, SpatialHash, PerceptionSubsystem, DeltaTime, World](FMassExecutionContext& Ctx)
        {
        	const FArcPerceptionSenseConfigFragment& Config = Ctx.GetConstSharedFragment<FArcPerceptionSenseConfigFragment>();
        		
            const TConstArrayView<FTransformFragment> TransformList = Ctx.GetFragmentView<FTransformFragment>();
            TArrayView<FArcMassHearingPerceptionResult> ResultList = Ctx.GetMutableFragmentView<FArcMassHearingPerceptionResult>();

            TArrayView<FArcMassPerceptionResultFragmentBase> BaseResultList(
                reinterpret_cast<FArcMassPerceptionResultFragmentBase*>(ResultList.GetData()),
                ResultList.Num());

            ProcessPerceptionChunk(EntityManager, Ctx, SpatialHash
            	, PerceptionSubsystem, DeltaTime, TransformList
            	, Config, BaseResultList);
        });
}

void UArcMassHearingPerceptionProcessor::ProcessPerceptionChunk(
    FMassEntityManager& EntityManager,
    FMassExecutionContext& Context,
    UArcMassSpatialHashSubsystem* SpatialHash,
    UArcMassPerceptionSubsystem* PerceptionSubsystem,
    float DeltaTime,
    const TConstArrayView<FTransformFragment>& TransformList,
    const FArcPerceptionSenseConfigFragment& Config,
    TArrayView<FArcMassPerceptionResultFragmentBase> ResultList)
{
    const FMassSpatialHashGrid& Grid = SpatialHash->GetSpatialHashGrid();

	const double CurrentWorldTime = EntityManager.GetWorld()->GetTimeSeconds();
	
    for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
    {
        const FTransformFragment& Transform = TransformList[EntityIt];
        FArcMassPerceptionResultFragmentBase& Result = ResultList[EntityIt];
        
        const FMassEntityHandle Entity = Context.GetEntity(EntityIt);

        // Update time since last seen
        for (FArcPerceivedEntity& PE : Result.PerceivedEntities)
        {
            PE.TimeSinceLastSeen += DeltaTime;
        }

        // Check update interval
        Result.TimeSinceLastUpdate += DeltaTime;
        if (Config.UpdateInterval > 0.0f && Result.TimeSinceLastUpdate < Config.UpdateInterval)
        {
#if WITH_GAMEPLAY_DEBUGGER
            if (Config.bEnableDebugDraw)
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
        TArray<TTuple<FMassEntityHandle, float>> QueriedEntities;

        if (Config.ShapeType == EArcPerceptionShapeType::Radius)
        {
            TArray<TPair<FMassEntityHandle, float>> RadiusResults;
            Grid.QueryEntitiesInRadiusWithDistance(Location, Config.Radius, RadiusResults);
            for (const auto& Pair : RadiusResults)
            {
                QueriedEntities.Add(MakeTuple(Pair.Key, Pair.Value));
            }
        }
        else
        {
            const float HalfAngleRadians = FMath::DegreesToRadians(Config.ConeHalfAngleDegrees);
            Grid.QueryEntitiesInConeWithDistance(Location, Forward, Config.ConeLength, HalfAngleRadians, QueriedEntities);
        }

        // Build new perceived list
        TSet<FMassEntityHandle> NewlyPerceivedSet;
        TArray<FArcPerceivedEntity> NewPerceivedList;

        for (const auto& Tuple : QueriedEntities)
        {
            FMassEntityHandle QueriedEntity = Tuple.Get<0>();
            float Distance = Tuple.Get<1>();

            if (QueriedEntity == Entity)
            {
                continue;
            }

            if (!EntityManager.IsEntityValid(QueriedEntity))
            {
                continue;
            }

            if (!EntityManager.DoesEntityHaveElement(QueriedEntity, FArcMassPerceivableTag::StaticStruct()))
            {
                continue;
            }

        	if (!EntityManager.DoesEntityHaveElement(QueriedEntity, FArcMassPerceivableStimuliHearingFragment::StaticStruct()))
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
            PerceivedData.TimeSinceLastSeen = 0.0f;

            if (const FTransformFragment* TargetTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(QueriedEntity))
            {
                PerceivedData.LastKnownLocation = TargetTransform->GetTransform().GetLocation();
            }

        	FArcMassPerceivableStimuliHearingFragment& HearingStimuli = EntityManager.GetFragmentDataChecked<FArcMassPerceivableStimuliHearingFragment>(QueriedEntity);
        	const float TimeHeard = CurrentWorldTime - HearingStimuli.EmissionTime;
        	if (TimeHeard > 0.5)
        	{
        		continue;
        	}
        	
        	const float FinalStrength = HearingStimuli.EmissionStrength * (1.0f - (Distance / Config.Radius));
        	PerceivedData.Strength = FinalStrength;
        	
            NewPerceivedList.Add(PerceivedData);
        }

        // Determine added/removed
        TSet<FMassEntityHandle> PreviouslyPerceivedSet;
        for (const FArcPerceivedEntity& PE : Result.PerceivedEntities)
        {
            PreviouslyPerceivedSet.Add(PE.Entity);
        }

        for (FMassEntityHandle NewEntity : NewlyPerceivedSet)
        {
            if (!PreviouslyPerceivedSet.Contains(NewEntity))
            {
                PerceptionSubsystem->BroadcastEntityPerceived(Entity, NewEntity, SenseTag);
            }
        }

        for (FMassEntityHandle OldEntity : PreviouslyPerceivedSet)
        {
            if (!NewlyPerceivedSet.Contains(OldEntity))
            {
                PerceptionSubsystem->BroadcastEntityLostFromPerception(Entity, OldEntity, SenseTag);
            }
        }

        Result.PerceivedEntities = MoveTemp(NewPerceivedList);

#if WITH_GAMEPLAY_DEBUGGER
        if (Config.bEnableDebugDraw)
        {
            DrawDebugPerception(EntityManager.GetWorld(), Location, Transform.GetTransform().GetRotation(), Config, Result);
        }
#endif
    }
}

//----------------------------------------------------------------------
// UArcMassPerceptionObserver
//----------------------------------------------------------------------

UArcMassPerceptionObserver::UArcMassPerceptionObserver()
    : ObserverQuery(*this)
{
    ObservedType = FArcMassPerceivableTag::StaticStruct();
    ObservedOperations = EMassObservedOperationFlags::Remove;
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
}

void UArcMassPerceptionObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcMassPerceptionObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = EntityManager.GetWorld();
    if (!World)
    {
        return;
    }

    UArcMassPerceptionSubsystem* PerceptionSubsystem = World->GetSubsystem<UArcMassPerceptionSubsystem>();
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

    // Clean up hearing results
    {
        FMassEntityQuery CleanupQuery;
        CleanupQuery.AddRequirement<FArcMassHearingPerceptionResult>(EMassFragmentAccess::ReadWrite);

        CleanupQuery.ForEachEntityChunk(Context,
            [&DestroyedEntities, PerceptionSubsystem](FMassExecutionContext& Ctx)
            {
                TArrayView<FArcMassHearingPerceptionResult> ResultList = Ctx.GetMutableFragmentView<FArcMassHearingPerceptionResult>();

                for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
                {
                    FArcMassHearingPerceptionResult& Result = ResultList[EntityIt];
                    FMassEntityHandle PerceiverEntity = Ctx.GetEntity(EntityIt);

                    for (int32 i = Result.PerceivedEntities.Num() - 1; i >= 0; --i)
                    {
                        if (DestroyedEntities.Contains(Result.PerceivedEntities[i].Entity))
                        {
                            PerceptionSubsystem->BroadcastEntityLostFromPerception(
                                PerceiverEntity,
                                Result.PerceivedEntities[i].Entity,
                                TAG_AI_Perception_Sense_Hearing);

                            Result.PerceivedEntities.RemoveAtSwap(i);
                        }
                    }
                }
            });
    }
}