// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_KnowledgeTagFilter.h"
#include "ArcTQSKnowledgeHelpers.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "Engine/World.h"

float FArcTQSStep_KnowledgeTagFilter::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	const FArcKnowledgeHandle Handle = ArcTQS::Knowledge::GetHandle(Item);
	if (!Handle.IsValid())
	{
		return UnscorableRaw;
	}

	UWorld* World = QueryContext.World.Get();
	if (!World)
	{
		return UnscorableRaw;
	}

	const UArcKnowledgeSubsystem* Subsystem = World->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!Subsystem)
	{
		return UnscorableRaw;
	}

	const FArcKnowledgeEntry* Entry = Subsystem->GetKnowledgeEntry(Handle);
	if (!Entry)
	{
		return UnscorableRaw;
	}

	return (TagQuery.IsEmpty() || TagQuery.Matches(Entry->Tags)) ? 1.0f : 0.0f;
}
