// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_KnowledgeAgeFilter.h"
#include "ArcTQSKnowledgeHelpers.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "Engine/World.h"

float FArcTQSStep_KnowledgeAgeFilter::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	const FArcKnowledgeHandle Handle = ArcTQS::Knowledge::GetHandle(Item);
	if (!Handle.IsValid())
	{
		return 0.0f;
	}

	UWorld* World = QueryContext.World.Get();
	if (!World)
	{
		return 0.0f;
	}

	const UArcKnowledgeSubsystem* Subsystem = World->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!Subsystem)
	{
		return 0.0f;
	}

	const FArcKnowledgeEntry* Entry = Subsystem->GetKnowledgeEntry(Handle);
	if (!Entry)
	{
		return 0.0f;
	}

	const float Age = static_cast<float>(World->GetTimeSeconds() - Entry->Timestamp);

	if (MinAge > 0.0f && Age < MinAge)
	{
		return 0.0f;
	}

	if (MaxAge > 0.0f && Age > MaxAge)
	{
		return 0.0f;
	}

	return 1.0f;
}
