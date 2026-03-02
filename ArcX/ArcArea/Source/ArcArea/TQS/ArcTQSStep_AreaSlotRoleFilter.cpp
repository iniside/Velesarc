// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_AreaSlotRoleFilter.h"
#include "ArcTQSAreaHelpers.h"
#include "ArcAreaSubsystem.h"
#include "ArcAreaSlotDefinition.h"
#include "Engine/World.h"

float FArcTQSStep_AreaSlotRoleFilter::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	const FArcAreaHandle Handle = ArcTQS::Area::GetAreaHandle(Item);
	if (!Handle.IsValid())
	{
		return 0.0f;
	}

	const int32 SlotIndex = ArcTQS::Area::GetSlotIndex(Item);
	if (SlotIndex == INDEX_NONE)
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
	if (!AreaData || !AreaData->SlotDefinitions.IsValidIndex(SlotIndex))
	{
		return 0.0f;
	}

	const FGameplayTag& RoleTag = AreaData->SlotDefinitions[SlotIndex].RoleTag;
	if (!RoleTag.IsValid())
	{
		return RoleTagQuery.IsEmpty() ? 1.0f : 0.0f;
	}

	FGameplayTagContainer RoleContainer;
	RoleContainer.AddTag(RoleTag);
	return (RoleTagQuery.IsEmpty() || RoleTagQuery.Matches(RoleContainer)) ? 1.0f : 0.0f;
}
