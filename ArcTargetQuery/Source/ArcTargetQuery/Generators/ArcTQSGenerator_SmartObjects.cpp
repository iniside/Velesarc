// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSGenerator_SmartObjects.h"

#include "MassActorSubsystem.h"
#include "Mass/EntityManager.h"
#include "SmartObjectSubsystem.h"
#include "Engine/World.h"

void FArcTQSGenerator_SmartObjects::GenerateItems(
	const FArcTQSQueryContext& QueryContext,
	TArray<FArcTQSTargetItem>& OutItems) const
{
	UWorld* World = QueryContext.World.Get();
	if (!World)
	{
		return;
	}

	USmartObjectSubsystem* SmartObjectSubsystem = World->GetSubsystem<USmartObjectSubsystem>();
	if (!SmartObjectSubsystem)
	{
		return;
	}

	const FVector BoxExtent(SearchRadius, SearchRadius, VerticalExtent);
	const FBox QueryBox(Center - BoxExtent, Center + BoxExtent);

	const FSmartObjectRequest Request(QueryBox, SmartObjectRequestFilter);

	TArray<FSmartObjectRequestResult> FoundSlots;
	SmartObjectSubsystem->FindSmartObjects(Request, FoundSlots, FConstStructView());

	OutItems.Reserve(OutItems.Num() + FoundSlots.Num());
	for (const FSmartObjectRequestResult& SlotResult : FoundSlots)
	{
		if (bOnlyClaimable && !SmartObjectSubsystem->CanBeClaimed(SlotResult.SlotHandle))
		{
			continue;
		}

		const TOptional<FVector> SlotLocation = SmartObjectSubsystem->GetSlotLocation(SlotResult);
		if (!SlotLocation.IsSet())
		{
			continue;
		}

		FArcTQSTargetItem Item;
		Item.TargetType = EArcTQSTargetType::SmartObject;
		Item.Location = SlotLocation.GetValue();
		Item.SmartObjectHandle = SlotResult.SmartObjectHandle;
		Item.SlotHandle = SlotResult.SlotHandle;

		FConstStructView OwnerData = SmartObjectSubsystem->GetOwnerData(SlotResult.SmartObjectHandle);
		if (OwnerData.GetScriptStruct() == FMassEntityHandle::StaticStruct())
		{
			Item.EntityHandle = OwnerData.Get<FMassEntityHandle>();

			if (QueryContext.EntityManager)
			{
				if (FMassActorFragment* MassActorFragment = QueryContext.EntityManager->GetFragmentDataPtr<FMassActorFragment>(Item.EntityHandle))
				{
					Item.Actor = MassActorFragment->GetMutable();
				}
			}
		}

		OutItems.Add(MoveTemp(Item));
	}
}
