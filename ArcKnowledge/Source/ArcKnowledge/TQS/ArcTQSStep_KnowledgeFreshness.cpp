// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_KnowledgeFreshness.h"
#include "ArcTQSKnowledgeHelpers.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "Engine/World.h"

float FArcTQSStep_KnowledgeFreshness::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
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
	return FMath::Pow(0.5f, Age / HalfLifeSeconds);
}
