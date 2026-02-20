// Copyright Lukasz Baran. All Rights Reserved.

#include "GameplayDebuggerCategory_ArcNeeds.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "ArcNeedsFragment.h"
#include "GameplayDebuggerCategoryReplicator.h"
#include "MassActorSubsystem.h"
#include "MassAgentComponent.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassGameplayDebugTypes.h"
#include "Engine/World.h"

namespace ArcNeedsDebug
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
} // namespace ArcNeedsDebug

FGameplayDebuggerCategory_ArcNeeds::FGameplayDebuggerCategory_ArcNeeds()
	: Super()
{
	bPickEntity = false;

	BindKeyPress(EKeys::N.GetFName(), FGameplayDebuggerInputModifier::Shift, this,
		&FGameplayDebuggerCategory_ArcNeeds::OnPickEntity, EGameplayDebuggerInputMode::Replicated);

	OnEntitySelectedHandle = FMassDebugger::OnEntitySelectedDelegate.AddRaw(
		this, &FGameplayDebuggerCategory_ArcNeeds::OnEntitySelected);
}

FGameplayDebuggerCategory_ArcNeeds::~FGameplayDebuggerCategory_ArcNeeds()
{
	FMassDebugger::OnEntitySelectedDelegate.Remove(OnEntitySelectedHandle);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_ArcNeeds::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_ArcNeeds());
}

void FGameplayDebuggerCategory_ArcNeeds::OnPickEntity()
{
	bPickEntity = true;
}

void FGameplayDebuggerCategory_ArcNeeds::OnEntitySelected(
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

void FGameplayDebuggerCategory_ArcNeeds::SetCachedEntity(
	FMassEntityHandle Entity, const FMassEntityManager& EntityManager)
{
	CachedEntity = Entity;
}

void FGameplayDebuggerCategory_ArcNeeds::PickEntity(
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
		BestEntity = ArcNeedsDebug::GetBestEntity(ViewLocation, ViewDirection, Entities, Locations, bLimitAngle, SearchRange);
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

		BestEntity = ArcNeedsDebug::GetBestEntity(ViewLocation, ViewDirection, Entities, Locations, bLimitAngle, SearchRange);
	}

	SetCachedEntity(BestEntity, EntityManager);
}

void FGameplayDebuggerCategory_ArcNeeds::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	const UWorld* World = GetDataWorld(OwnerPC, DebugActor);
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	if (!EntitySubsystem)
	{
		AddTextLine(FString::Printf(TEXT("{red}MassEntitySubsystem not available")));
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	if (DebugActor)
	{
		const FMassEntityHandle EntityHandle = ArcNeedsDebug::GetEntityFromActor(*DebugActor);
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

	AddTextLine(FString::Printf(TEXT("{white}Arc Needs Debug  |  {cyan}Shift+N{white} to pick entity")));

	if (!CachedEntity.IsSet())
	{
		AddTextLine(FString::Printf(TEXT("{grey}No entity selected.")));
		return;
	}

	if (!EntityManager.IsEntityValid(CachedEntity))
	{
		AddTextLine(FString::Printf(TEXT("{red}Entity is no longer valid.")));
		CachedEntity = FMassEntityHandle();
		return;
	}

	AddTextLine(FString::Printf(TEXT("{white}Entity: {yellow}%s"), *CachedEntity.DebugGetDescription()));

	// --- FArcNeedsFragment (array-based needs) ---
	if (const FArcNeedsFragment* NeedsFragment = EntityManager.GetFragmentDataPtr<FArcNeedsFragment>(CachedEntity))
	{
		AddTextLine(FString::Printf(TEXT("{cyan}--- Needs (FArcNeedsFragment) --- {white}[%d items]"), NeedsFragment->Needs.Num()));
		for (const FArcNeedItem& Need : NeedsFragment->Needs)
		{
			AddTextLine(FString::Printf(TEXT("  {yellow}%s{white}  Value: {green}%.1f{white}  Rate: %.2f"),
				*Need.NeedName.ToString(), Need.CurrentValue, Need.ChangeRate));
		}
	}

	// --- FArcHungerNeedFragment ---
	if (const FArcHungerNeedFragment* HungerFragment = EntityManager.GetFragmentDataPtr<FArcHungerNeedFragment>(CachedEntity))
	{
		AddTextLine(FString::Printf(TEXT("{cyan}--- Hunger ---")));
		AddTextLine(FString::Printf(TEXT("  {white}Value: {green}%.1f{white}  Rate: %.2f  Resistance: %.2f  Type: %d"),
			HungerFragment->CurrentValue, HungerFragment->ChangeRate, HungerFragment->Resistance, HungerFragment->NeedType));
	}

	// --- FArcThirstNeedFragment ---
	if (const FArcThirstNeedFragment* ThirstFragment = EntityManager.GetFragmentDataPtr<FArcThirstNeedFragment>(CachedEntity))
	{
		AddTextLine(FString::Printf(TEXT("{cyan}--- Thirst ---")));
		AddTextLine(FString::Printf(TEXT("  {white}Value: {green}%.1f{white}  Rate: %.2f  Resistance: %.2f  Type: %d"),
			ThirstFragment->CurrentValue, ThirstFragment->ChangeRate, ThirstFragment->Resistance, ThirstFragment->NeedType));
	}

	// --- FArcFatigueNeedFragment ---
	if (const FArcFatigueNeedFragment* FatigueFragment = EntityManager.GetFragmentDataPtr<FArcFatigueNeedFragment>(CachedEntity))
	{
		AddTextLine(FString::Printf(TEXT("{cyan}--- Fatigue ---")));
		AddTextLine(FString::Printf(TEXT("  {white}Value: {green}%.1f{white}  Rate: %.2f  Resistance: %.2f  Type: %d"),
			FatigueFragment->CurrentValue, FatigueFragment->ChangeRate, FatigueFragment->Resistance, FatigueFragment->NeedType));
	}

	// --- FArcResourceFragment ---
	if (const FArcResourceFragment* ResourceFragment = EntityManager.GetFragmentDataPtr<FArcResourceFragment>(CachedEntity))
	{
		AddTextLine(FString::Printf(TEXT("{cyan}--- Resource ---")));
		AddTextLine(FString::Printf(TEXT("  {white}Amount: {green}%.1f"), ResourceFragment->CurrentResourceAmount));
	}

	// --- FArcNeedStateTreeFragment ---
	if (const FArcNeedStateTreeFragment* NeedSTFragment = EntityManager.GetFragmentDataPtr<FArcNeedStateTreeFragment>(CachedEntity))
	{
		AddTextLine(FString::Printf(TEXT("{cyan}--- Need StateTree ---")));
		AddTextLine(FString::Printf(TEXT("  {white}StateTree: {yellow}%s"),
			NeedSTFragment->StateTree ? *NeedSTFragment->StateTree->GetName() : TEXT("None")));
		AddTextLine(FString::Printf(TEXT("  {white}OwnerNeed: {yellow}%s"),
			NeedSTFragment->OwnerNeed.IsValid() ? *NeedSTFragment->OwnerNeed.GetScriptStruct()->GetName() : TEXT("None")));
	}
}

void FGameplayDebuggerCategory_ArcNeeds::DrawData(
	APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	FGameplayDebuggerCategory::DrawData(OwnerPC, CanvasContext);
}

#endif // WITH_GAMEPLAY_DEBUGGER
