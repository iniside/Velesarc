// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaPopulationSpawner.h"
#include "ArcAreaPopulationTypes.h"
#include "ArcAreaSubsystem.h"
#include "ArcAreaTypes.h"
#include "ArcAreaSlotDefinition.h"
#include "MassSpawnerSubsystem.h"
#include "MassEntityFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassCommands.h"
#include "TargetQuery/ArcTQSQuerySubsystem.h"
#include "Components/SceneComponent.h"
#include "Components/BillboardComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "Engine/World.h"

AArcAreaPopulationSpawner::AArcAreaPopulationSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
	RootComponent->Mobility = EComponentMobility::Static;

#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;

	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (SpriteComponent)
	{
		static ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteFinder(TEXT("/Engine/EditorResources/S_NavP"));
		SpriteComponent->Sprite = SpriteFinder.Get();
		SpriteComponent->SetRelativeScale3D(FVector(0.5f));
		SpriteComponent->bHiddenInGame = true;
		SpriteComponent->SetupAttachment(RootComponent);
	}
#endif
}

void AArcAreaPopulationSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (!PopulationDefinition)
	{
		return;
	}

	UArcAreaSubsystem* AreaSub = GetWorld()->GetSubsystem<UArcAreaSubsystem>();
	if (!AreaSub)
	{
		return;
	}

	SlotStateChangedHandle = AreaSub->AddOnSlotStateChanged(
		FArcAreaSlotStateChanged::FDelegate::CreateUObject(this, &AArcAreaPopulationSpawner::OnSlotStateChanged));

	// Delayed initial scan: areas may not have registered yet on frame 0.
	GetWorldTimerManager().SetTimer(InitialScanTimerHandle, [this]()
	{
		UArcAreaSubsystem* AreaSubInner = GetWorld() ? GetWorld()->GetSubsystem<UArcAreaSubsystem>() : nullptr;
		if (!AreaSubInner)
		{
			return;
		}

		// Scan all currently registered areas for vacant slots
		for (const auto& [AreaHandle, AreaData] : AreaSubInner->GetAllAreas())
		{
			const TArray<FArcAreaSlotHandle> VacantSlots = AreaSubInner->GetVacantSlots(AreaHandle);
			for (const FArcAreaSlotHandle& SlotHandle : VacantSlots)
			{
				SpawnForSlot(SlotHandle);
			}
		}
	}, InitialScanDelay, false);
}

void AArcAreaPopulationSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(InitialScanTimerHandle);

	if (UArcTQSQuerySubsystem* TQSSub = GetWorld()->GetSubsystem<UArcTQSQuerySubsystem>())
	{
		for (const int32 QueryId : ActiveQueryIds)
		{
			TQSSub->AbortQuery(QueryId);
		}
	}
	ActiveQueryIds.Empty();

	if (UArcAreaSubsystem* AreaSub = GetWorld()->GetSubsystem<UArcAreaSubsystem>())
	{
		AreaSub->RemoveOnSlotStateChanged(SlotStateChangedHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AArcAreaPopulationSpawner::OnSlotStateChanged(const FArcAreaSlotHandle& SlotHandle, EArcAreaSlotState NewState)
{
	if (NewState == EArcAreaSlotState::Vacant)
	{
		SpawnForSlot(SlotHandle);
	}
}

void AArcAreaPopulationSpawner::SpawnForSlot(const FArcAreaSlotHandle& SlotHandle)
{
	UArcAreaSubsystem* AreaSub = GetWorld()->GetSubsystem<UArcAreaSubsystem>();
	if (!AreaSub || !PopulationDefinition)
	{
		return;
	}

	const FArcAreaData* AreaData = AreaSub->GetAreaData(SlotHandle.AreaHandle);
	if (!AreaData)
	{
		return;
	}

	// Get slot definition for requirements
	if (!AreaData->SlotDefinitions.IsValidIndex(SlotHandle.SlotIndex))
	{
		return;
	}
	const FArcAreaSlotDefinition& SlotDef = AreaData->SlotDefinitions[SlotHandle.SlotIndex];

	// Find a matching population entry by checking each entry's RoleTag against the slot's RequirementQuery
	const FArcAreaPopulationEntry* MatchingEntry = nullptr;
	for (const FArcAreaPopulationEntry& Entry : PopulationDefinition->Entries)
	{
		if (!Entry.RoleTag.IsValid())
		{
			continue;
		}

		// Check if this entry's role matches the slot's requirement query
		FGameplayTagContainer RoleTags;
		RoleTags.AddTag(Entry.RoleTag);
		if (SlotDef.RequirementQuery.IsEmpty() || SlotDef.RequirementQuery.Matches(RoleTags))
		{
			// Check MaxCount
			const int32 CurrentCount = SpawnedCounts.FindRef(Entry.RoleTag);
			if (Entry.MaxCount <= 0 || CurrentCount < Entry.MaxCount)
			{
				MatchingEntry = &Entry;
				break;
			}
		}
	}

	if (!MatchingEntry)
	{
		return;
	}

	const FGameplayTag RoleTag = MatchingEntry->RoleTag;
	const FVector AreaLocation = AreaData->Location;

	if (SpawnLocationQuery)
	{
		UArcTQSQuerySubsystem* TQSSub = GetWorld()->GetSubsystem<UArcTQSQuerySubsystem>();
		if (!TQSSub)
		{
			// No TQS subsystem available — fall back to direct spawn
			ExecuteSpawn(SlotHandle, RoleTag, AreaLocation);
			return;
		}

		FArcTQSQueryContext QueryContext;
		QueryContext.QuerierLocation = AreaLocation;
		QueryContext.QuerierForward = FVector::ForwardVector;
		QueryContext.World = GetWorld();

		TWeakObjectPtr<AArcAreaPopulationSpawner> WeakThis(this);
		const FArcAreaSlotHandle CapturedSlot = SlotHandle;

		const int32 QueryId = TQSSub->RunQuery(
			SpawnLocationQuery,
			QueryContext,
			FArcTQSQueryFinished::CreateLambda(
				[WeakThis, CapturedSlot, RoleTag](FArcTQSQueryInstance& CompletedQuery)
				{
					AArcAreaPopulationSpawner* Self = WeakThis.Get();
					if (!Self)
					{
						return;
					}

					Self->ActiveQueryIds.Remove(CompletedQuery.QueryId);

					if (CompletedQuery.Status == EArcTQSQueryStatus::Completed && !CompletedQuery.Results.IsEmpty())
					{
						Self->ExecuteSpawn(CapturedSlot, RoleTag, CompletedQuery.Results[0].Location);
					}
				})
		);

		if (QueryId != INDEX_NONE)
		{
			ActiveQueryIds.Add(QueryId);
		}
	}
	else
	{
		ExecuteSpawn(SlotHandle, RoleTag, AreaLocation);
	}
}

void AArcAreaPopulationSpawner::ExecuteSpawn(const FArcAreaSlotHandle& SlotHandle, const FGameplayTag& RoleTag, const FVector& SpawnLocation)
{
	if (!PopulationDefinition)
	{
		return;
	}

	const FArcAreaPopulationEntry* Entry = PopulationDefinition->FindEntryForRole(RoleTag);
	if (!Entry)
	{
		return;
	}

	const UMassEntityConfigAsset* EntityConfig = Entry->EntityType.GetEntityConfig();
	if (!EntityConfig)
	{
		return;
	}

	UWorld* World = GetWorld();
	UMassSpawnerSubsystem* SpawnerSub = World->GetSubsystem<UMassSpawnerSubsystem>();
	if (!SpawnerSub)
	{
		return;
	}

	FMassEntityManager& EntityManager = SpawnerSub->GetEntityManagerChecked();
	const FMassEntityTemplate& EntityTemplate = EntityConfig->GetOrCreateEntityTemplate(*World);

	if (EntityManager.IsProcessing())
	{
		// SpawnEntities is a synchronous API — defer via Mass command buffer.
		// The lambda executes after the current processing phase completes.
		TWeakObjectPtr<AArcAreaPopulationSpawner> WeakThis(this);
		EntityManager.Defer().PushCommand<FMassDeferredCreateCommand>(
			[WeakThis, SlotHandle, RoleTag, SpawnLocation](FMassEntityManager& Manager)
			{
				AArcAreaPopulationSpawner* Self = WeakThis.Get();
				if (!Self)
				{
					return;
				}
				// Re-enter through ExecuteSpawn — IsProcessing() will be false now
				Self->ExecuteSpawn(SlotHandle, RoleTag, SpawnLocation);
			});
		return;
	}

	TArray<FMassEntityHandle> SpawnedEntities;
	TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext =
		SpawnerSub->SpawnEntities(EntityTemplate, 1, SpawnedEntities);

	if (SpawnedEntities.IsEmpty())
	{
		return;
	}

	// Set spawn location
	if (FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(SpawnedEntities[0]))
	{
		FTransform SpawnTransform = FTransform::Identity;
		SpawnTransform.SetLocation(SpawnLocation);
		TransformFrag->SetTransform(SpawnTransform);
	}

	// Release creation context — observers fire (including area assignment observers)
	CreationContext.Reset();

	// Track spawned count
	SpawnedCounts.FindOrAdd(RoleTag)++;
}
