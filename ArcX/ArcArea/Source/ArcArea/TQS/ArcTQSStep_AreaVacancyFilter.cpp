// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_AreaVacancyFilter.h"
#include "ArcTQSAreaHelpers.h"
#include "ArcAreaSubsystem.h"
#include "ArcAreaTypes.h"
#include "Engine/World.h"

float FArcTQSStep_AreaVacancyFilter::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	const FArcAreaHandle Handle = ArcTQS::Area::GetAreaHandle(Item);
	if (!Handle.IsValid())
	{
		return 0.0f;
	}

	UWorld* World = QueryContext.World.Get();
	if (!World)
	{
		return 0.0f;
	}

	const UArcAreaSubsystem* Subsystem = World->GetSubsystem<UArcAreaSubsystem>();
	if (!Subsystem)
	{
		return 0.0f;
	}

	const FArcAreaData* AreaData = Subsystem->GetAreaData(Handle);
	if (!AreaData)
	{
		return 0.0f;
	}

	int32 VacantCount = 0;
	for (const FArcAreaSlotRuntime& Slot : AreaData->Slots)
	{
		if (Slot.State == EArcAreaSlotState::Vacant)
		{
			++VacantCount;
		}
	}

	return VacantCount >= MinVacantSlots ? 1.0f : 0.0f;
}
