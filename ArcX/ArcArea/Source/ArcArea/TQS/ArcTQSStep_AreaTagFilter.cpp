// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_AreaTagFilter.h"
#include "ArcTQSAreaHelpers.h"
#include "ArcAreaSubsystem.h"
#include "Engine/World.h"

float FArcTQSStep_AreaTagFilter::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
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

	return (TagQuery.IsEmpty() || TagQuery.Matches(AreaData->AreaTags)) ? 1.0f : 0.0f;
}
