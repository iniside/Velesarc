// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_KnowledgePayloadTypeFilter.h"
#include "ArcTQSKnowledgeHelpers.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "Engine/World.h"

float FArcTQSStep_KnowledgePayloadTypeFilter::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	if (!RequiredPayloadType)
	{
		return 1.0f;
	}

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
	if (!Entry || !Entry->Payload.IsValid())
	{
		return 0.0f;
	}

	const UScriptStruct* PayloadStruct = Entry->Payload.GetScriptStruct();
	return (PayloadStruct && PayloadStruct->IsChildOf(RequiredPayloadType)) ? 1.0f : 0.0f;
}
