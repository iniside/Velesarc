// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassHasClaimedAdvertisementsCondition.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcKnowledgeSubsystem.h"

bool FArcMassHasClaimedAdvertisementsCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);

	UWorld* World = MassCtx.GetWorld();
	if (!World)
	{
		return false ^ bInvert;
	}

	UArcKnowledgeSubsystem* Subsystem = World->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!Subsystem)
	{
		return false ^ bInvert;
	}

	const FMassEntityHandle Entity = MassCtx.GetEntity();
	TArray<FArcKnowledgeHandle> ClaimedHandles;
	Subsystem->GetAdvertisementsClaimedBy(Entity, ClaimedHandles);

	bool bResult = ClaimedHandles.Num() > 0;
	return bResult ^ bInvert;
}

#if WITH_EDITOR
FText FArcMassHasClaimedAdvertisementsCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return bInvert
		? NSLOCTEXT("ArcKnowledge", "HasNoClaimedAdsDesc", "Has NO Claimed Advertisements")
		: NSLOCTEXT("ArcKnowledge", "HasClaimedAdsDesc", "Has Claimed Advertisements");
}
#endif
