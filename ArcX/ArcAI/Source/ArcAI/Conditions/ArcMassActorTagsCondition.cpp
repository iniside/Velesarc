#include "ArcMassActorTagsCondition.h"

#include "GameplayTagAssetInterface.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "StateTreeNodeDescriptionHelpers.h"

bool FArcMassActorTagsCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassActorHandle);
	
	return true;
}

void FArcMassActorTagsCondition::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FMassActorFragment>();
}

bool FArcMassActorTagsCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassActorFragment* MassActor = MassStateTreeContext.GetExternalDataPtr(MassActorHandle);
	if (!MassActor)
	{
		return false;
	}

	if (!MassActor->Get())
	{
		return false;
	}

	const IGameplayTagAssetInterface* GTAI = Cast<IGameplayTagAssetInterface>(MassActor->Get());
	if (!GTAI)
	{
		return false;
	}

	FGameplayTagContainer OwnedTags;
	GTAI->GetOwnedGameplayTags(OwnedTags);

	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
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