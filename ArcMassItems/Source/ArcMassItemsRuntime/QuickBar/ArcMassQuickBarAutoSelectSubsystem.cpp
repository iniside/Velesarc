// Copyright Lukasz Baran. All Rights Reserved.

#include "QuickBar/ArcMassQuickBarAutoSelectSubsystem.h"

#include "QuickBar/ArcMassQuickBarAutoSelect.h"
#include "Events/ArcMassItemEventSubsystem.h"

#include "Engine/World.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"

void UArcMassQuickBarAutoSelectSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UMassEntitySubsystem>();
	Collection.InitializeDependency<UArcMassItemEventSubsystem>();

	EventSubsystem = GetWorld()->GetSubsystem<UArcMassItemEventSubsystem>();
	if (!EventSubsystem)
	{
		return;
	}

	// One subscription to the generic slot rail covers assign, clear, and move —
	// the drain fires OnItemSlotChanged for all three.
	HandleSlotChanged = EventSubsystem->OnItemSlotChanged.AddUObject(
		this, &UArcMassQuickBarAutoSelectSubsystem::OnItemSlotChanged);
}

void UArcMassQuickBarAutoSelectSubsystem::Deinitialize()
{
	if (EventSubsystem)
	{
		EventSubsystem->OnItemSlotChanged.Remove(HandleSlotChanged);
		EventSubsystem = nullptr;
	}

	EntityManager = nullptr;

	Super::Deinitialize();
}

bool UArcMassQuickBarAutoSelectSubsystem::EnsureCached()
{
	if (!EntityManager)
	{
		UMassEntitySubsystem* MassSub = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
		if (MassSub)
		{
			EntityManager = &MassSub->GetMutableEntityManager();
		}
	}
	return EntityManager != nullptr;
}

void UArcMassQuickBarAutoSelectSubsystem::OnItemSlotChanged(
	FMassEntityHandle Entity, FArcItemId ItemId, FGameplayTag NewSlot, const UScriptStruct* StoreType)
{
	if (!EnsureCached())
	{
		return;
	}

	ArcMassItems::QuickBar::AutoSelect::HandleSlotChanged(*EntityManager, Entity, ItemId, NewSlot, StoreType);
}
