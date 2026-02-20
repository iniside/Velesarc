// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSContextProvider_SmartObjects.h"

#include "SmartObjectRequestTypes.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectTypes.h"
#include "Engine/World.h"

void FArcTQSContextProvider_SmartObjects::GenerateContextLocations(
	const FArcTQSQueryContext& QueryContext,
	TArray<FVector>& OutLocations) const
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

	const FVector Center = QueryContext.QuerierLocation;
	const FVector BoxExtent(SearchRadius, SearchRadius, VerticalExtent);
	const FBox QueryBox(Center - BoxExtent, Center + BoxExtent);

	FSmartObjectRequestFilter Filter;
	Filter.ActivityRequirements = ActivityRequirements;
	Filter.bShouldIncludeClaimedSlots = bIncludeClaimedSlots;

	const FSmartObjectRequest Request(QueryBox, Filter);

	TArray<FSmartObjectRequestResult> Results;
	SmartObjectSubsystem->FindSmartObjects(Request, Results, FConstStructView());

	// Deduplicate by smart object handle (multiple slots on same object → one context location)
	TSet<FSmartObjectHandle> SeenObjects;
	OutLocations.Reserve(OutLocations.Num() + Results.Num());

	for (const FSmartObjectRequestResult& Result : Results)
	{
		bool bAlreadyInSet = false;
		SeenObjects.Add(Result.SmartObjectHandle, &bAlreadyInSet);
		if (bAlreadyInSet)
		{
			continue;
		}

		const TOptional<FVector> SlotLocation = SmartObjectSubsystem->GetSlotLocation(Result);
		if (SlotLocation.IsSet())
		{
			OutLocations.Add(SlotLocation.GetValue());
		}
	}
}
