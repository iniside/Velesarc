// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSGenerator_AreaVacantSlots.h"
#include "ArcTQSAreaHelpers.h"
#include "ArcAreaSubsystem.h"
#include "ArcAreaSlotDefinition.h"
#include "Engine/World.h"

void FArcTQSGenerator_AreaVacantSlots::GenerateItems(const FArcTQSQueryContext& QueryContext, TArray<FArcTQSTargetItem>& OutItems) const
{
	UWorld* World = QueryContext.World.Get();
	if (!World)
	{
		return;
	}

	const UArcAreaSubsystem* Subsystem = World->GetSubsystem<UArcAreaSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	const float MaxDistSq = MaxDistance > 0.0f ? MaxDistance * MaxDistance : 0.0f;
	const bool bFilterByRole = RoleTagFilter.IsValid();

	FInstancedPropertyBag TemplateBag = ArcTQS::Area::MakeSlotTemplate();

	const TMap<FArcAreaHandle, FArcAreaData>& AllAreas = Subsystem->GetAllAreas();

	for (const auto& [Handle, AreaData] : AllAreas)
	{
		// Tag filter
		if (!AreaTagQuery.IsEmpty() && !AreaTagQuery.Matches(AreaData.AreaTags))
		{
			continue;
		}

		// Distance filter
		if (MaxDistSq > 0.0f)
		{
			const float DistSq = FVector::DistSquared(QueryContext.QuerierLocation, AreaData.Location);
			if (DistSq > MaxDistSq)
			{
				continue;
			}
		}

		// Iterate vacant slots
		const TArray<int32> VacantIndices = Subsystem->GetVacantSlotIndices(Handle);

		for (const int32 SlotIndex : VacantIndices)
		{
			// Role filter
			if (bFilterByRole && AreaData.SlotDefinitions.IsValidIndex(SlotIndex))
			{
				if (AreaData.SlotDefinitions[SlotIndex].RoleTag != RoleTagFilter)
				{
					continue;
				}
			}

			FArcTQSTargetItem Item;
			Item.TargetType = EArcTQSTargetType::Location;
			Item.Location = AreaData.Location;

			Item.ItemData = TemplateBag;
			ArcTQS::Area::SetAreaHandle(Item.ItemData, Handle);
			ArcTQS::Area::SetSlotIndex(Item.ItemData, SlotIndex);

			OutItems.Add(MoveTemp(Item));
		}
	}
}

void FArcTQSGenerator_AreaVacantSlots::GetOutputSchema(TArray<FPropertyBagPropertyDesc>& OutDescs) const
{
	OutDescs.Emplace(ArcTQS::Area::Names::AreaHandle, EPropertyBagPropertyType::Struct, FArcAreaHandle::StaticStruct());
	OutDescs.Emplace(ArcTQS::Area::Names::SlotIndex, EPropertyBagPropertyType::Int32);
}
