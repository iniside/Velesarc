// Copyright Lukasz Baran. All Rights Reserved.

#include "QuickBar/ArcMassQuickBarAutoSelectProcessor.h"
#include "QuickBar/ArcMassQuickBarTypes.h"
#include "QuickBar/ArcMassQuickBarConfig.h"
#include "QuickBar/ArcMassQuickBarSharedFragment.h"
#include "QuickBar/ArcMassQuickBarOperations.h"
#include "QuickBar/ArcMassQuickBarStateFragment.h"
#include "QuickBar/ArcMassQuickBarInternal.h"

#include "Items/ArcItemData.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Fragments/ArcMassItemEventFragment.h"
#include "Signals/ArcMassItemSignals.h"

#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "MassSignalSubsystem.h"
#include "Mass/EntityFragments.h"

UArcMassQuickBarAutoSelectProcessor::UArcMassQuickBarAutoSelectProcessor()
{
	bRequiresGameThreadExecution = true;
}

void UArcMassQuickBarAutoSelectProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	if (const UWorld* World = GetWorld())
	{
		if (UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>())
		{
			const FName SignalName = UE::ArcMassItems::Signals::GetSignalName(
				UE::ArcMassItems::Signals::ItemSlotChanged,
				FArcMassItemStoreFragment::StaticStruct());
			SubscribeToSignal(*SignalSubsystem, SignalName);
		}
	}
}

void UArcMassQuickBarAutoSelectProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcMassQuickBarSharedFragment>(EMassFragmentPresence::All);
}

void UArcMassQuickBarAutoSelectProcessor::SignalEntities(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context,
	FMassSignalNameLookup& EntitySignals)
{
	EntityQuery.ForEachEntityChunk(Context, [&EntityManager](FMassExecutionContext& Ctx)
	{
		const FArcMassQuickBarSharedFragment& Shared = Ctx.GetConstSharedFragment<FArcMassQuickBarSharedFragment>();
		if (!Shared.Config)
		{
			return;
		}

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

			FMassEntityView View(EntityManager, Entity);
			const FArcMassItemEventFragment* EventFrag = View.GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
			if (!EventFrag)
			{
				continue;
			}

			for (const FArcMassItemEvent& Event : EventFrag->Events)
			{
				if (Event.Type != EArcMassItemEventType::SlotChanged)
				{
					continue;
				}

				const UScriptStruct* StoreType = Event.StoreType.Get();
				if (!StoreType)
				{
					continue;
				}

				if (Event.SlotTag.IsValid())
				{
					for (const FArcMassQuickBarEntry& BarEntry : Shared.Config->QuickBars)
					{
						for (const FArcMassQuickBarSlot& QSlot : BarEntry.Slots)
						{
							if (QSlot.ItemSlotId != Event.SlotTag)
							{
								continue;
							}

							const FArcItemData* ItemData = ArcMassItems::QuickBar::Internal::GetItemFromStore(
								EntityManager, Entity, StoreType, Event.ItemId);

							if (!ArcMassItems::QuickBar::Internal::ItemMatchesRequiredTags(ItemData, BarEntry.ItemRequiredTags) ||
								!ArcMassItems::QuickBar::Internal::ItemMatchesRequiredTags(ItemData, QSlot.ItemRequiredTags))
							{
								continue;
							}

							ArcMassItems::QuickBar::AddAndActivateSlot(
								EntityManager, Entity, StoreType,
								BarEntry.BarId, QSlot.QuickBarSlotId, Event.ItemId);
						}
					}
				}
				else
				{
					const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
					if (!State)
					{
						continue;
					}

					TArray<TPair<FGameplayTag, FGameplayTag>, TInlineAllocator<4>> SlotsToRemove;
					for (const FArcMassQuickBarActiveSlot& ActiveSlot : State->ActiveSlots)
					{
						if (ActiveSlot.AssignedItemId == Event.ItemId)
						{
							SlotsToRemove.Emplace(ActiveSlot.BarId, ActiveSlot.QuickSlotId);
						}
					}
					for (const TPair<FGameplayTag, FGameplayTag>& Pair : SlotsToRemove)
					{
						ArcMassItems::QuickBar::RemoveSlot(
							EntityManager, Entity, StoreType,
							Pair.Key, Pair.Value);
					}
				}
			}
		}
	});
}
