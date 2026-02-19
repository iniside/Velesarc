// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassHearingPerception.h"

#include "DrawDebugHelpers.h"
#include "MassExecutionContext.h"
#include "Engine/World.h"
#include "MassEntityFragments.h"

#include "MassEntitySubsystem.h"

#include "ArcMass/ArcMassGameplayTagContainerFragment.h"
#include "ArcMass/ArcMassSpatialHashSubsystem.h"


void UArcMassHearingPerceptionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UArcMassSpatialHashSubsystem>();
}

void UArcMassHearingPerceptionSubsystem::Deinitialize()
{
	OnEntityPerceived.Empty();
	OnEntityLostFromPerception.Empty();
	
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
	OnEntityPerceived.FindOrAdd(Perceiver).Broadcast(Perceiver, Perceived, SenseTag);
}

void UArcMassHearingPerceptionSubsystem::BroadcastEntityLostFromPerception(FMassEntityHandle Perceiver, FMassEntityHandle Perceived, FGameplayTag SenseTag)
{
	OnEntityLostFromPerception.FindOrAdd(Perceiver).Broadcast(Perceiver, Perceived, SenseTag);
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
    UArcMassHearingPerceptionSubsystem* PerceptionSubsystem = World->GetSubsystem<UArcMassHearingPerceptionSubsystem>();

    if (!SpatialHash || !PerceptionSubsystem)
    {
        return;
    }

    const float DeltaTime = Context.GetDeltaTimeSeconds();

	
    HearingQuery.ForEachEntityChunk(Context,
        [this, &EntityManager, SpatialHash, PerceptionSubsystem, DeltaTime, World](FMassExecutionContext& Ctx)
        {
        	const FArcPerceptionHearingSenseConfigFragment& Config = Ctx.GetConstSharedFragment<FArcPerceptionHearingSenseConfigFragment>();
        		
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
    UArcMassHearingPerceptionSubsystem* PerceptionSubsystem,
    float DeltaTime,
    const TConstArrayView<FTransformFragment>& TransformList,
    const FArcPerceptionHearingSenseConfigFragment& Config,
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
            PE.TimeSinceLastPerceived += DeltaTime;
        }

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
            TArray<FArcMassEntityInfo> RadiusResults;
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

        for (const FArcMassEntityInfo& QueriedEntityInfo : QueriedEntities)
        {
            const FMassEntityHandle QueriedEntity = QueriedEntityInfo.Entity;
            float Distance = QueriedEntityInfo.Distance;

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
            if (!PassesFilters(EntityManager, QueriedEntity, Config))
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

            NewlyPerceivedSet.Add(QueriedEntity);

            FArcPerceivedEntity PerceivedData;
            PerceivedData.Entity = QueriedEntity;
            PerceivedData.Distance = Distance;
            PerceivedData.TimeSinceLastPerceived = 0.0f;
        	PerceivedData.Strength = FinalStrength;

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

            PerceivedData.LastKnownLocation = PerceivedLocation;
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
                PerceptionSubsystem->BroadcastEntityPerceived(Entity, NewEntity, TAG_AI_Perception_Sense_Hearing);
            }
        }

        for (FMassEntityHandle OldEntity : PreviouslyPerceivedSet)
        {
            if (!NewlyPerceivedSet.Contains(OldEntity))
            {
                PerceptionSubsystem->BroadcastEntityLostFromPerception(Entity, OldEntity, TAG_AI_Perception_Sense_Hearing);
            }
        }

        Result.PerceivedEntities = MoveTemp(NewPerceivedList);
    	
    	if (FArcPerceptionEntityList* Delegate = PerceptionSubsystem->OnPerceptionUpdated.Find(Entity))
		{
			Delegate->Broadcast(Entity, Result.PerceivedEntities, TAG_AI_Perception_Sense_Hearing);
		}
    	
#if WITH_GAMEPLAY_DEBUGGER
        if (CVarArcDebugDrawPerception.GetValueOnAnyThread())
        {
            DrawDebugPerception(EntityManager.GetWorld(), Location, Transform.GetTransform().GetRotation(), Config, Result);
        }
#endif
    }
}


bool UArcMassHearingPerceptionProcessor::PassesFilters(
    const FMassEntityManager& EntityManager,
    FMassEntityHandle Entity,
    const FArcPerceptionSenseConfigFragment& Config)
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
void UArcMassHearingPerceptionProcessor::DrawDebugPerception(
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