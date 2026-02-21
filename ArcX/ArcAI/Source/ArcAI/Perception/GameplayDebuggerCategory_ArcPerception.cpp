// Copyright Lukasz Baran. All Rights Reserved.

#include "GameplayDebuggerCategory_ArcPerception.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "ArcMassSightPerception.h"
#include "ArcMassHearingPerception.h"
#include "GameplayDebuggerCategoryReplicator.h"
#include "MassActorSubsystem.h"
#include "MassAgentComponent.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassGameplayDebugTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

namespace ArcPerceptionDebug
{
	static FMassEntityHandle GetEntityFromActor(const AActor& Actor)
	{
		FMassEntityHandle EntityHandle;
		if (const UMassAgentComponent* AgentComp = Actor.FindComponentByClass<UMassAgentComponent>())
		{
			EntityHandle = AgentComp->GetEntityHandle();
		}
		else if (UMassActorSubsystem* ActorSubsystem = UWorld::GetSubsystem<UMassActorSubsystem>(Actor.GetWorld()))
		{
			EntityHandle = ActorSubsystem->GetEntityHandleFromActor(&Actor);
		}
		return EntityHandle;
	}

	static FMassEntityHandle GetBestEntity(
		const FVector ViewLocation, const FVector ViewDirection,
		TConstArrayView<FMassEntityHandle> Entities, TConstArrayView<FVector> Locations,
		bool bLimitAngle, FVector::FReal MaxScanDistance)
	{
		constexpr FVector::FReal MinViewDirDot = 0.707; // 45 degrees
		const FVector::FReal MaxScanDistanceSq = MaxScanDistance * MaxScanDistance;

		FVector::FReal BestScore = bLimitAngle ? MinViewDirDot : (-1.0 - KINDA_SMALL_NUMBER);
		FMassEntityHandle BestEntity;

		for (int32 i = 0; i < Entities.Num(); ++i)
		{
			if (!Entities[i].IsSet())
			{
				continue;
			}

			const FVector ToEntity = Locations[i] - ViewLocation;
			const FVector::FReal DistSq = ToEntity.SizeSquared();
			if (DistSq > MaxScanDistanceSq)
			{
				continue;
			}

			const FVector DirToEntity = ToEntity.GetSafeNormal();
			const FVector::FReal Dot = FVector::DotProduct(ViewDirection, DirToEntity);
			if (Dot > BestScore)
			{
				BestScore = Dot;
				BestEntity = Entities[i];
			}
		}

		return BestEntity;
	}

	static constexpr float DebugLifetime = -1.0f;
	static constexpr uint8 DepthPriority = 0;
	static constexpr float ShapeThickness = 1.5f;
	static constexpr float LineThickness = 1.0f;

	static const FColor SightShapeColor = FColor(50, 200, 50, 128);
	static const FColor HearingShapeColor = FColor(50, 100, 200, 128);
	static const FColor ActivePerceivedColor = FColor::Red;
	static const FColor FadingPerceivedColor = FColor(255, 165, 0); // Orange
} // namespace ArcPerceptionDebug

FGameplayDebuggerCategory_ArcPerception::FGameplayDebuggerCategory_ArcPerception()
	: Super()
{
	bPickEntity = false;

	BindKeyPress(EKeys::P.GetFName(), FGameplayDebuggerInputModifier::Shift, this,
		&FGameplayDebuggerCategory_ArcPerception::OnPickEntity, EGameplayDebuggerInputMode::Replicated);

	BindKeyPress(EKeys::One.GetFName(), FGameplayDebuggerInputModifier::Shift, this,
		&FGameplayDebuggerCategory_ArcPerception::OnToggleSight, EGameplayDebuggerInputMode::Local);

	BindKeyPress(EKeys::Two.GetFName(), FGameplayDebuggerInputModifier::Shift, this,
		&FGameplayDebuggerCategory_ArcPerception::OnToggleHearing, EGameplayDebuggerInputMode::Local);

	OnEntitySelectedHandle = FMassDebugger::OnEntitySelectedDelegate.AddRaw(
		this, &FGameplayDebuggerCategory_ArcPerception::OnEntitySelected);
}

FGameplayDebuggerCategory_ArcPerception::~FGameplayDebuggerCategory_ArcPerception()
{
	FMassDebugger::OnEntitySelectedDelegate.Remove(OnEntitySelectedHandle);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_ArcPerception::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_ArcPerception());
}

void FGameplayDebuggerCategory_ArcPerception::OnPickEntity()
{
	bPickEntity = true;
}

void FGameplayDebuggerCategory_ArcPerception::OnToggleSight()
{
	bShowSight = !bShowSight;
}

void FGameplayDebuggerCategory_ArcPerception::OnToggleHearing()
{
	bShowHearing = !bShowHearing;
}

void FGameplayDebuggerCategory_ArcPerception::OnEntitySelected(
	const FMassEntityManager& EntityManager, FMassEntityHandle EntityHandle)
{
	UWorld* World = EntityManager.GetWorld();
	if (World != GetWorldFromReplicator())
	{
		return;
	}

	AActor* BestActor = nullptr;
	if (EntityHandle.IsSet() && World)
	{
		if (const UMassActorSubsystem* ActorSubsystem = World->GetSubsystem<UMassActorSubsystem>())
		{
			BestActor = ActorSubsystem->GetActorFromHandle(EntityHandle);
		}
	}

	CachedEntity = EntityHandle;
	CachedDebugActor = BestActor;
	check(GetReplicator());
	GetReplicator()->SetDebugActor(BestActor);
}

void FGameplayDebuggerCategory_ArcPerception::SetCachedEntity(
	FMassEntityHandle Entity, const FMassEntityManager& EntityManager)
{
	CachedEntity = Entity;
}

void FGameplayDebuggerCategory_ArcPerception::PickEntity(
	const FVector& ViewLocation, const FVector& ViewDirection,
	const UWorld& World, FMassEntityManager& EntityManager, bool bLimitAngle)
{
	FMassEntityHandle BestEntity;
	constexpr float SearchRange = 25000.f;

	if (UE::Mass::Debug::HasDebugEntities() && !UE::Mass::Debug::IsDebuggingSingleEntity())
	{
		TArray<FMassEntityHandle> Entities;
		TArray<FVector> Locations;
		UE::Mass::Debug::GetDebugEntitiesAndLocations(EntityManager, Entities, Locations);
		BestEntity = ArcPerceptionDebug::GetBestEntity(ViewLocation, ViewDirection, Entities, Locations, bLimitAngle, SearchRange);
	}
	else
	{
		TArray<FMassEntityHandle> Entities;
		TArray<FVector> Locations;
		FMassExecutionContext ExecutionContext(EntityManager);
		FMassEntityQuery Query(EntityManager.AsShared());
		Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
		Query.ForEachEntityChunk(ExecutionContext, [&Entities, &Locations](FMassExecutionContext& Context)
		{
			Entities.Append(Context.GetEntities().GetData(), Context.GetEntities().Num());
			TConstArrayView<FTransformFragment> InLocations = Context.GetFragmentView<FTransformFragment>();
			Locations.Reserve(Locations.Num() + InLocations.Num());
			for (const FTransformFragment& TransformFragment : InLocations)
			{
				Locations.Add(TransformFragment.GetTransform().GetLocation());
			}
		});

		BestEntity = ArcPerceptionDebug::GetBestEntity(ViewLocation, ViewDirection, Entities, Locations, bLimitAngle, SearchRange);
	}

	SetCachedEntity(BestEntity, EntityManager);
}

void FGameplayDebuggerCategory_ArcPerception::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	const UWorld* World = GetDataWorld(OwnerPC, DebugActor);
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	if (!EntitySubsystem)
	{
		AddTextLine(TEXT("{red}MassEntitySubsystem not available"));
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	if (DebugActor)
	{
		const FMassEntityHandle EntityHandle = ArcPerceptionDebug::GetEntityFromActor(*DebugActor);
		SetCachedEntity(EntityHandle, EntityManager);
		CachedDebugActor = DebugActor;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;
	if (GetViewPoint(OwnerPC, ViewLocation, ViewDirection))
	{
		if (bPickEntity)
		{
			PickEntity(ViewLocation, ViewDirection, *World, EntityManager);
			bPickEntity = false;
		}
	}

	// Header
	AddTextLine(FString::Printf(TEXT("{white}Arc Perception  |  {cyan}Shift+P{white} pick  |  {cyan}Shift+1{white} sight [%s]  |  {cyan}Shift+2{white} hearing [%s]"),
		bShowSight ? TEXT("{green}ON") : TEXT("{red}OFF"),
		bShowHearing ? TEXT("{green}ON") : TEXT("{red}OFF")));

	if (!CachedEntity.IsSet())
	{
		AddTextLine(TEXT("{grey}No entity selected."));
		return;
	}

	if (!EntityManager.IsEntityValid(CachedEntity))
	{
		AddTextLine(TEXT("{red}Entity is no longer valid."));
		CachedEntity = FMassEntityHandle();
		return;
	}

	AddTextLine(FString::Printf(TEXT("{white}Entity: {yellow}%s"), *CachedEntity.DebugGetDescription()));

	if (bShowSight)
	{
		CollectSightData(EntityManager);
	}

	if (bShowHearing)
	{
		CollectHearingData(EntityManager);
	}
}

void FGameplayDebuggerCategory_ArcPerception::CollectSightData(FMassEntityManager& EntityManager)
{
	const FArcMassSightPerceptionResult* SightResult = EntityManager.GetFragmentDataPtr<FArcMassSightPerceptionResult>(CachedEntity);
	if (!SightResult)
	{
		AddTextLine(TEXT("{grey}  [Sight] No sight perception fragment"));
		return;
	}

	AddTextLine(FString::Printf(TEXT("{green}--- Sight --- {white}[%d perceived]  Update: %.2fs"),
		SightResult->PerceivedEntities.Num(), SightResult->TimeSinceLastUpdate));

	for (const FArcPerceivedEntity& PE : SightResult->PerceivedEntities)
	{
		const double WorldTime = EntityManager.GetWorld() ? EntityManager.GetWorld()->GetTimeSeconds() : 0.0;
		const float TimeSinceSeen = static_cast<float>(WorldTime - PE.LastTimeSeen);
		const bool bActive = TimeSinceSeen < 0.1f;

		AddTextLine(FString::Printf(TEXT("  %s%s{white}  Dist:{yellow}%.0f  {white}Seen:{yellow}%.1fs ago  {white}Duration:{yellow}%.1fs"),
			bActive ? TEXT("{green}") : TEXT("{yellow}"),
			*PE.Entity.DebugGetDescription(),
			PE.Distance,
			TimeSinceSeen,
			PE.TimePerceived));
	}
}

void FGameplayDebuggerCategory_ArcPerception::CollectHearingData(FMassEntityManager& EntityManager)
{
	const FArcMassHearingPerceptionResult* HearingResult = EntityManager.GetFragmentDataPtr<FArcMassHearingPerceptionResult>(CachedEntity);
	if (!HearingResult)
	{
		AddTextLine(TEXT("{grey}  [Hearing] No hearing perception fragment"));
		return;
	}

	AddTextLine(FString::Printf(TEXT("{cyan}--- Hearing --- {white}[%d perceived]  Update: %.2fs"),
		HearingResult->PerceivedEntities.Num(), HearingResult->TimeSinceLastUpdate));

	for (const FArcPerceivedEntity& PE : HearingResult->PerceivedEntities)
	{
		const double WorldTime = EntityManager.GetWorld() ? EntityManager.GetWorld()->GetTimeSeconds() : 0.0;
		const float TimeSinceSeen = static_cast<float>(WorldTime - PE.LastTimeSeen);
		const bool bActive = TimeSinceSeen < 0.1f;

		AddTextLine(FString::Printf(TEXT("  %s%s{white}  Dist:{yellow}%.0f  {white}Str:{yellow}%.2f  {white}Seen:{yellow}%.1fs ago  {white}Duration:{yellow}%.1fs"),
			bActive ? TEXT("{green}") : TEXT("{yellow}"),
			*PE.Entity.DebugGetDescription(),
			PE.Distance,
			PE.Strength,
			TimeSinceSeen,
			PE.TimePerceived));
	}
}

void FGameplayDebuggerCategory_ArcPerception::DrawData(
	APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	FGameplayDebuggerCategory::DrawData(OwnerPC, CanvasContext);

	if (!CachedEntity.IsSet())
	{
		return;
	}

	const UWorld* World = GetDataWorld(OwnerPC, nullptr);
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	if (!EntitySubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	if (!EntityManager.IsEntityValid(CachedEntity))
	{
		return;
	}

	DrawSenseRange(const_cast<UWorld*>(World), EntityManager);
	DrawPerceivedEntities(const_cast<UWorld*>(World), EntityManager);
}

void FGameplayDebuggerCategory_ArcPerception::DrawSenseRange(UWorld* World, FMassEntityManager& EntityManager)
{
	using namespace ArcPerceptionDebug;

	const FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(CachedEntity);
	if (!TransformFragment)
	{
		return;
	}

	const FTransform& EntityTransform = TransformFragment->GetTransform();
	const FVector Forward = EntityTransform.GetRotation().GetForwardVector();

	// Sight range
	if (bShowSight)
	{
		const FArcPerceptionSightSenseConfigFragment* SightConfig = EntityManager.GetConstSharedFragmentDataPtr<FArcPerceptionSightSenseConfigFragment>(CachedEntity);
		if (SightConfig)
		{
			FVector Location = EntityTransform.GetLocation();
			Location.Z += SightConfig->EyeOffset;

			if (SightConfig->ShapeType == EArcPerceptionShapeType::Radius)
			{
				DrawDebugSphere(World, Location, SightConfig->Radius, 24, SightShapeColor, false, DebugLifetime, DepthPriority, ShapeThickness);
			}
			else
			{
				const float HalfAngleRadians = FMath::DegreesToRadians(SightConfig->ConeHalfAngleDegrees);
				DrawDebugCone(World, Location, Forward, SightConfig->ConeLength,
					HalfAngleRadians, HalfAngleRadians, 16,
					SightShapeColor, false, DebugLifetime, DepthPriority, ShapeThickness);
			}
		}
	}

	// Hearing range
	if (bShowHearing)
	{
		const FArcPerceptionHearingSenseConfigFragment* HearingConfig = EntityManager.GetConstSharedFragmentDataPtr<FArcPerceptionHearingSenseConfigFragment>(CachedEntity);
		if (HearingConfig)
		{
			FVector Location = EntityTransform.GetLocation();
			Location.Z += HearingConfig->EyeOffset;

			if (HearingConfig->ShapeType == EArcPerceptionShapeType::Radius)
			{
				DrawDebugSphere(World, Location, HearingConfig->Radius, 24, HearingShapeColor, false, DebugLifetime, DepthPriority, ShapeThickness);
			}
			else
			{
				const float HalfAngleRadians = FMath::DegreesToRadians(HearingConfig->ConeHalfAngleDegrees);
				DrawDebugCone(World, Location, Forward, HearingConfig->ConeLength,
					HalfAngleRadians, HalfAngleRadians, 16,
					HearingShapeColor, false, DebugLifetime, DepthPriority, ShapeThickness);
			}
		}
	}
}

void FGameplayDebuggerCategory_ArcPerception::DrawPerceivedEntities(UWorld* World, FMassEntityManager& EntityManager)
{
	using namespace ArcPerceptionDebug;

	const FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(CachedEntity);
	if (!TransformFragment)
	{
		return;
	}

	const FVector EntityLocation = TransformFragment->GetTransform().GetLocation();
	const double WorldTime = World->GetTimeSeconds();

	// Sight perceived entities
	if (bShowSight)
	{
		const FArcMassSightPerceptionResult* SightResult = EntityManager.GetFragmentDataPtr<FArcMassSightPerceptionResult>(CachedEntity);
		if (SightResult)
		{
			for (const FArcPerceivedEntity& PE : SightResult->PerceivedEntities)
			{
				const float TimeSinceSeen = static_cast<float>(WorldTime - PE.LastTimeSeen);
				const bool bActive = TimeSinceSeen < 0.1f;
				const FColor& LineColor = bActive ? ActivePerceivedColor : FadingPerceivedColor;
				const float SphereRadius = bActive ? 40.0f : 25.0f;

				FVector Origin = EntityLocation;
				const FArcPerceptionSightSenseConfigFragment* SightConfig = EntityManager.GetConstSharedFragmentDataPtr<FArcPerceptionSightSenseConfigFragment>(CachedEntity);
				if (SightConfig)
				{
					Origin.Z += SightConfig->EyeOffset;
				}

				DrawDebugLine(World, Origin, PE.LastKnownLocation, LineColor, false, DebugLifetime, DepthPriority, LineThickness);
				DrawDebugSphere(World, PE.LastKnownLocation, SphereRadius, 8, LineColor, false, DebugLifetime, DepthPriority, LineThickness);

				// Label
				const FString Label = FString::Printf(TEXT("S:%.0f %.1fs"), PE.Distance, TimeSinceSeen);
				DrawDebugString(World, PE.LastKnownLocation + FVector(0, 0, 50), Label, nullptr, LineColor, DebugLifetime, false, 1.0f);
			}
		}
	}

	// Hearing perceived entities
	if (bShowHearing)
	{
		const FArcMassHearingPerceptionResult* HearingResult = EntityManager.GetFragmentDataPtr<FArcMassHearingPerceptionResult>(CachedEntity);
		if (HearingResult)
		{
			for (const FArcPerceivedEntity& PE : HearingResult->PerceivedEntities)
			{
				const float TimeSinceSeen = static_cast<float>(WorldTime - PE.LastTimeSeen);
				const bool bActive = TimeSinceSeen < 0.1f;
				const FColor& LineColor = bActive ? ActivePerceivedColor : FadingPerceivedColor;
				const float SphereRadius = bActive ? 40.0f : 25.0f;

				FVector Origin = EntityLocation;
				const FArcPerceptionHearingSenseConfigFragment* HearingConfig = EntityManager.GetConstSharedFragmentDataPtr<FArcPerceptionHearingSenseConfigFragment>(CachedEntity);
				if (HearingConfig)
				{
					Origin.Z += HearingConfig->EyeOffset;
				}

				// Use dashed line style for hearing (alternate segments)
				DrawDebugLine(World, Origin, PE.LastKnownLocation, LineColor, false, DebugLifetime, DepthPriority, LineThickness);
				DrawDebugBox(World, PE.LastKnownLocation, FVector(SphereRadius * 0.5f), LineColor, false, DebugLifetime, DepthPriority, LineThickness);

				// Label with strength
				const FString Label = FString::Printf(TEXT("H:%.0f S:%.2f %.1fs"), PE.Distance, PE.Strength, TimeSinceSeen);
				DrawDebugString(World, PE.LastKnownLocation + FVector(0, 0, 50), Label, nullptr, LineColor, DebugLifetime, false, 1.0f);
			}
		}
	}
}

#endif // WITH_GAMEPLAY_DEBUGGER
