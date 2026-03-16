// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSGenerator_Areas.h"
#include "ArcTQSAreaHelpers.h"
#include "ArcAreaSubsystem.h"
#include "Engine/World.h"

void FArcTQSGenerator_Areas::GenerateItems(const FArcTQSQueryContext& QueryContext, TArray<FArcTQSTargetItem>& OutItems) const
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

	FInstancedPropertyBag TemplateBag = ArcTQS::Area::MakeAreaTemplate();

	const TMap<FArcAreaHandle, FArcAreaData>& AllAreas = Subsystem->GetAllAreas();
	OutItems.Reserve(OutItems.Num() + AllAreas.Num());

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

		FArcTQSTargetItem Item;
		Item.TargetType = EArcTQSTargetType::Location;
		Item.Location = AreaData.Location;

		Item.ItemData = TemplateBag;
		ArcTQS::Area::SetAreaHandle(Item.ItemData, Handle);

		OutItems.Add(MoveTemp(Item));
	}
}

void FArcTQSGenerator_Areas::GetOutputSchema(TArray<FPropertyBagPropertyDesc>& OutDescs) const
{
	OutDescs.Emplace(ArcTQS::Area::Names::AreaHandle, EPropertyBagPropertyType::Struct, FArcAreaHandle::StaticStruct());
}
