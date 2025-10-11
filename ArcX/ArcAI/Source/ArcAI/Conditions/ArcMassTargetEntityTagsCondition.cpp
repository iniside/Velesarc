#include "ArcMassTargetEntityTagsCondition.h"

#include "MassStateTreeExecutionContext.h"
#include "StateTreeNodeDescriptionHelpers.h"
#include "ArcMass/ArcMassGameplayTagContainerFragment.h"

bool FArcMassTargetEntityTagsCondition::Link(FStateTreeLinker& Linker)
{	
	return true;
}

void FArcMassTargetEntityTagsCondition::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
}

bool FArcMassTargetEntityTagsCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassEntityManager& EM = MassStateTreeContext.GetEntityManager();
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.TargetInput.GetEntityHandle().IsValid() == false)
	{
		return false;
	}
	
	FArcMassGameplayTagContainerFragment* MassTags = EM.GetFragmentDataPtr<FArcMassGameplayTagContainerFragment>(InstanceData.TargetInput.GetEntityHandle());
	if (!MassTags)
	{
		return false;
	}

	bool bResult = false;
	switch (MatchType)
	{
		case EGameplayContainerMatchType::Any:
			bResult = bExactMatch ? MassTags->Tags.HasAnyExact(InstanceData.TagContainer) : MassTags->Tags.HasAny(InstanceData.TagContainer);
			break;
		case EGameplayContainerMatchType::All:
			bResult = bExactMatch ? MassTags->Tags.HasAllExact(InstanceData.TagContainer) : MassTags->Tags.HasAll(InstanceData.TagContainer);
			break;
		default:
			ensureMsgf(false, TEXT("Unhandled match type %s."), *UEnum::GetValueAsString(MatchType));
	}

	SET_NODE_CUSTOM_TRACE_TEXT(Context, Override, TEXT("%s'%s' contains '%s %s%s'")
		, *UE::StateTree::DescHelpers::GetInvertText(bInvert, EStateTreeNodeFormatting::Text).ToString()
		, *UE::StateTree::DescHelpers::GetGameplayTagContainerAsText(InstanceData.TagContainer).ToString()
		, *UEnum::GetValueAsString(MatchType)
		, *UE::StateTree::DescHelpers::GetExactMatchText(bExactMatch, EStateTreeNodeFormatting::Text).ToString()
		, *UE::StateTree::DescHelpers::GetGameplayTagContainerAsText(MassTags->Tags).ToString());

	return bResult ^ bInvert;
}