// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_AreaTagFilter.h"
#include "ArcTQSAreaHelpers.h"
#include "ArcAreaSubsystem.h"
#include "Engine/World.h"

float FArcTQSStep_AreaTagFilter::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	const FSmartObjectHandle Handle = ArcTQS::Area::GetAreaHandle(Item);
	if (!Handle.IsValid())
	{
		return UnscorableRaw;
	}

	UWorld* World = QueryContext.World.Get();
	if (!World)
	{
		return UnscorableRaw;
	}

	const UArcAreaSubsystem* Subsystem = World->GetSubsystem<UArcAreaSubsystem>();
	if (!Subsystem)
	{
		return UnscorableRaw;
	}

	const FArcAreaData* AreaData = Subsystem->GetAreaData(Handle);
	if (!AreaData)
	{
		return UnscorableRaw;
	}

	return (TagQuery.IsEmpty() || TagQuery.Matches(AreaData->AreaTags)) ? 1.0f : 0.0f;
}
