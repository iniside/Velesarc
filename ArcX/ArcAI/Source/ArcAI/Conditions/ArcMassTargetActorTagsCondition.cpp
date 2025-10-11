#include "ArcMassTargetActorTagsCondition.h"

#include "GameplayTagAssetInterface.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeNodeDescriptionHelpers.h"

bool FArcMassTargetActorTagsCondition::Link(FStateTreeLinker& Linker)
{	
	return true;
}

void FArcMassTargetActorTagsCondition::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
}

bool FArcMassTargetActorTagsCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.TargetInput)
	{
		return false;
	}
	
	const IGameplayTagAssetInterface* GTAI = Cast<IGameplayTagAssetInterface>(InstanceData.TargetInput);
	if (!GTAI)
	{
		return false;
	}

	FGameplayTagContainer OwnedTags;
	GTAI->GetOwnedGameplayTags(OwnedTags);

	bool bResult = false;
	switch (MatchType)
	{
		case EGameplayContainerMatchType::Any:
			bResult = bExactMatch ? OwnedTags.HasAnyExact(InstanceData.TagContainer) : OwnedTags.HasAny(InstanceData.TagContainer);
			break;
		case EGameplayContainerMatchType::All:
			bResult = bExactMatch ? OwnedTags.HasAllExact(InstanceData.TagContainer) : OwnedTags.HasAll(InstanceData.TagContainer);
			break;
		default:
			ensureMsgf(false, TEXT("Unhandled match type %s."), *UEnum::GetValueAsString(MatchType));
	}

	SET_NODE_CUSTOM_TRACE_TEXT(Context, Override, TEXT("%s'%s' contains '%s %s%s'")
		, *UE::StateTree::DescHelpers::GetInvertText(bInvert, EStateTreeNodeFormatting::Text).ToString()
		, *UE::StateTree::DescHelpers::GetGameplayTagContainerAsText(InstanceData.TagContainer).ToString()
		, *UEnum::GetValueAsString(MatchType)
		, *UE::StateTree::DescHelpers::GetExactMatchText(bExactMatch, EStateTreeNodeFormatting::Text).ToString()
		, *UE::StateTree::DescHelpers::GetGameplayTagContainerAsText(OwnedTags).ToString());

	return bResult ^ bInvert;
}