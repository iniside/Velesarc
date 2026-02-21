// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassHearingPerception.h"

#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"
#include "Engine/World.h"

#include "MassEntitySubsystem.h"

#include "ArcMass/ArcMassSpatialHashSubsystem.h"

//----------------------------------------------------------------------
// Traits
//----------------------------------------------------------------------

void UArcPerceptionHearingPerceiverTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.AddFragment<FArcMassHearingPerceptionResult>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(HearingConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}

void UArcPerceptionHearingPerceivableTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FArcMassSpatialHashFragment>();
	BuildContext.AddTag<FArcMassHearingPerceivableTag>();
	BuildContext.AddFragment<FArcMassPerceivableStimuliHearingFragment>();
}

//----------------------------------------------------------------------
// Subsystem
//----------------------------------------------------------------------

void UArcMassHearingPerceptionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UArcMassSpatialHashSubsystem>();
}

void UArcMassHearingPerceptionSubsystem::Deinitialize()
{
	OnEntityPerceived.Empty();
	OnEntityLostFromPerception.Empty();
	OnPerceptionUpdated.Empty();

	Super::Deinitialize();
}

UMassEntitySubsystem* UArcMassHearingPerceptionSubsystem::GetEntitySubsystem() const
{
	if (CachedEntitySubsystem.IsValid())
	{
		return CachedEntitySubsystem.Get();
	}

	UWorld* World = GetWorld();
	if (World)
	{
		const_cast<UArcMassHearingPerceptionSubsystem*>(this)->CachedEntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
		return CachedEntitySubsystem.Get();
	}
	return nullptr;
}

void UArcMassHearingPerceptionSubsystem::BroadcastEntityPerceived(FMassEntityHandle Perceiver, FMassEntityHandle Perceived, FGameplayTag SenseTag)
{
	if (FArcPerceptionEntityAddedNative* Delegate = OnEntityPerceived.Find(Perceiver))
	{
		Delegate->Broadcast(Perceiver, Perceived, SenseTag);
	}
}

void UArcMassHearingPerceptionSubsystem::BroadcastEntityLostFromPerception(FMassEntityHandle Perceiver, FMassEntityHandle Perceived, FGameplayTag SenseTag)
{
	if (FArcPerceptionEntityAddedNative* Delegate = OnEntityLostFromPerception.Find(Perceiver))
	{
		Delegate->Broadcast(Perceiver, Perceived, SenseTag);
	}
}

UArcMassHearingPerceptionProcessor::UArcMassHearingPerceptionProcessor()
    : HearingQuery(*this)
{
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
    ExecutionOrder.ExecuteAfter.Add(TEXT("ArcMassSpatialHashUpdateProcessor"));
}

//----------------------------------------------------------------------
// UArcMassHearingPerceptionProcessor
//----------------------------------------------------------------------

void UArcMassHearingPerceptionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    HearingQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    HearingQuery.AddRequirement<FArcMassHearingPerceptionResult>(EMassFragmentAccess::ReadWrite);
	HearingQuery.AddConstSharedRequirement<FArcPerceptionHearingSenseConfigFragment>(EMassFragmentPresence::All);
}

void UArcMassHearingPerceptionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = EntityManager.GetWorld();
    if (!World)
    {
        return;
    }

    UArcMassSpatialHashSubsystem* SpatialHash = World->GetSubsystem<UArcMassSpatialHashSubsystem>();
    UArcMassHearingPerceptionSubsystem* PerceptionSubsystem = World->GetSubsystem<UArcMassHearingPerceptionSubsystem>();

    if (!SpatialHash || !PerceptionSubsystem)
    {
        return;
    }

    const float DeltaTime = Context.GetDeltaTimeSeconds();

    HearingQuery.ForEachEntityChunk(Context,
        [this, &EntityManager, SpatialHash, PerceptionSubsystem, DeltaTime](FMassExecutionContext& Ctx)
        {
        	const FArcPerceptionHearingSenseConfigFragment& Config = Ctx.GetConstSharedFragment<FArcPerceptionHearingSenseConfigFragment>();

            const TConstArrayView<FTransformFragment> TransformList = Ctx.GetFragmentView<FTransformFragment>();
            TArrayView<FArcMassHearingPerceptionResult> ResultList = Ctx.GetMutableFragmentView<FArcMassHearingPerceptionResult>();

            ProcessPerceptionChunk(EntityManager, Ctx, SpatialHash
            	, PerceptionSubsystem, DeltaTime, TransformList
            	, Config, ResultList);
        });
}

void UArcMassHearingPerceptionProcessor::ProcessPerceptionChunk(
    FMassEntityManager& EntityManager,
    FMassExecutionContext& Context,
    UArcMassSpatialHashSubsystem* SpatialHash,
    UArcMassHearingPerceptionSubsystem* PerceptionSubsystem,
    float DeltaTime,
    const TConstArrayView<FTransformFragment>& TransformList,
    const FArcPerceptionHearingSenseConfigFragment& Config,
    TArrayView<FArcMassHearingPerceptionResult> ResultList)
{
    const FMassSpatialHashGrid& Grid = SpatialHash->GetSpatialHashGrid();

	const double CurrentWorldTime = EntityManager.GetWorld()->GetTimeSeconds();

    for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
    {
        const FTransformFragment& Transform = TransformList[EntityIt];
        FArcMassHearingPerceptionResult& Result = ResultList[EntityIt];

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

        // Remove entities past ForgetTime
        TSet<FMassEntityHandle> NewlyPerceivedSet;
        TArray<FArcPerceivedEntity> NewPerceivedList;
    	TMap<FMassEntityHandle, int32> CurrentEntities;
    	TArray<FMassEntityHandle> RemovedEntities;

    	CurrentEntities.Reserve(Result.PerceivedEntities.Num());
    	for (int32 Idx = Result.PerceivedEntities.Num() - 1; Idx >= 0; --Idx)
    	{
    		const float TimeSinceLastSeen = CurrentWorldTime - Result.PerceivedEntities[Idx].LastTimeSeen;
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

        for (const FArcMassEntityInfo& QueriedEntityInfo : QueriedEntities)
        {
            const FMassEntityHandle QueriedEntity = QueriedEntityInfo.Entity;
            const float Distance = QueriedEntityInfo.Distance;

            if (QueriedEntity == Entity)
            {
                continue;
            }

            if (!EntityManager.IsEntityValid(QueriedEntity))
            {
                continue;
            }

            if (!EntityManager.DoesEntityHaveElement(QueriedEntity, FArcMassHearingPerceivableTag::StaticStruct()))
            {
                continue;
            }

        	if (!EntityManager.DoesEntityHaveElement(QueriedEntity, FArcMassPerceivableStimuliHearingFragment::StaticStruct()))
        	{
        		continue;
        	}

            if (!ArcPerception::PassesFilters(EntityManager, QueriedEntity, Config))
            {
                continue;
            }

        	FArcMassPerceivableStimuliHearingFragment& HearingStimuli = EntityManager.GetFragmentDataChecked<FArcMassPerceivableStimuliHearingFragment>(QueriedEntity);
        	const float TimeHeard = CurrentWorldTime - HearingStimuli.EmissionTime;
        	if (Config.SoundMaxAge > 0.0f && TimeHeard > Config.SoundMaxAge)
        	{
        		continue;
        	}

        	const float SafeRadius = FMath::Max(Config.Radius, 1.0f);
        	const float DistanceAlpha = FMath::Clamp(1.0f - (Distance / SafeRadius), 0.0f, 1.0f);
        	const float DistanceAttenuation = FMath::Pow(DistanceAlpha, Config.DistanceFalloffExponent);

        	FVector TargetLocation = QueriedEntityInfo.Location;

        	const FVector ToSound = (TargetLocation - Location).GetSafeNormal();
        	const float DirectionDot = FVector::DotProduct(Forward, ToSound);
        	const float DirectionAlpha = FMath::Clamp((DirectionDot + 1.0f) * 0.5f, 0.0f, 1.0f);
        	const float DirectionMultiplier = FMath::Lerp(Config.BackHearingMultiplier, Config.FrontHearingMultiplier, DirectionAlpha);

        	float AgeAlpha = 1.0f;
        	if (Config.SoundMaxAge > 0.0f)
        	{
        		AgeAlpha = FMath::Clamp(1.0f - (TimeHeard / Config.SoundMaxAge), 0.0f, 1.0f);
        	}
        	if (Config.SoundDecayRate > 0.0f)
        	{
        		AgeAlpha *= FMath::Exp(-Config.SoundDecayRate * TimeHeard);
        	}

        	const float FinalStrength = HearingStimuli.EmissionStrength * DistanceAttenuation * DirectionMultiplier * AgeAlpha;
        	if (FinalStrength < Config.MinAudibleStrength)
        	{
        		continue;
        	}

        	FVector PerceivedLocation = TargetLocation;
        	if (Config.bApproximateSoundLocation)
        	{
        		const float StrengthNormalized = (Config.MaxHearingStrength > 0.0f)
        			? FMath::Clamp(FinalStrength / Config.MaxHearingStrength, 0.0f, 1.0f)
        			: FMath::Clamp(FinalStrength, 0.0f, 1.0f);

        		const float AngleErrorAlpha = 1.0f - DirectionAlpha;
        		float ErrorRadius = Config.BaseLocationError + (Config.DistanceErrorScale * Distance);
        		ErrorRadius *= (1.0f + (Config.AngleErrorScale * AngleErrorAlpha));
        		ErrorRadius *= (1.0f + (Config.StrengthErrorScale * (1.0f - StrengthNormalized)));

        		if (ErrorRadius > 0.0f)
        		{
        			PerceivedLocation += FMath::VRand() * ErrorRadius;
        		}
        	}

        	// Update existing or add new
        	if (int32* ExistingIdx = CurrentEntities.Find(QueriedEntity))
        	{
        		Result.PerceivedEntities[*ExistingIdx].LastTimeSeen = CurrentWorldTime;
        		Result.PerceivedEntities[*ExistingIdx].TimeSinceLastPerceived = 0.0f;
        		Result.PerceivedEntities[*ExistingIdx].Distance = Distance;
        		Result.PerceivedEntities[*ExistingIdx].Strength = FinalStrength;
        		Result.PerceivedEntities[*ExistingIdx].TimePerceived += DeltaTime;
        		Result.PerceivedEntities[*ExistingIdx].LastKnownLocation = PerceivedLocation;
        		continue;
        	}

            NewlyPerceivedSet.Add(QueriedEntity);

            FArcPerceivedEntity PerceivedData;
            PerceivedData.Entity = QueriedEntity;
            PerceivedData.Distance = Distance;
            PerceivedData.TimeSinceLastPerceived = 0.0f;
        	PerceivedData.Strength = FinalStrength;
        	PerceivedData.LastTimeSeen = CurrentWorldTime;
        	PerceivedData.TimeFirstPerceived = CurrentWorldTime;
        	PerceivedData.TimePerceived = 0.f;
            PerceivedData.LastKnownLocation = PerceivedLocation;
            NewPerceivedList.Add(PerceivedData);
        }

    	// Broadcast events
    	for (const FMassEntityHandle& NewEntity : NewlyPerceivedSet)
    	{
    		PerceptionSubsystem->BroadcastEntityPerceived(Entity, NewEntity, TAG_AI_Perception_Sense_Hearing);
    	}

    	for (const FMassEntityHandle& OldEntity : RemovedEntities)
    	{
    		if (!NewlyPerceivedSet.Contains(OldEntity))
    		{
    			PerceptionSubsystem->BroadcastEntityLostFromPerception(Entity, OldEntity, TAG_AI_Perception_Sense_Hearing);
    		}
    	}

    	// Append newly perceived entities to the result
    	Result.PerceivedEntities.Append(NewPerceivedList);

    	const bool bChanged = NewPerceivedList.Num() > 0 || RemovedEntities.Num() > 0;
    	if (bChanged)
    	{
    		if (FArcPerceptionEntityList* Delegate = PerceptionSubsystem->OnPerceptionUpdated.Find(Entity))
    		{
    			Delegate->Broadcast(Entity, Result.PerceivedEntities, TAG_AI_Perception_Sense_Hearing);
    		}
    	}
    }
}

//----------------------------------------------------------------------
// UArcMassHearingPerceptionObserver
//----------------------------------------------------------------------

UArcMassHearingPerceptionObserver::UArcMassHearingPerceptionObserver()
    : ObserverQuery(*this)
{
    ObservedType = FArcMassHearingPerceivableTag::StaticStruct();
    ObservedOperations = EMassObservedOperationFlags::Remove;
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
}

void UArcMassHearingPerceptionObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcMassHearingPerceptionObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
   	UWorld* World = EntityManager.GetWorld();
   	if (!World)
   	{
   	    return;
   	}

    UArcMassHearingPerceptionSubsystem* PerceptionSubsystem = World->GetSubsystem<UArcMassHearingPerceptionSubsystem>();
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
