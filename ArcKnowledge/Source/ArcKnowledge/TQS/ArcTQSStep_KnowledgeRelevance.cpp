// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_KnowledgeRelevance.h"
#include "ArcTQSKnowledgeHelpers.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "Engine/World.h"

float FArcTQSStep_KnowledgeRelevance::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
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

	return Entry->Relevance;
}
