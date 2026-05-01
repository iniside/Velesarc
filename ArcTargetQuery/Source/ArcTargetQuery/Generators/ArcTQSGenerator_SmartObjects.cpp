// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSGenerator_SmartObjects.h"

#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
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

	// Deduplicate slots across multiple context locations
	TSet<FSmartObjectSlotHandle> SeenSlots;

	for (int32 ContextIdx = 0; ContextIdx < QueryContext.ContextLocations.Num(); ++ContextIdx)
	{
		const FVector& CenterLocation = QueryContext.ContextLocations[ContextIdx];
		const FVector BoxExtent(SearchRadius, SearchRadius, VerticalExtent);
		const FBox QueryBox(CenterLocation - BoxExtent, CenterLocation + BoxExtent);

		const FSmartObjectRequest Request(QueryBox, SmartObjectRequestFilter);

		TArray<FSmartObjectRequestResult> FoundSlots;
		SmartObjectSubsystem->FindSmartObjects(Request, FoundSlots, FConstStructView());

		OutItems.Reserve(OutItems.Num() + FoundSlots.Num());
		for (const FSmartObjectRequestResult& SlotResult : FoundSlots)
		{
			// Skip already-added slots (from overlapping context radii)
			// First context to find the slot "owns" it
			bool bAlreadyInSet = false;
			SeenSlots.Add(SlotResult.SlotHandle, &bAlreadyInSet);
			if (bAlreadyInSet)
			{
				continue;
			}

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
			Item.ContextIndex = ContextIdx;
			
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
}
