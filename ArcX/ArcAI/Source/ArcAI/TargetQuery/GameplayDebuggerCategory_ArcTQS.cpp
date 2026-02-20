// Copyright Lukasz Baran. All Rights Reserved.

#include "GameplayDebuggerCategory_ArcTQS.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "ArcTQSQuerySubsystem.h"
#include "GameplayDebuggerCategoryReplicator.h"
#include "MassActorSubsystem.h"
#include "MassAgentComponent.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassGameplayDebugTypes.h"
#include "Engine/World.h"

namespace ArcTQSDebug
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

	// Lerp from red (0.0) through yellow (0.5) to green (1.0)
	static FColor ScoreToColor(float Score)
	{
		Score = FMath::Clamp(Score, 0.0f, 1.0f);
		if (Score < 0.5f)
		{
			const float T = Score * 2.0f;
			return FColor(255, static_cast<uint8>(T * 255), 0);
		}
		else
		{
			const float T = (Score - 0.5f) * 2.0f;
			return FColor(static_cast<uint8>((1.0f - T) * 255), 255, 0);
		}
	}

	static const TCHAR* SelectionModeToString(EArcTQSSelectionMode Mode)
	{
		switch (Mode)
		{
		case EArcTQSSelectionMode::HighestScore:	return TEXT("HighestScore");
		case EArcTQSSelectionMode::TopN:			return TEXT("TopN");
		case EArcTQSSelectionMode::AllPassing:		return TEXT("AllPassing");
		case EArcTQSSelectionMode::RandomWeighted:	return TEXT("RandomWeighted");
		default:									return TEXT("Unknown");
		}
	}

	static const TCHAR* StatusToString(EArcTQSQueryStatus Status)
	{
		switch (Status)
		{
		case EArcTQSQueryStatus::Completed:		return TEXT("Completed");
		case EArcTQSQueryStatus::Failed:		return TEXT("Failed");
		case EArcTQSQueryStatus::Aborted:		return TEXT("Aborted");
		default:								return TEXT("Unknown");
		}
	}
} // namespace ArcTQSDebug

FGameplayDebuggerCategory_ArcTQS::FGameplayDebuggerCategory_ArcTQS()
	: Super()
{
	bPickEntity = false;

	BindKeyPress(EKeys::T.GetFName(), FGameplayDebuggerInputModifier::Shift, this,
		&FGameplayDebuggerCategory_ArcTQS::OnPickEntity, EGameplayDebuggerInputMode::Replicated);

	OnEntitySelectedHandle = FMassDebugger::OnEntitySelectedDelegate.AddRaw(
		this, &FGameplayDebuggerCategory_ArcTQS::OnEntitySelected);
}

FGameplayDebuggerCategory_ArcTQS::~FGameplayDebuggerCategory_ArcTQS()
{
	FMassDebugger::OnEntitySelectedDelegate.Remove(OnEntitySelectedHandle);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_ArcTQS::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_ArcTQS());
}

void FGameplayDebuggerCategory_ArcTQS::OnPickEntity()
{
	bPickEntity = true;
}

void FGameplayDebuggerCategory_ArcTQS::OnEntitySelected(
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

void FGameplayDebuggerCategory_ArcTQS::SetCachedEntity(
	FMassEntityHandle Entity, const FMassEntityManager& EntityManager)
{
	CachedEntity = Entity;
}

void FGameplayDebuggerCategory_ArcTQS::PickEntity(
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
		BestEntity = ArcTQSDebug::GetBestEntity(ViewLocation, ViewDirection, Entities, Locations, bLimitAngle, SearchRange);
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

		BestEntity = ArcTQSDebug::GetBestEntity(ViewLocation, ViewDirection, Entities, Locations, bLimitAngle, SearchRange);
	}

	SetCachedEntity(BestEntity, EntityManager);
}

void FGameplayDebuggerCategory_ArcTQS::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	const UWorld* World = GetDataWorld(OwnerPC, DebugActor);
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	UArcTQSQuerySubsystem* TQSSubsystem = UWorld::GetSubsystem<UArcTQSQuerySubsystem>(World);

	if (!EntitySubsystem || !TQSSubsystem)
	{
		AddTextLine(FString::Printf(TEXT("{red}TQS Subsystem not available")));
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	// Handle entity picking from DebugActor
	if (DebugActor)
	{
		const FMassEntityHandle EntityHandle = ArcTQSDebug::GetEntityFromActor(*DebugActor);
		SetCachedEntity(EntityHandle, EntityManager);
		CachedDebugActor = DebugActor;
	}

	// Handle manual picking via Shift+T
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

	// Header info
	const auto& AllDebug = TQSSubsystem->GetAllDebugData();
	AddTextLine(FString::Printf(TEXT("{white}Arc TQS Debug  |  {yellow}%d{white} entities with query data  |  {cyan}Shift+T{white} to pick"),
		AllDebug.Num()));

	if (!CachedEntity.IsSet())
	{
		AddTextLine(FString::Printf(TEXT("{grey}No entity selected. Pick an entity with Shift+T.")));
		return;
	}

	AddTextLine(FString::Printf(TEXT("{white}Selected Entity: {yellow}%s"),
		*CachedEntity.DebugGetDescription()));

	const FArcTQSDebugQueryData* DebugData = TQSSubsystem->GetDebugData(CachedEntity);
	if (!DebugData)
	{
		AddTextLine(FString::Printf(TEXT("{grey}No query data for this entity.")));
		return;
	}

	// Query stats
	AddTextLine(FString::Printf(TEXT("{white}Status: {yellow}%s{white}  |  Mode: {yellow}%s"),
		ArcTQSDebug::StatusToString(DebugData->Status),
		ArcTQSDebug::SelectionModeToString(DebugData->SelectionMode)));

	AddTextLine(FString::Printf(TEXT("{white}Generated: {yellow}%d{white}  |  Valid: {green}%d{white}  |  Filtered: {red}%d{white}  |  Results: {cyan}%d"),
		DebugData->TotalGenerated,
		DebugData->TotalValid,
		DebugData->TotalGenerated - DebugData->TotalValid,
		DebugData->Results.Num()));

	AddTextLine(FString::Printf(TEXT("{white}Execution: {yellow}%.3f ms"),
		DebugData->ExecutionTimeMs));

	// Draw result details
	if (DebugData->Results.Num() > 0)
	{
		AddTextLine(TEXT("{cyan}--- Results ---"));
		const int32 MaxDisplay = FMath::Min(DebugData->Results.Num(), 10);
		for (int32 i = 0; i < MaxDisplay; ++i)
		{
			const FArcTQSTargetItem& Result = DebugData->Results[i];
			const TCHAR* TypeStr = TEXT("?");
			switch (Result.TargetType)
			{
			case EArcTQSTargetType::MassEntity:	TypeStr = TEXT("Entity"); break;
			case EArcTQSTargetType::Actor:		TypeStr = TEXT("Actor"); break;
			case EArcTQSTargetType::Location:	TypeStr = TEXT("Location"); break;
			case EArcTQSTargetType::Object:		TypeStr = TEXT("Object"); break;
			default: break;
			}

			AddTextLine(FString::Printf(TEXT("  {yellow}#%d{white} [%s] Score: {green}%.4f{white}  Loc: (%.0f, %.0f, %.0f)"),
				i, TypeStr, Result.Score,
				Result.Location.X, Result.Location.Y, Result.Location.Z));
		}
		if (DebugData->Results.Num() > MaxDisplay)
		{
			AddTextLine(FString::Printf(TEXT("  {grey}... and %d more"), DebugData->Results.Num() - MaxDisplay));
		}
	}

	// Draw 3D shapes for all generated items
	CollectDebugShapes(*DebugData);
}

void FGameplayDebuggerCategory_ArcTQS::CollectDebugShapes(const FArcTQSDebugQueryData& DebugData)
{
	// Build a set of result locations for quick lookup
	TSet<int32> ResultIndices;
	for (const FArcTQSTargetItem& Result : DebugData.Results)
	{
		// Find the matching item in AllItems by location (results are copies)
		for (int32 i = 0; i < DebugData.AllItems.Num(); ++i)
		{
			if (DebugData.AllItems[i].Location.Equals(Result.Location, 1.0f) &&
				DebugData.AllItems[i].TargetType == Result.TargetType)
			{
				ResultIndices.Add(i);
				break;
			}
		}
	}

	// Draw querier position
	AddShape(FGameplayDebuggerShape::MakeCylinder(
		DebugData.QuerierLocation, 30.0f, 200.0f, FColor::White, TEXT("Querier")));

	// Draw all generated items
	for (int32 i = 0; i < DebugData.AllItems.Num(); ++i)
	{
		const FArcTQSTargetItem& Item = DebugData.AllItems[i];

		if (!Item.bValid)
		{
			// Filtered-out items: small grey point
			AddShape(FGameplayDebuggerShape::MakePoint(Item.Location + FVector(0, 0, 15.0f), 8.0f, FColor(80, 80, 80)));
			continue;
		}

		const bool bIsResult = ResultIndices.Contains(i);

		if (bIsResult)
		{
			// Result items: larger sphere with score-based color + label
			const FColor Color = ArcTQSDebug::ScoreToColor(Item.Score);
			AddShape(FGameplayDebuggerShape::MakeCylinder(
				Item.Location, 25.0f, 150.0f, Color,
				FString::Printf(TEXT("%.3f"), Item.Score)));

			// Arrow from querier to result
			AddShape(FGameplayDebuggerShape::MakeArrow(
				DebugData.QuerierLocation + FVector(0, 0, 80.0f),
				Item.Location + FVector(0, 0, 80.0f),
				10.0f, 3.0f, FColor::Cyan, TEXT("")));
		}
		else
		{
			// Valid but not selected: small sphere with score color
			const FColor Color = ArcTQSDebug::ScoreToColor(Item.Score);
			AddShape(FGameplayDebuggerShape::MakePoint(
				Item.Location + FVector(0, 0, 15.0f), 12.0f, Color));
		}
	}
}

void FGameplayDebuggerCategory_ArcTQS::DrawData(
	APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	FGameplayDebuggerCategory::DrawData(OwnerPC, CanvasContext);
}

#endif // WITH_GAMEPLAY_DEBUGGER
