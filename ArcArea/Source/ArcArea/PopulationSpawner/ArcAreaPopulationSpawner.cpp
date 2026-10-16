// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaPopulationSpawner.h"
#include "ArcAreaPopulationTypes.h"
#include "ArcAreaSubsystem.h"
#include "ArcAreaTypes.h"
#include "MassSpawnerSubsystem.h"
#include "Mass/EntityFragments.h"
#include "MassEntityConfigAsset.h"
#include "Mass/Commands.h"
#include "ArcMass/Spawner/ArcMassSpawnerSubsystem.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcTQSQuerySubsystem.h"
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
		for (const TPair<FSmartObjectHandle, FArcAreaData>& Pair : AreaSubInner->GetAllAreas())
		{
			const TArray<FSmartObjectSlotHandle> VacantSlots = AreaSubInner->GetVacantSlots(Pair.Key);
			for (const FSmartObjectSlotHandle& SlotHandle : VacantSlots)
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

void AArcAreaPopulationSpawner::OnSlotStateChanged(const FSmartObjectSlotHandle& SlotHandle, EArcAreaSlotState NewState)
{
	if (NewState == EArcAreaSlotState::Vacant)
	{
		SpawnForSlot(SlotHandle);
	}
}

void AArcAreaPopulationSpawner::SpawnForSlot(const FSmartObjectSlotHandle& SlotHandle)
{
	UArcAreaSubsystem* AreaSub = GetWorld()->GetSubsystem<UArcAreaSubsystem>();
	if (!AreaSub || !PopulationDefinition)
	{
		return;
	}

	const FArcAreaData* AreaData = AreaSub->GetAreaData(SlotHandle.GetSmartObjectHandle());
	if (!AreaData)
	{
		return;
	}

	const FArcAreaSlotRuntime* SlotRuntime = AreaData->Slots.Find(SlotHandle);
	if (!SlotRuntime)
	{
		return;
	}

	// Find a matching population entry by checking each entry's RoleTag against the slot's effective user filter
	const FArcAreaPopulationEntry* MatchingEntry = nullptr;
	for (const FArcAreaPopulationEntry& Entry : PopulationDefinition->Entries)
	{
		if (!Entry.RoleTag.IsValid())
		{
			continue;
		}

		FGameplayTagContainer RoleTags;
		RoleTags.AddTag(Entry.RoleTag);
		if (SlotRuntime->MatchesUserTags(RoleTags))
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

	if (SpawnLocationQuery.IsValid())
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
		const FSmartObjectSlotHandle CapturedSlot = SlotHandle;

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

void AArcAreaPopulationSpawner::ExecuteSpawn(const FSmartObjectSlotHandle& SlotHandle, const FGameplayTag& RoleTag, const FVector& SpawnLocation)
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

	UArcMassEntityConfigAsset* ArcCfg = Cast<UArcMassEntityConfigAsset>(const_cast<UMassEntityConfigAsset*>(EntityConfig));
	if (!ArcCfg)
	{
		UE_LOGF(LogTemp, Warning,"AArcAreaPopulationSpawner::ExecuteSpawn: EntityConfig '%ls' is not UArcMassEntityConfigAsset — skipping.",
			*GetNameSafe(EntityConfig));
		return;
	}

	UWorld* World = GetWorld();
	UArcMassSpawnerSubsystem* ArcSpawner = World ? World->GetSubsystem<UArcMassSpawnerSubsystem>() : nullptr;
	if (!ArcSpawner)
	{
		return;
	}

	const FMassEntityHandle Entity = ArcSpawner->SpawnEntity(ArcCfg, FTransform(SpawnLocation));
	if (!Entity.IsValid())
	{
		return;
	}

	// Track spawned count
	SpawnedCounts.FindOrAdd(RoleTag)++;
}
